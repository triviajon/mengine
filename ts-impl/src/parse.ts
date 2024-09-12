import assert from "assert";
import { AppExpression, DefineExpression, Expression, isExpression, LambdaExpression, prettifyToString, SDefineExpression, VarExpression } from "./Expression";
import { Closure, Environment } from "./env";
import { evaluate } from "./eval";

// Grammar for REPL. 
// <base>          ::= <define_expr> | <sdefine_expr> | <lambda_expr>
// <define_expr>   ::= "define" <whitespace> <var> <whitespace> <lambda_expr>
// <sdefine_expr>  ::= "sdefine" <whitespace> <var> <whitespace> <var>
// <lambda_expr>   ::= <abstraction> | <application> | <var>
// <abstraction>   ::= <lambda> <var> "." <whitespace>? <lambda_expr>
// <application> ::= "(" <lambda_expr> <lambda_expr>+ ")"
// <var>           ::= <letter> <alnum_or_quote>*
// <lambda>        ::= "\\" | "λ"
// <whitespace>    ::= <space>+
// <alnum_or_quote>::= <alnum> | "'"


type TokenType = 'DEFINE' | 'SDEFINE' | 'LAMBDA' | 'DOT' | 'VAR' | 'LPAREN' | 'RPAREN' | 'WHITESPACE' | 'EOF';

interface Token {
    type: TokenType;
    value: string;
}

const tokenSpec: [TokenType, RegExp][] = [
    ['DEFINE', /define/],
    ['SDEFINE', /sdefine/],
    ['LAMBDA', /\\/],
    ['DOT', /\./],
    ['VAR', /[A-Za-z][A-Za-z0-9'']*/],
    ['LPAREN', /\(/],
    ['RPAREN', /\)/],
    ['WHITESPACE', /\s+/],
];

function tokenize(input: string): Token[] {
    const tokens: Token[] = [];
    let pos = 0;

    while (pos < input.length) {
        let matchFound = false;

        for (const [type, regex] of tokenSpec) {
            const match = input.slice(pos).match(regex);

            if (match && match.index === 0) {
                matchFound = true;
                if (type !== 'WHITESPACE') {
                    tokens.push({ type, value: match[0] });
                }
                pos += match[0].length;
                break;
            }
        }

        if (!matchFound) {
            throw new Error(`Unexpected token at position ${pos}: "${input[pos]}"`);
        }
    }

    tokens.push({ type: 'EOF', value: '' });
    return tokens;
}

function parseApplication(tokens: Token[], index: number, contextStack: Array<Map<string, VarExpression>>, env: Environment): [Expression, number] {
    assert(tokens[index].type === 'LPAREN');
    const expressions: Expression[] = [];
    let nextIndex = index + 1;

    while (tokens[nextIndex] && tokens[nextIndex].type !== 'RPAREN') {
        let [expr, newIndex] = parseLambdaExpression(tokens, nextIndex, contextStack, env);
        expressions.push(expr);
        nextIndex = newIndex;

        // Handle cases where arguments are separated by whitespace
        while (tokens[nextIndex] && tokens[nextIndex].type === 'WHITESPACE') {
            nextIndex++;
        }
    }

    if (tokens[nextIndex].type !== 'RPAREN') {
        throw new Error(`Expected closing parenthesis, but found ${tokens[nextIndex].type}`);
    }

    if (expressions.length < 1) {
        throw new Error('Application must have at least one argument');
    }

    // Combine expressions into a single application expression
    let result = expressions[0];
    for (let i = 1; i < expressions.length; i++) {
        result = new AppExpression(result, expressions[i]);
    }

    return [result, nextIndex + 1];
}

function parseAbstraction(tokens: Token[], index: number, contextStack: Array<Map<string, VarExpression>>, env: Environment): [Expression, number] {
    assert(tokens[index].type === 'LAMBDA');
    assert(tokens[index + 1].type === 'VAR');

    // Abstraction means fresh variable. 
    const variable = new VarExpression(tokens[index + 1].value);
    contextStack.push(new Map(contextStack[contextStack.length - 1]));
    contextStack[contextStack.length - 1].set(tokens[index + 1].value, variable);

    assert(tokens[index + 2].type === 'DOT');
    const [body, nextIndex] = parseLambdaExpression(tokens, index + 3, contextStack, env);

    // Once we are done parsing this abstraction, we are done with this context frame
    contextStack.pop();

    return [new LambdaExpression(variable, body), nextIndex];
}

