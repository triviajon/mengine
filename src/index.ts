import * as readline from 'readline';
import { Environment } from './env';
import { runProgram } from './parse';

function startREPL() {
    const env = new Environment();
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        prompt: '> '
    });

    let expressions: Array<string> = [];

    rl.prompt();

    rl.on('line', (line) => {
        line = line.trim();

        if (line === "start") {
            try {
                runProgram(env, expressions);
                expressions = [];
            } catch (err) {
                if (err instanceof Error) {
                    console.error(`Error: ${err.message}`);
                }
                expressions = [];
            }
        } else if (line == "pre") {
            const exprs = [
                String.raw`define zero \f. (\x. x)`,
                String.raw`define succ \n. (\f. (\x. (f (n f x))))`,
                String.raw`(succ zero)`

                // String.raw`define id \x. x`,
                // String.raw`(\y. y y) id`
            ]
            runProgram(env, exprs);
        } else if (line == "clear") {
            console.log("Program has been cleared.")
            expressions = [];
        } else if (line) {
            try {
                expressions.push(line);
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
