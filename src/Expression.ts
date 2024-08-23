import assert from "assert";
import { DoublyLinkedList, DoublyLinkedListNode } from "./DoubleList";

export enum Relation {
    LAMBDA_BODY = 1,
    LAMBDA_VAR,
    APP_FUNC,
    APP_ARG,
}

export interface ChildCell {
    relation: Relation;
    parent: Expression;
}

function formatChildCell(cc: ChildCell): string {
    return `ChildCell { relation: '${cc.relation}', parent: '${cc.parent.toString()}' }`;
}

export interface Expression {
    toString(): string;
    equalValue(that: Expression): boolean;
    getParents(): DoublyLinkedList<ChildCell>;
    addToParents(cclink: ChildCell | undefined): DoublyLinkedListNode<ChildCell> | undefined;
}

export class VarExpression implements Expression {
    private parents: DoublyLinkedList<ChildCell>;

    constructor(public name: string) {
        this.name = name;
        this.parents = new DoublyLinkedList<ChildCell>(null, null, formatChildCell);
    }

    public toString(): string {
        return `${this.name}`;
        // return `VarExpression { name: '${this.name}', parents: '${this.parents.toString()}'}`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof VarExpression) {
            return this.name === that.name;
        }
        return false;
    }

    public getParents(): DoublyLinkedList<ChildCell> {
        return this.parents;
    }

    public addToParents(cclink: ChildCell | undefined): DoublyLinkedListNode<ChildCell> {
        assert(cclink !== undefined);
        return this.parents.addBeginning(cclink);
    }
}

export class LambdaExpression implements Expression {
    private parents: DoublyLinkedList<ChildCell>;
    public bodyRef: DoublyLinkedListNode<ChildCell> | undefined = undefined;

    constructor(public variable: VarExpression, public body: Expression) {
        this.variable = variable;
        this.body = body;
        this.parents = new DoublyLinkedList<ChildCell>(null, null, formatChildCell);

        // Install uplinks
        this.bodyRef = this.body.addToParents({relation: Relation.LAMBDA_BODY, parent: this});
        this.variable.addToParents({relation: Relation.LAMBDA_VAR, parent: this});
    }

    public toString(): string {
        return `(\\${this.variable.toString()}. ${this.body.toString()})`;
        // return `LambdaExpression { variable: ${this.variable}, body: ${this.body}, parents: '${this.parents.toString()}' }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof LambdaExpression) {
            return this.variable.equalValue(that.variable) && this.body.equalValue(that.body);
        }
        return false;
    }

    public getParents(): DoublyLinkedList<ChildCell> {
        return this.parents;
    }

    public addToParents(cclink: ChildCell | undefined): DoublyLinkedListNode<ChildCell> {
        assert(cclink !== undefined);
        return this.parents.addBeginning(cclink);
    }
}

export class AppExpression implements Expression {
    private parents: DoublyLinkedList<ChildCell>;
    public funcRef: DoublyLinkedListNode<ChildCell> | undefined = undefined;
    public argRef: DoublyLinkedListNode<ChildCell> | undefined = undefined;
    public copy: AppExpression | undefined;

    constructor(public func: Expression, public arg: Expression) {
        this.func = func;
        this.arg = arg;
        this.parents = new DoublyLinkedList<ChildCell>(null, null, formatChildCell);

        // Install uplinks
        this.funcRef = this.func.addToParents({relation: Relation.APP_FUNC, parent: this});
        this.argRef = this.arg.addToParents({relation: Relation.APP_ARG, parent: this});
    
    }

    public toString(): string {
        return `(${this.func.toString()} ${this.arg.toString()})`;
        // return `AppExpression { func: ${this.func}, arg: ${this.arg}, parents: '${this.parents.toString()}' }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof AppExpression) {
            return this.func.equalValue(that.func) && this.arg.equalValue(that.arg);
        }
        return false;
    }

    public getParents(): DoublyLinkedList<ChildCell> {
        return this.parents;
    }

    public addToParents(cclink: ChildCell | undefined): DoublyLinkedListNode<ChildCell> {
        assert(cclink !== undefined);
        return this.parents.addBeginning(cclink);
    }
}

