# Metalanguage grammar (BNF)

This file documents the core grammar for the metalanguage's term language.

```bnf
<term>         ::=  <prefix_term>

<prefix_term>  ::=  <lambda_expr>
                  | <forall_expr>
				  | <match_expr>
                  | <let_expr>
                  | <application>

<lambda_expr>  ::= "fun" "(" <binder> ")" "=>" <term>

<forall_expr>  ::= "forall" "(" <binder> ")" "," <term>

<let_expr>     ::= "let" <ident> ":=" <term> "in" <term>

<binder>       ::= <ident> ":" <term>

<application>  ::= <atomic> { <atomic> }

<atomic>       ::= <ident>
				 | "Type"
				 | "Prop"
				 | "(" <term> ")"

<ident> 	   ::= [_A-Za-z][_A-Za-z0-9]*

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
fun (x : Type) => x
```

- Universal quantification:

```text
forall (x : Type), x
```

- Arrow type as forall (syntactic sugar):

```text
forall (_: Type), Type    // represents Type -> Type
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

- Let-binding

```text
let x := n + 1 in
let y := x * x in
y + x
```
