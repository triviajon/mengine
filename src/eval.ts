import { Closure, Environment } from "./env";
import { Expression, VarExpression, LambdaExpression, AppExpression, DefineExpression, SDefineExpression, prettifyToString } from "./Expression";
import { normalise } from "./reduce";


export function evaluate(env: Environment, expression: Expression): Expression | Closure | string {
    if (expression instanceof VarExpression) {
        const lookup = env.lookup(expression);
        if (lookup === undefined) {
            throw new Error(`Expected ${expression} to evaluate to a valuable expression`);
        }
        return lookup;
    } else if (expression instanceof LambdaExpression) {
        normalise(expression)
        return new Closure(env, expression.variable, expression.body);
    } else if (expression instanceof AppExpression) {
        normalise(expression);
        return apply(evaluate(env, expression.func), evaluate(env, expression.arg));
    } else {
        throw new Error("Unknown expression type");
    }
}

function apply(clos: any, arg: any): any {
    if (clos instanceof Closure) {
        const extendedEnv = clos.env.extend(clos.variable, arg);
        return evaluate(extendedEnv, clos.body);
    } else if (typeof clos === 'string') {
        return `(${clos} ${arg})`; // Temporary solution, but this means that clos is not yet a value
    } else {
        throw new Error(`Attempted to apply a non-closure value (got [${clos} ${arg}]: [${typeof clos} ${typeof arg}])`);
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