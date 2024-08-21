import assert from "assert";
import { AppExpression, DefineExpression, Expression, LambdaExpression, VarExpression } from "./Expression";

/**
 * Grammar for REPL. 
 * 
 * base         ::= [lambdaExpr | defineExpr]*
 * lambdaExpr   ::= [var | '\' var '. ' lambdaExpr | '(' lambdaExpr ' ' lambdaExpr ')'] 
 * var          ::= [A-Za-z_][\w|']*
 * defineExpr   ::= 'define ' var ' ' lambdaExpr 
 */

type TokenType = 'LAMBDA' | 'DOT' | 'VAR' | 'LPAREN' | 'RPAREN' | 'WHITESPACE' | 'DEFINE' | 'EOF';

interface Token {
    type: TokenType;
    value: string;
}

const tokenSpec: [TokenType, RegExp][] = [
    ['DEFINE', /define/],
    ['LAMBDA', /\\/],
    ['DOT', /\./],
    ['VAR', /[A-Za-z_][\w|']*/],
    ['LPAREN', /\(/],
    ['RPAREN', /\)/],
    ['WHITESPACE', /\s+/],
];

export function tokenize(input: string): Token[] {
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

export function parse(tokens: Token[]): Expression {
    let pos = 0;

    function peek(): Token {
        return tokens[pos];
    }

    function consume(expectedType: TokenType): Token {
        const token = tokens[pos];
        if (token.type !== expectedType) {
            throw new Error(`Expected ${expectedType}, but found ${token.type}`);
        }
        pos++;
        return token;
    }

    function parseVar(): VarExpression {
        const token = consume('VAR');
        return new VarExpression(token.value);
    }

    function parseLambdaExpr(): Expression {
        if (peek().type === 'LAMBDA') {
            consume('LAMBDA');
            const variable = parseVar();
            consume('DOT');
            const body = parseLambdaExpr();
            return new LambdaExpression(variable, body);
        } else if (peek().type === 'LPAREN') {
            consume('LPAREN');
            const func = parseLambdaExpr();
            const arg = parseLambdaExpr();
            consume('RPAREN');
            return new AppExpression(func, arg);
        } else {
            return parseVar();
        }
    }

    function parseDefineExpr(): Expression {
        consume('DEFINE');
        const variable = parseVar();
        const body = parseLambdaExpr();
        return new DefineExpression(variable, body);
    }

    function parseBase(): Expression {
        if (peek().type === 'DEFINE') {
            const expr = parseDefineExpr();
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