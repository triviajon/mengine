Similar to Rocq's Vernacular:

```bnf
<command> ::= <declaration>
            | <definition>
            | <statement>
       <!-- | <fixpoint> -->
       <!-- | <inductive> -->

<declaration> ::= <declaration_keyword> <assumption> "."
<declaration_keyword> ::= "Axiom" | "Variable"
<assumption> ::= <binder>

<definition> ::= "Definition" <identifier> { "(" <binder> ")" }
                 ":" <term>
                 ":=" <term> "."

<statement> ::= <statement_keyword> <identifier>
                { "(" <binder> ")" }
                ":" <term> "."
<statement_keyword> ::= "Theorem" | "Lemma"

<!-- The following are defined in src/metalanguage/bnf.md -->
<binder> ::= ...
<identifier> ::= ...
<term> ::= ...

```

It is also a requirement that all commands end in newlines.
