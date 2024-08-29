import assert from "assert";
import { AppExpression, ChildCell, Expression, freeDeadExpr, LambdaExpression, newApp, newLambda, prettifyToString, Relation, replaceChild, upcopy, VarExpression } from "./Expression";
import { DoublyLinkedList } from "./DoubleList";

export function clearCopies(reducedLambda: LambdaExpression, topApp: AppExpression): void {
    const topcopy = topApp.copy;
    topApp.copy = undefined;

    topcopy?.arg.addToParents(topcopy.argRef?.value);
    topcopy?.func.addToParents(topcopy.funcRef?.value);

    function varClean(variable: VarExpression) {
        const parents = variable.getParents();
        parents.app(cc => cleanUp(cc.parent, cc.relation));
    }

    function cleanUp(expression: Expression, relation: Relation): void {
        if (expression instanceof LambdaExpression) {
            varClean(expression.variable);
            for (const cc of expression.getParents()) {
                cleanUp(cc.parent, cc.relation);
            }
        } else if (expression instanceof AppExpression) {
            if (expression.copy === undefined) {
                return;
            }
            const arg = expression.copy.arg;
            const argRef = expression.copy.argRef!;
            const func = expression.copy.func;
            const funcRef = expression.copy.funcRef!;
            expression.copy = undefined;

            arg.addToParents(argRef.value);
            func.addToParents(funcRef.value);
            for (const cc of expression.getParents()) {
                cleanUp(cc.parent, cc.relation);
            }
        } else {
            throw new Error(`Invalid time to cleanup, got ${typeof expression}`);
        }
    }

    function lambdaScan(reducedLambda: LambdaExpression) {
        varClean(reducedLambda.variable);
        if (reducedLambda.body instanceof LambdaExpression) {
            lambdaScan(reducedLambda.body);
        }
    }

    lambdaScan(reducedLambda);
}

function reduce(a: AppExpression) {
    assert(a.func instanceof LambdaExpression);

    const lambda: LambdaExpression = a.func;
    const variable = lambda.variable;

    function scandown(expr: Expression, argterm: Expression, varpars: DoublyLinkedList<ChildCell>): [Expression, AppExpression | null] {
        if (expr instanceof LambdaExpression) {
            const [bodyPrime, topapp] = scandown(expr.body, argterm, varpars);
            const lPrime = newLambda(expr.variable, bodyPrime);
            return [lPrime, topapp];
        } else if (expr instanceof VarExpression) {
            return [argterm, null];
        } else if (expr instanceof AppExpression) {
            const aPrime = newApp(expr.func, expr.arg);
            expr.copy = aPrime;

            varpars.app(cc => upcopy(argterm, cc));

            return [aPrime, expr];
        } else {
            throw new Error("Unexpected node type");
        }
    }

    let answer;
    console.log("Entering reduce with:", prettifyToString(a.toString()));
    console.log("Conditions:", lambda.getParents().size(), variable.getParents().size());
    if (lambda.getParents().size() === 1) {
        replaceChild(variable.getParents(), a.arg);
        answer = lambda.body;
    } else if (variable.getParents().size() === 1) { // differs from SML implementation since we add LAMBDA_VAR relation
        answer = lambda.body;
    } else {
        const [result, topApp] = scandown(lambda.body, a.arg, variable.getParents());
        answer = result;
        if (topApp !== null) {
            clearCopies(lambda, topApp);
        }
    }
    console.log("Post getting answer:", prettifyToString(a.toString()));
    replaceChild(a.getParents(), answer);
    freeDeadExpr(a);
    console.log("Answer:", prettifyToString(answer.toString()));
    return answer;
}

function normaliseWeakHead(expression: Expression): void {
    if (expression instanceof AppExpression) {
        const app = expression;
        normaliseWeakHead(app.func);
        const funcValue = app.func;
        if (funcValue instanceof LambdaExpression) {
            normaliseWeakHead(reduce(app));
        }
    }
}

export function normalise(expression: Expression): void{
    if (expression instanceof AppExpression) {
        const app = expression;
        normaliseWeakHead(app.func);
        const funcValue = app.func;

        if (funcValue instanceof LambdaExpression) {
            console.log("Found redex in:", app.toString());
            normalise(reduce(app));
        } else if (funcValue instanceof VarExpression) {
            normalise(app.arg);
        } else {
            normalise(funcValue);
            normalise(app.arg);
        }
    } else if (expression instanceof LambdaExpression) {
        normalise(expression.body);
    }
}
