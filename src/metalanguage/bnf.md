# Metalanguage grammar (BNF)

This file documents the core grammar for the metalanguage's term language.

```bnf
<term>         ::= <prefix_term>

<prefix_term>  ::= <lambda_expr>
								 | <forall_expr>
								 | <application>

<lambda_expr>  ::= ("lambda" | "fun" | "\\") <binder> "=>" <term>

<forall_expr>  ::= ("forall") <binder> "," <term>

<binder>       ::= <ident> ":" <term>

<application>  ::= <atomic> { <atomic> }

<atomic>       ::= <ident>
				 | <hole>
				 | "Type"
				 | "Prop"
				 | "(" <term> ")"

// Arrow notation
// `->` is used for function types and implication
// `=>` is used as the lambda/branch separator
// Incorporate them explicitly in the grammar:

<arrow_type>   ::= <term> "->" <term>

// Pattern matching
<match_expr>   ::= "match" <term> "with" { "|" <pattern> "=>" <term> } "end"

<pattern>      ::= <ident>  // simple identifier pattern (extendable)
```

Examples:

- Lambda with function-type parameter:

```text
fun x : Type -> Type => x
```

- Universal quantification:

```text
forall x : Type, x
```

- Application (left-associative):

```text
f x y    // parsed as (f x) y
```

- Pattern match (uses `=>` for branches):

```text
match b with
	| true  => false
	| false => true
end
```
