# mengine-language

VS Code language support for [mengine](https://github.com/triviajon/mengine): a lambda-DAG based proof engine.

## Features

- Syntax highlighting for `.me` files
- Nested `(* ... *)` comment support
- Auto-closing pairs and bracket matching
- Indentation hints for `match`/`with`/`end` blocks

## Highlighted constructs

- **Vernacular commands**: `Definition`, `Theorem`, `Lemma`, `Fixpoint`, `Inductive`, `Axiom`, `Variable`, `Check`, `Print`, `Eval`, `Tactic`, `Register`, `Relation`
- **Term keywords**: `fun`, `forall`, `fix`, `match`, `with`, `end`, `let`, `in`
- **Sorts**: `Type`, `Prop`
- **Tactic built-ins**: `goal_type`, `type_of`, `current_goal`, `mk_hole`, `fill`, `subst`, `eunify`, `intro_step`, `rewrite`, `erewrite`, `cbv`, `compute`, etc.
- **Tactic combinators**: `try`, `repeat`, `first`, `idtac`, `fail`
- **Pattern variables**: `?x`, `?P`, etc.
- **Operators**: `:=`, `=>`, `<-`, `|-`, `||`, `|`, `;`
