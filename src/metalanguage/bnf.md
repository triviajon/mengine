# Metalanguage grammar (BNF)

This file documents the core grammar for the metalanguage's term language.

```bnf
<term>         ::=  <prefix_term>

<prefix_term>  ::=  <lambda_expr>
                  | <forall_expr>
				  | <match_expr>
				  | <fix_expr>
                  | <let_expr>
                  | <application>

<lambda_expr>  ::= "fun" "(" <binder> ")" "=>" <term>

<forall_expr>  ::= "forall" "(" <binder> ")" "," <term>

<fix_expr>     ::= "fix" <identifier> { "(" <binder> ")" } <decreasing_arg_annotation> ":" <term> ":=" <term>

<decreasing_arg_annotation> ::= "{" "struct" <identifier> "}"

<let_expr>     ::= "let" <identifier> ":" <term> ":=" <term> "in" <term>

<binder>       ::= <identifier> ":" <term>

<application>  ::= <atomic> { <atomic> }

<atomic>       ::= <identifier>
				 | "Type"
				 | "Prop"
				 | "(" <term> ")"

<identifier> 	   ::= [_'A-Za-z][_'A-Za-z0-9]*

// Arrow notation
// Arrow types like `A -> B` are represented as `forall (_: A), B`
// `=>` is used as the lambda/branch separator

// Pattern matching
<match_expr>   ::= "match" <term> "with" { <match_branch> } "end"
<match_branch> ::= "|" <pattern> "=>" <term>
<pattern>      ::= <identifier>
```
