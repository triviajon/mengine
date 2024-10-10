#include <stdio.h>
#include <stdlib.h>

#include "kernel/context.h"
#include "kernel/expression.h"
#include "kernel/inductive.h"
#include "kernel/utils.h"

int main() {
  Expression *type = init_type_expression();

  // Inductive natural numbers
  // Inductive nat : Type := z : nat | S : Forall (_: nat),  nat.
  // name = "nat"
  // parameters = none
  // arity = Type
  //
  // constructor 0: name = "z", type = Var{"nat"}
  // constructor 1: name = "S", type = Forall (_: Var{"nat"}), Var{"nat"}

  Expression *nat = init_var_expression("nat");
  Expression *z = init_var_expression("z");
  Expression *S = init_var_expression("S");

  Context *pars = context_create_empty();
  Expression *arity = init_type_expression();
  Constructor *c0 = init_constructor(z, nat);
  Constructor *c1 = init_constructor(S, init_arrow_expression(nat, nat));

  DoublyLinkedList *constructors = dll_create();
  dll_insert_at_tail(constructors, dll_new_node(c0));
  dll_insert_at_tail(constructors, dll_new_node(c1));

  Inductive *nat_inductive = init_inductive(nat, pars, arity, constructors);
  Context *with_nats =
      context_insert_inductive(context_create_empty(), nat_inductive);

  // Inductive equality
  // Inductive eq A (x:A) : A → Prop := eq_refl : eq x x.
  // name = "eq"
  // parameters = (A: Type) (x:A)
  // arity = A -> Type
  //
  // constructor 0: name = "eq_refl", type = eq A x x

  Expression *eq = init_var_expression("eq");
  Expression *eq_refl = init_var_expression("eq_refl");
  Expression *A = init_var_expression("A");
  Expression *x = init_var_expression("x");

  Context *eq_pars = context_create_empty();
  eq_pars = context_insert(eq_pars, A, type);
  eq_pars = context_insert(eq_pars, x, A);

  Expression *eq_arity = init_arrow_expression(A, type);
  Constructor *eq_c0 = init_constructor(
      eq_refl,
      init_app_expression(
          eq_pars,
          init_app_expression(eq_pars, init_app_expression(eq_pars, eq, A), x),
          x));
  
  DoublyLinkedList *eq_constructors = dll_create();
  dll_insert_at_tail(eq_constructors, dll_new_node(eq_c0));
  Inductive *eq_inductive = init_inductive(eq, eq_pars, eq_arity, eq_constructors);

  Context *with_eq_and_nats = context_insert_inductive(with_nats, eq_inductive);

  printf("%s\n", stringify_context(with_eq_and_nats));
  printf("Successfully ran program. \n");

  return 0;
}
