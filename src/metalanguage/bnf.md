# Metalanguage grammar (BNF)

This file documents the core grammar for the metalanguage's term language.

```bnf
<term>         ::=  <prefix_term>

<prefix_term>  ::=  <lambda_expr>
                  | <forall_expr>
				  | <match_expr>
                  | <application>

<lambda_expr>  ::= ("fun") <binder> "=>" <term>

<forall_expr>  ::= ("forall") <binder> "," <term>

<binder>       ::= <ident> ":" <term>

<application>  ::= <atomic> { <atomic> }

<atomic>       ::= <ident>
				 | "Type"
				 | "Prop"
				 | "(" <term> ")"

// Arrow notation
// Arrow types like `A -> B` are represented as `forall (_: A), B`
// `=>` is used as the lambda/branch separator

// Pattern matching
<match_expr>   ::= "match" <term> "with" { <match_branch> } "end"
<match_branch> ::= "|" <pattern> "=>" <term>
<pattern>      ::= <ident>
```

Examples:

- Lambda expression:

```text
fun x : Type => x
```

- Universal quantification:

```text
forall x : Type, x
```

- Arrow type as forall (syntactic sugar):

```text
forall _: Type, Type    // represents Type -> Type
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
