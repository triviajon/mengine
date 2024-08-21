enum Relation {
    LAMBDA_BODY = 1,
    APP_FUNC,
    APP_ARG,
}

export class VarExpression {
    name: string;

    constructor(name: string) {
        this.name = name;
    }

    public equalValue(that: Expression) {
        if (that instanceof VarExpression) {
            return this.name === that.name;
        }
        return false;
    }
}

export class LambdaExpression {
    var: VarExpression;
    body: Expression;

    constructor(variable: VarExpression, body: Expression) {
        this.var = variable;
        this.body = body;
    }
}

export class AppExpression {
    func: Expression;
    arg: Expression;

    constructor(func: Expression, arg: Expression) {
        this.func = func;
        this.arg = arg;
    }
}

export class DefineExpression {
    var: VarExpression;
    definition: Expression; 

    constructor(variable: VarExpression, definition: Expression) {
        this.var = variable;
        this.definition = definition;
    }
}

export type Expression = VarExpression | LambdaExpression | AppExpression | DefineExpression;
