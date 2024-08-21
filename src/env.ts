import { VarExpression } from "./Expression";

interface EnvEntry {
    key: VarExpression;
    value: string;
}

// Immutable
export class Envirnoment {
    private env: Array<EnvEntry>;

    constructor() {
        this.env = [];
    }

    public extend(key: VarExpression, value: string): Envirnoment {
        const newEnv = new Envirnoment();
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
    constructor(public env: Envirnoment, public variable: VarExpression, public body: any) {
    }
}