enum Relation {
    LAMBDA_BODY = 1,
    APP_FUNC,
    APP_ARG,
}

export interface Expression {
    toString(): string;
    equalValue(that: Expression): boolean;
}

export class VarExpression implements Expression {
    name: string;

    constructor(name: string) {
        this.name = name;
    }

    public toString(): string {
        return `VarExpression { name: '${this.name}' }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof VarExpression) {
            return this.name === that.name;
        }
        return false;
    }
}

export class LambdaExpression implements Expression {
    var: VarExpression;
    body: Expression;

    constructor(variable: VarExpression, body: Expression) {
        this.var = variable;
        this.body = body;
    }

    public toString(): string {
        return `LambdaExpression { var: ${this.var}, body: ${this.body} }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof LambdaExpression) {
            return this.var.equalValue(that.var) && this.body.equalValue(that.body);
        }
        return false;
    }
}

export class AppExpression implements Expression {
    func: Expression;
    arg: Expression;

    constructor(func: Expression, arg: Expression) {
        this.func = func;
        this.arg = arg;
    }

    public toString(): string {
        return `AppExpression { func: ${this.func}, arg: ${this.arg} }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof AppExpression) {
            return this.func.equalValue(that.func) && this.arg.equalValue(that.arg);
        }
        return false;
    }
}

export class DefineExpression implements Expression {
    var: VarExpression;
    definition: Expression;

    constructor(variable: VarExpression, definition: Expression) {
        this.var = variable;
        this.definition = definition;
    }

    public toString(): string {
        return `DefineExpression { var: ${this.var}, definition: ${this.definition} }`;
    }

    public equalValue(that: Expression): boolean {
        if (that instanceof DefineExpression) {
            return this.var.equalValue(that.var) && this.definition.equalValue(that.definition);
        }
        return false;
    }
}
