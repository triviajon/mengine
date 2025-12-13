Tactic language

```bnf
<proof_command> ::= "Proof" "."
                 | <tactic> "."
                 | "Qed" "."
                 | "Admitted" "."

<tactic> ::= "intro" [ <ident> ]
          | "intros" { <ident> }
          | "apply" <term>
          | "eapply" <term>
          | "exact" <term>
          | "rewrite" <term> "with" <term>
          | "rewrite" "<-" <term> "with" <term>
          | "reflexivity"
          | "assumption"
          | "split"
          | "left"
          | "right"
          | "exists" <term>
```