export class DefineExpression implements Expression {

    constructor(public variable: VarExpression, public definition: Expression) {
        this.variable = variable;
        this.definition = definition;
    }

    public toString(): string {
        return `DefineExpression { var: ${this.variable}, definition: ${this.definition} }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof DefineExpression) {
            return this.variable.equalValue(that.variable) && this.definition.equalValue(that.definition);
        }
        return false;
    }

    public getParents(): DoublyLinkedList<ChildCell> {
        return new DoublyLinkedList(null, null, formatChildCell);
    }

    public addToParents(_: ChildCell | undefined): undefined {
        // Intentionally do nothing
        return undefined;
    }
}


export function freeDeadExpr(expr: Expression) {

    function delParent(expr: Expression, cclink: DoublyLinkedListNode<ChildCell>) {
        const exprParents = expr.getParents();
        const _ = exprParents.remove(cclink);
        if (exprParents.isEmpty()) {
            free(expr);
        }
    }

    function free(expr: Expression) {
        if (expr instanceof AppExpression) {
            delParent(expr.func, expr.funcRef!);
            delParent(expr.arg, expr.argRef!);
        } else if (expr instanceof LambdaExpression) {
            delParent(expr.body, expr.bodyRef!);
        }
    }

    free(expr);
}

/**
 * Replace a child with another child.
 * 
 * @param oldpref parent list for the old term
 * @param newExpr replacement term
 */
export function replaceChild(oldpref: DoublyLinkedList<ChildCell>, newExpr: Expression): void {
    for (const cc of oldpref) {
        if (cc.relation === Relation.LAMBDA_BODY && cc.parent instanceof LambdaExpression) {
            cc.parent.body = newExpr;
        } else if (cc.relation === Relation.APP_FUNC && cc.parent instanceof AppExpression) {
            cc.parent.func = newExpr;
        } else if (cc.relation === Relation.APP_ARG && cc.parent instanceof AppExpression) {
            cc.parent.arg = newExpr;
        } 

        newExpr.addToParents(cc);
    }
}

export function newLambda(oldvar: VarExpression, body: Expression) {
    const nVar = new VarExpression(oldvar.name)
    const nlambda = new LambdaExpression(nVar, body)
    const bodyRefCell = body.addToParents({ relation: Relation.LAMBDA_BODY, parent: nlambda });
    nlambda.bodyRef = bodyRefCell;
    const varParents = oldvar.getParents()
    varParents.app(cc => upcopy(nVar, cc));
    return nlambda;
}

export function newApp(func: Expression, arg: Expression) {
    const nApp = new AppExpression(func, arg);
    nApp.funcRef = { value: { relation: Relation.APP_FUNC, parent: nApp }, next: null, prev: null };
    nApp.argRef = { value: { relation: Relation.APP_ARG, parent: nApp }, next: null, prev: null };
    return nApp;
}

export function upcopy(newChild: Expression, parRef: ChildCell) {
    if (parRef.relation === Relation.LAMBDA_BODY) {
        const lambda = parRef.parent;
        assert(lambda instanceof LambdaExpression);
        lambda.getParents().app(cc => upcopy(newLambda(lambda.variable, newChild), cc));
    } else if (parRef.relation === Relation.APP_FUNC) {
        const app = parRef.parent;
        assert(app instanceof AppExpression);
        if (app.copy === undefined) {
            const nApp = newApp(newChild, app.arg);
            app.copy = nApp;
            app.getParents().app(cc => upcopy(nApp, cc));
        } else {
            app.copy.func = newChild;
        }
    } else if (parRef.relation === Relation.APP_ARG) {
        const app = parRef.parent;
        assert(app instanceof AppExpression);

        if (app.copy === undefined) {
            const nApp = newApp(app.func, newChild);
            app.copy = nApp;
            app.getParents().app(cc => upcopy(nApp, cc));
        } else {
            app.copy.arg = newChild;
        }
    } else {
        throw new Error(`Unexpected relation type: ${parRef.relation}`);
    }
}