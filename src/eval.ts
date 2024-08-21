import { Closure, Envirnoment } from "./env";
import { Expression, VarExpression, LambdaExpression, AppExpression, DefineExpression } from "./Expression";


export function evaluate(env: Envirnoment, expression: Expression): any {
    if (expression instanceof VarExpression) {
        return env.lookup(expression);
    } else if (expression instanceof LambdaExpression) {
        return new Closure(env, expression.var, expression.body);
    } else if (expression instanceof AppExpression) {
        return apply(evaluate(env, expression.func), evaluate(env, expression.arg));
    } else {
        throw new Error("Unknown expression type");
    }
}

export function apply(clos: Closure, arg: any): any {
    if (clos instanceof Closure) {
        const extendedEnv = clos.env.extend(clos.variable, arg);
        return evaluate(extendedEnv, clos.body);
    } else {
        throw new Error("Attempted to apply a non-closure value");
    }
}

export function runProgram(env: Envirnoment, expressions: Array<Expression>): void {
    let currentEnv = env;
    for (const expr of expressions) {
        console.log(currentEnv);
        if (expr instanceof DefineExpression) {
            currentEnv = currentEnv.extend(expr.var, evaluate(env, expr.definition));
        } else {
            const evalResult = evaluate(currentEnv, expr);
            console.log("Result:", evalResult);
        }
    }
}