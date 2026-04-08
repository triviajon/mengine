Tactic language

```bnf
<tactic_def>        ::= "Tactic" <identifier> { <identifier> } ":=" <tactic_expr> "."

<proof_command>     ::= <tactic_expr> "."
                      | "Admitted" "."

<tactic_expr>       ::= <tactic_seq> { "||" <tactic_seq> }
<tactic_seq>        ::= <tactic_atom> { ";" <tactic_atom> }
<tactic_atom>       ::= <primitive_tactic>
                      | "try" <tactic_atom>
                      | "repeat" <tactic_atom>
                      | "first" "[" <tactic_expr> { "|" <tactic_expr> } "]"
                      | "idtac"
                      | "fail"
                      | "(" <tactic_expr> ")"
                      | <identifier> { <atomic_term> }

<primitive_tactic>  ::= "intro" [ <identifier> ]
                      | "intros" { <identifier> }
                      | "apply" <term>
                      | "eapply" <term>
                      | "exact" <term>
                      | "rewrite" <term> "with" <term>
                      | "rewrite" "<-" <term> "with" <term>
                      | "erewrite" <term> "with" <term>
                      | "erewrite" "<-" <term> "with" <term>
                      | "reflexivity"
                      | "assumption"
                      | "split"
                      | "left"
                      | "right"
                      | "exists" <term>
                      | "cbv" { <ident> }
```
