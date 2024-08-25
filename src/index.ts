import * as readline from 'readline';
import { evaluate, runProgram } from './eval';
import { Environment } from './env';
import { VarExpression, LambdaExpression, AppExpression, Expression, DefineExpression } from './Expression';
import { tokenize, parse } from './parse';

function startREPL() {
    const env = new Environment();
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
            try {
                runProgram(env, expressions);
                expressions = [];
            } catch (err) {
                console.log("erred")
                if (err instanceof Error) {
                    console.error(`Error: ${err.message}`);
                }
            }
        } else if (line == "pre") {
            const expr = parse(tokenize("((\\x. x) (\\y. y))"));
            console.log("Using preloaded test value:", expr);
            runProgram(env, [expr]);
        } else if (line == "clear") {
            console.log("Program has been cleared.")
            expressions = [];
        } else if (line) {
            try {
                const tokens = tokenize(line);
                const expr = parse(tokens);
                expressions.push(expr);
                console.log("Added expression:", expr.toString());
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
