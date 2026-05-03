#ifndef COMPILED_TACTICS_H
#define COMPILED_TACTICS_H

#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_ast.h"

void tactic_def_attach_compiled(MEngineRuntime *rt, TacticDef *def);

#endif  // COMPILED_TACTICS_H
