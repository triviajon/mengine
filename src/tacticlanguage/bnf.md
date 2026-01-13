Tactic language

```bnf
<proof_command> ::= | <tactic> "."
                    | "Admitted" "."

<tactic> ::= "intro" [ <identifier> ]
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