function parseVariable(tokens: Token[], index: number, contextStack: Array<Map<string, VarExpression>>, env: Environment): [Expression, number] {
    assert(tokens[index].type === 'VAR');

    const token = tokens[index];
    const currentContext = contextStack[contextStack.length - 1];

    // Local context takes more precedence than "global context"
    if (currentContext.has(token.value)) {
        return [currentContext.get(token.value)!, index + 1];
    }

    const variable = new VarExpression(token.value);
    const envLookup = env.lookup(variable);
    if (envLookup !== undefined) {
        assert(isExpression(envLookup));
        return [envLookup, index + 1];
    }

    currentContext.set(token.value, new VarExpression(token.value));

    return [currentContext.get(token.value)!, index + 1];
}

function parseDefine(tokens: Token[], index: number, contextStack: Array<Map<string, VarExpression>>, env: Environment): [Expression, number] {
    assert(tokens[index].type === 'DEFINE');
    assert(tokens[index + 1].type === 'VAR');
    const variable = new VarExpression(tokens[index + 1].value);
    // For now, we disallow recursive define statements (i.g., 'define x \y. x'),
    // so we do not need to make changes to the contextStack.
    const [lambdaExpr, nextIndex] = parseLambdaExpression(tokens, index + 2, contextStack, env);
    return [new DefineExpression(variable, lambdaExpr), nextIndex];
}

function parseSDefine(tokens: Token[], index: number, contextStack: Array<Map<string, VarExpression>>, env: Environment): [Expression, number] {
    assert(tokens[index].type === 'SDEFINE');
    assert(tokens[index + 1].type === 'VAR');
    const variable = new VarExpression(tokens[index + 1].value);
    assert(tokens[index + 2].type === 'VAR');
    const definition = tokens[index + 2].value;
    return [new SDefineExpression(variable, definition), index + 3];
}

function parseLambdaExpression(tokens: Token[], index: number, contextStack: Array<Map<string, VarExpression>>, env: Environment): [Expression, number] {
    switch (tokens[index].type) {
        case 'LAMBDA':
            return parseAbstraction(tokens, index, contextStack, env);
        case 'VAR':
            return parseVariable(tokens, index, contextStack, env);
        case 'LPAREN':
            return parseApplication(tokens, index, contextStack, env);
        default:
            throw new Error(`Unexpected token type ${tokens[index].type}`);
    }
}

// Main Parsing Function
function parse(tokens: Token[], env: Environment): Expression {
    let index = 0;
    const contextStack: Array<Map<string, VarExpression>> = [new Map()];

    function parseBase(): Expression {
        switch (tokens[index].type) {
            case 'DEFINE':
                const [defineExpr, nextIndex1] = parseDefine(tokens, index, contextStack, env);
                index = nextIndex1;
                return defineExpr;
            case 'SDEFINE':
                const [sdefineExpr, nextIndex2] = parseSDefine(tokens, index, contextStack, env);
                index = nextIndex2;
                return sdefineExpr;
            default:
                const [lambdaExpr, nextIndex3] = parseLambdaExpression(tokens, index, contextStack, env);
                index = nextIndex3;
                return lambdaExpr;
        }
    }

    const result = parseBase();
    if (tokens[index].type !== 'EOF') {
        throw new Error(`Unexpected token after end of input: ${tokens[index].value}`);
    }

    return result;
}

export function runProgram(env: Environment, maybeExpressions: Array<string>): void {
    let currentEnv = env;
    for (const maybeExpr of maybeExpressions) {
        const expr = parse(tokenize(maybeExpr), currentEnv);
        console.log(`Parsed [${maybeExpr}] as \n[${prettifyToString(expr.toString())}]`);
        if (expr instanceof DefineExpression) {
            currentEnv = currentEnv.extend(expr.variable, evaluate(currentEnv, expr.definition));
        } else if (expr instanceof SDefineExpression) {
            currentEnv = currentEnv.extend(expr.variable, expr.definition);
        } else {
            console.log("Evaluating lambda calculus term:", prettifyToString(expr.toString()));
            const evalResult = evaluate(currentEnv, expr);
            try {
                console.log("Result (-unbound):", prettifyToString(evalResult.toString()));
            } catch (e) {
                console.log("Result (+unbound):", evalResult);
            }
        }
    }
}