import { VarExpression } from "./Expression";

interface EnvEntry {
    key: VarExpression;
    value: string;
}

// Immutable
export class Environment {
    private env: Array<EnvEntry>;

    constructor() {
        this.env = [];
    }

    public extend(key: VarExpression, value: string): Environment {
        const newEnv = new Environment();
        newEnv.env = [...this.env, { key, value }];
        return newEnv;
    }

    public lookup(key: VarExpression): string | undefined {
        for (const envEntry of this.env) {
            if (envEntry.key.equalValue(key)) {
                return envEntry.value;
            }
        }
        return undefined;
    }
}

export class Closure {
    constructor(public env: Environment, public variable: VarExpression, public body: any) {
    }

    public toString(): string {
        return `Closure { var: ${this.variable}, body: ${this.body} }`;
    }
}

export class NVar {
    constructor(public name: string) {
    }

    public toString(): string {
        return `NVar { name: ${this.name} }`;
    }
}

export class NApp {
    constructor(public func: Neutral, public arg: string) {

    }

    public toString(): string {
        return `NAppp { func: ${this.func}, arg: ${this.arg} }`
    }
}

export type Neutral = NVar | NApp