#ifndef SYMBOLIC_H
#define SYMBOLIC_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"

extern Expression *positive;
extern Expression *xI;
extern Expression *xO;
extern Expression *xH;

extern Expression *N;
extern Expression *N0;
extern Expression *positive;

extern Expression *Z;
extern Expression *Z0;
extern Expression *Zpos;
extern Expression *Zneg;

extern Expression *string;
extern Expression *a_string;
extern Expression *b_string;
extern Expression *c_string;
extern Expression *not_eq_string_b_a;

extern Expression *list;
extern Expression *list_nil;
extern Expression *list_cons;

extern Expression *option;
extern Expression *option_some;
extern Expression *option_none;

extern Expression *word;
extern Expression *word_of_Z;
extern Expression *word_add;
extern Expression *word_sub;
extern Expression *byte;
extern Expression *trace;
extern Expression *mem;
extern Expression *locals;

extern Expression *bopname;
extern Expression *bopname_add;
extern Expression *bopname_sub;
extern Expression *bopname_mul;
extern Expression *bopname_mulhuu;
extern Expression *bopname_divu;
extern Expression *bopname_remu;
extern Expression *bopname_and;
extern Expression *bopname_or;
extern Expression *bopname_xor;
extern Expression *bopname_sru;
extern Expression *bopname_slu;
extern Expression *bopname_srs;
extern Expression *bopname_lts;
extern Expression *bopname_ltu;
extern Expression *bopname_eq;
extern Expression *interp_binop;
extern Expression *binop_add_to_word_add;
extern Expression *binop_add_to_word_sub;

extern Expression *expr;
extern Expression *expr_literal;
extern Expression *expr_var;
extern Expression *expr_op;
extern Expression *expr_ite;
extern Expression *eval_expr_ref;
extern Expression *eval_expr;

extern Expression *cmd;
extern Expression *cmd_skip;
extern Expression *cmd_set;
extern Expression *cmd_unset;
extern Expression *cmd_cond;
extern Expression *cmd_seq;
extern Expression *cmd_input;
extern Expression *cmd_output;

extern Expression *partial_map;
extern Expression *partial_map_empty;
extern Expression *partial_map_get;
extern Expression *partial_map_put;
extern Expression *partial_map_remove;
extern Expression *partial_map_get_put_same;
extern Expression *partial_map_get_put_diff;
extern Expression *partial_map_put_put_same;

extern Expression *IOEvent;
extern Expression *IOEvent_IN;
extern Expression *IOEvent_OUT;

extern Expression *exec;
extern Expression *exec_skip;
extern Expression *eval_expr;
extern Expression *exec_set;
extern Expression *exec_seq;
extern Expression *exec_input;

extern Expression *word_add_0_r;
extern Expression *word_add_sub_cancel;

void run_symbolic(int n);

#endif // SYMBOLIC_H