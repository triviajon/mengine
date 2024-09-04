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
        normalise(expression);
        return expression; // Should be doing closures of the current envirnoment here, but too hard to implement for now
    } else if (expression instanceof AppExpression) {
        console.log("Before normalization in eval:", expression);
        normalise(expression);
        console.log("After normalization in eval:", expression);
        return apply(evaluate(env, expression.func), evaluate(env, expression.arg));
    } else {
        throw new Error("Unknown expression type");
    }
}

function apply(func: any, arg: any): any {
    if (func instanceof Closure) {
        const extendedEnv = func.env.extend(func.variable, arg);
        return evaluate(extendedEnv, func.body);
    } else if (typeof func === 'string') {
        return `(${func} ${arg})`; // Temporary solution, but this means that func is not yet a value
    } else {
        throw new Error(`Attempted to apply a non-closure value (got [${func} ${arg}]: [${typeof func} ${typeof arg}])`);
    }
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