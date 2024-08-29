import assert from "assert";
import { AppExpression, DefineExpression, Expression, isExpression, LambdaExpression, prettifyToString, SDefineExpression, VarExpression } from "./Expression";
import { Closure, Environment } from "./env";
import { evaluate } from "./eval";

// Grammar for REPL. 
// Note, that while synatically, symbols and vars are the same, they are treated differently in parsing. 
// 
// base         ::= [lambdaExpr | defineExpr]*
// lambdaExpr   ::= [var | '\' var '. ' lambdaExpr | '(' lambdaExpr ')' | lambdaExpr lambdaExpr]
// var          ::= [A-Za-z_][\w|']*
// defineExpr   ::= 'define ' var ' ' lambdaExpr 
// sdefineExpr  ::= 'sdefine ' var ' ' symbol
// symbol       ::= var

type TokenType = 'LAMBDA' | 'DOT' | 'VAR' | 'LPAREN' | 'RPAREN' | 'WHITESPACE' | 'DEFINE' | 'SDEFINE' | 'EOF';

interface Token {
    type: TokenType;
    value: string;
}

const tokenSpec: [TokenType, RegExp][] = [
    ['DEFINE', /define/],
    ['SDEFINE', /sdefine/],
    ['LAMBDA', /\\/],
    ['DOT', /\./],
    ['VAR', /[A-Za-z_][\w|']*/],
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

function parse(env: Environment, tokens: Token[]): Expression {
    let pos = 0;

    const contextStack: Array<Map<string, VarExpression>> = [new Map()];

    function peek(): Token {
        return tokens[pos];
    }

    function consume(expectedType: TokenType): Token {
        const token = tokens[pos];
        if (token.type !== expectedType) {
            throw new Error(`Expected ${expectedType}, but found ${token.type} at position ${pos}: ` +
                tokens.map(token => token.value).reduce((value, current) => value + " " + current));
        }
        pos++;
        return token;
    }

    function parseVar(): Expression {
        const token = consume('VAR');
        const currentContext = contextStack[contextStack.length - 1];

        // Check if this variable already exists in the current scope
        // If it doesn't, generate a fresh variable. If it does, we
        // should use that variable object instead of a new one.
        if (currentContext.has(token.value)) {
            return currentContext.get(token.value)!;
        }

        // If not, check if this variable has been defined previously in the env
        const varEnvValue: Expression | Closure | string | undefined = env.lookup(new VarExpression(token.value));
        if (varEnvValue !== undefined) {
            assert(varEnvValue instanceof Closure || isExpression(varEnvValue));
            if (varEnvValue instanceof Closure) {
                return new LambdaExpression(varEnvValue.variable, varEnvValue.body);
            }

            return varEnvValue;
        }

        currentContext.set(token.value, new VarExpression(token.value));
        return currentContext.get(token.value)!;
    }

    function parseSymbol(): string {
        const token = consume('VAR'); 
        return token.value;
    }


    function parseLambdaVar(): VarExpression {
        const token = consume('VAR');
        const currentContext = contextStack[contextStack.length - 1];

        // If we have a name collision, we should consider this to be a fresh variable
        // and overwrite the previous variable, since this is a new scope.
        currentContext.set(token.value, new VarExpression(token.value));
        return currentContext.get(token.value)!;
    }

    function parseLambdaExpr(): Expression {
        let expr: Expression;

        if (peek().type === 'LPAREN') {
            consume('LPAREN');
            expr = parseLambdaExpr();
            consume('RPAREN');
        } else if (peek().type === 'LAMBDA') {
            consume('LAMBDA');
            contextStack.push(new Map(contextStack[contextStack.length - 1]));
            const variable = parseLambdaVar();
            consume('DOT');
            const body = parseLambdaExpr();

            // End the scope after parsing the body
            contextStack.pop();

            expr = new LambdaExpression(variable, body);
        } else if (peek().type === 'VAR') {
            expr = parseVar();
        } else {
            throw new Error(`Unexpected token at position ${pos}: ${peek().value}`);
        }

        // Handle function application
        while (peek().type === 'VAR' || peek().type === 'LPAREN') {
            const arg = parseLambdaExpr();
            expr = new AppExpression(expr, arg);
        }

        return expr;
    }

    function parseDefineExpr(): Expression {
        consume('DEFINE');
        const variable = parseVar();
        const body = parseLambdaExpr();
        assert(variable instanceof VarExpression);
        return new DefineExpression(variable, body);
    }

    function parseSDefineExpr(): Expression {
        consume('SDEFINE');
        const variable = parseVar();
        const body = parseSymbol();
        assert(variable instanceof VarExpression);
        return new SDefineExpression(variable, body);
    }

    function parseBase(): Expression {
        if (peek().type === 'DEFINE') {
            const expr = parseDefineExpr();
            assert(peek().type === 'EOF');
            return expr;
        } else if (peek().type === 'SDEFINE') {
          const expr = parseSDefineExpr();
          assert(peek().type === 'EOF');
          return expr;
        } else {
            const expr = parseLambdaExpr();
            assert(peek().type === 'EOF');
            return expr;
        }
    }

    return parseBase();
}

export function runProgram(env: Environment, maybeExpressions: Array<string>): void {
    let currentEnv = env;
    for (const maybeExpr of maybeExpressions) {
        const expr = parse(currentEnv, tokenize(maybeExpr));
        if (expr instanceof DefineExpression) {
            currentEnv = currentEnv.extend(expr.variable, evaluate(currentEnv, expr.definition));
            console.log(currentEnv.toString());
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