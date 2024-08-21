import * as readline from 'readline';
import { evaluate, runProgram } from './eval';
import { Envirnoment } from './env';
import { VarExpression, LambdaExpression, AppExpression, Expression, DefineExpression } from './Expression';

function parseInput(input: string): Expression {
    input = input.trim();

    const defineRegex = /^define\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+(.*)$/;
    const varRegex = /^[a-zA-Z_][a-zA-Z0-9_]*/;
    const lambdaRegex = /^\\([a-zA-Z_][a-zA-Z0-9_]*)/;
    const parenRegex = /^\(([^()]*)\)/;

    if (defineRegex.test(input)) {
        const match = input.match(defineRegex)!;
        const varName = match[1];
        const definition = match[2];
        const definitionExpr = parseInput(definition);
        return new DefineExpression(new VarExpression(varName), definitionExpr);
    }

    if (lambdaRegex.test(input)) {
        const match = input.match(lambdaRegex);
        const varName = match![1];
        const body = input.slice(match![0].length).trim();
        return new LambdaExpression(new VarExpression(varName), parseInput(body));
    }

    if (varRegex.test(input)) {
        const match = input.match(varRegex);
        const varName = match![0];
        input = input.slice(varName.length).trim();
        if (input.startsWith("(")) {
            const func = new VarExpression(varName);
            const args = parseInput(input);
            return new AppExpression(func, args);
        }
        return new VarExpression(varName);
    }

    if (parenRegex.test(input)) {
        const match = input.match(parenRegex);
        return parseInput(match![1]);
    }

    throw new Error(`Unrecognized input: ${input}`);
}

function startREPL() {
    const env = new Envirnoment();
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        prompt: '> '
    });

    let expressions: Array<Expression> = [];

    rl.prompt();

    rl.on('line', (line) => {
        line = line.trim();

        if (line === "start") {
            console.log(1)
            try {
                runProgram(env, expressions);
                expressions = [];  // Clear the expressions after running
            } catch (err) {
                console.log("erred")
                if (err instanceof Error) {
                    console.error(`Error: ${err.message}`);
                }
            }
        } else if (line) {
            console.log(2)
            try {
                const expr = parseInput(line);
                expressions.push(expr);
                console.log("Added expression:", expr);
            } catch (err) {
                if (err instanceof Error) {
                    console.error(`Error: ${err.message}`);
                }
            }
        }

        rl.prompt();
    }).on('close', () => {
        console.log('REPL closed.');
        process.exit(0);
    });
}


startREPL();
