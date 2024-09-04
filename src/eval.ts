import { Closure, Environment } from "./env";
import { Expression, VarExpression, LambdaExpression, AppExpression, DefineExpression, SDefineExpression, prettifyToString } from "./Expression";
import { normalise } from "./reduce";


export function evaluate(env: Environment, expression: Expression): Expression | string {
    if (expression instanceof VarExpression) {
        const lookup = env.lookup(expression);
        if (lookup === undefined) {
            throw new Error(`Expected ${expression} to evaluate to a valuable expression`);
        }
        return lookup;
    } else if (expression instanceof LambdaExpression) {
        return expression; // A lambda expression is a value
    } else if (expression instanceof AppExpression) {
        // Try normalization
        normalise(expression);
        console.log("After normalization:", prettifyToString(expression.toString()));
        return apply(expression);
        // return (evaluate(env, expression.func), evaluate(env, expression.arg));
    } else {
        throw new Error("Unknown expression type");
    }
}

function apply(app: AppExpression): any {
    // We know that app is already normalized
    return app.arg;
}

function addPrime(x: string): string {
    return x + "'";
}

function freshen(used: Array<string>, x: string) {
    let new_x = x;
    while (used.indexOf(new_x) !== -1) {
        new_x = addPrime(x);
    }
    return new_x;
}