Similar to Rocq's Vernacular:

```bnf
<command> ::= <declaration>
            | <definition>
            | <statement>
            | <inductive>
            | <fixpoint>
            | <tactic_def>
            | <check>
            | <print>
            | <show>

<declaration> ::= <declaration_keyword> <assumption> "."
<declaration_keyword> ::= "Axiom" | "Variable"
<assumption> ::= <binder>

<definition> ::= "Definition" <identifier> { "(" <binder> ")" }
                 ":" <term>
                 ":=" <term> "."

<statement> ::= <statement_keyword> <identifier>
                ":" <term> "."
<statement_keyword> ::= "Theorem" | "Lemma"

<inductive> ::= "Inductive" <identifier> { "(" <binder> ")" }
                ":" <term>
                ":=" { <constructor> } "."

<constructor> ::= "|" <identifier> ":" <term>

<fixpoint> ::= "Fixpoint" <identifier> { "(" <binder> ")" } <decreasing_arg_annotation>
                ":" <term>
                ":=" <term> "."

<check> ::= "Check" <term> "."

<print> ::= "Print" <identifer> "."

<show> ::= "Show" <show_keyword> "."
<show_keyword> ::= "Context" | "Proof" | "Goal" | "State"

<tactic_def> ::= "Tactic" <identifier> { <identifier> } ":=" <tactic_expr> "."

<!-- The following are defined in src/termlanguage/bnf.md -->
<decreasing_arg_annotation> ::= ...
<binder> ::= ...
<identifier> ::= ...
<term> ::= ...

```

It is also a requirement that all commands end in newlines.
