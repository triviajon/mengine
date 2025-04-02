#include "symbolic.h"

//   Inductive expr: Set :=
//   | literal (v: Z)
//   | var (x: String.string)
//   | load (_ : access_size) (addr:expr)
//   | inlinetable (_ : access_size) (table: list Byte.byte) (index: expr)
//   | op (op: bopname) (e1 e2: expr)
//   | ite (c e1 e2: expr). 

Expression *positive = NULL;
Expression *xI = NULL;
Expression *xO = NULL;
Expression *xH = NULL;

Expression *N = NULL;
Expression *N0 = NULL;
Expression *Npos = NULL;

Expression *Z = NULL;
Expression *Z0 = NULL;
Expression *Zpos = NULL;
Expression *Zneg = NULL;

Expression *string = NULL;
Expression *a_string = NULL;
Expression *b_string = NULL;
Expression *c_string = NULL;
Expression *terminator_string = NULL;

Expression *list = NULL;
Expression *list_nil = NULL;
Expression *list_cons = NULL;

Expression *option = NULL;
Expression *option_some = NULL;
Expression *option_none = NULL;

Expression *word = NULL;
Expression *word_of_Z = NULL;
Expression *word_add = NULL;
Expression *byte = NULL;
Expression *trace = NULL;
Expression *mem = NULL;
Expression *locals = NULL;

Expression *bopname = NULL;
Expression *bopname_add = NULL;
Expression *bopname_sub = NULL;
Expression *bopname_mul = NULL;
Expression *bopname_mulhuu = NULL;
Expression *bopname_divu = NULL;
Expression *bopname_remu = NULL;
Expression *bopname_and = NULL;
Expression *bopname_or = NULL;
Expression *bopname_xor = NULL;
Expression *bopname_sru = NULL;
Expression *bopname_slu = NULL;
Expression *bopname_srs = NULL;
Expression *bopname_lts = NULL;
Expression *bopname_ltu = NULL;
Expression *bopname_eq = NULL;
Expression *interp_binop = NULL;
Expression *binop_add_to_word_add = NULL;

Expression *expr = NULL;
Expression *expr_literal = NULL;
Expression *expr_var = NULL;
Expression *expr_op = NULL;
Expression *expr_ite = NULL;
Expression *eval_expr = NULL;

Expression *cmd = NULL;
Expression *cmd_skip = NULL;
Expression *cmd_set = NULL;
Expression *cmd_unset = NULL;
Expression *cmd_cond = NULL;
Expression *cmd_seq = NULL;
Expression *cmd_input = NULL;
Expression *cmd_output = NULL;

Expression *partial_map = NULL;
Expression *partial_map_empty = NULL;
Expression *partial_map_get = NULL;
Expression *partial_map_put = NULL;
Expression *partial_map_remove = NULL;
Expression *partial_map_get_put_same = NULL;
Expression *partial_map_get_put_diff = NULL;

Expression *IOEvent = NULL;
Expression *IOEvent_IN = NULL;
Expression *IOEvent_OUT = NULL;

Expression *exec = NULL;
Expression *exec_skip = NULL;
Expression *exec_set = NULL;
Expression *exec_seq = NULL;
Expression *exec_input = NULL;

Expression *word_add_0_r = NULL;


void init_positive() {
	positive = init_var_expression("positive", init_type_expression());

	xI = init_var_expression("xI", init_arrow_expression(positive, positive));
	xO = init_var_expression("xO", init_arrow_expression(positive, positive));
	xH = init_var_expression("xH", positive);
}

void init_N() {
	N = init_var_expression("N", init_type_expression());
	N0 = init_var_expression("N0", N);
	Npos = init_var_expression("Npos", init_arrow_expression(positive, N));
}

void init_Z() {
	Z = init_var_expression("Z", init_type_expression());
	Z0 = init_var_expression("Z0", Z);
	Zpos = init_var_expression("Zpos", init_arrow_expression(positive, Z));
	Zneg = init_var_expression("Zneg", init_arrow_expression(positive, Z));
}

void init_string() {
	string = init_var_expression("string", init_type_expression());
	a_string = init_var_expression("a", string);
	b_string = init_var_expression("b", string);
	c_string = init_var_expression("c", string);
	terminator_string = init_var_expression("\\0", string);
}

void init_list() {
	list = init_var_expression("list", init_arrow_expression(init_type_expression(), init_type_expression()));
	Expression *bind1 = init_var_expression("A", init_type_expression());
	list_nil = init_var_expression("list_nil", 
		init_forall_expression(bind1, init_app_expression(list, bind1)));

	Expression *bind2 = init_var_expression("A", init_type_expression());
	list_cons = init_var_expression("list_cons", 
		init_forall_expression(bind2, 
			init_arrow_expression(bind2, 
				init_arrow_expression(init_app_expression(list, bind2),
					init_app_expression(list, bind2)
			))));
}

void init_word() {
	word = init_var_expression("word", init_type_expression());
}

void init_storage() {
	word_of_Z = init_var_expression("word_of_Z", init_arrow_expression(Z, word));
	word_add = init_var_expression("word_add", 
		init_arrow_expression(
			init_app_expression(option, word), 
			init_arrow_expression(init_app_expression(option, word), word)));
	byte = init_var_expression("byte", init_type_expression());
	trace = init_app_expression(list, IOEvent);
	mem = init_app_expression(init_app_expression(partial_map, word), byte);
	locals = init_app_expression(init_app_expression(partial_map, string), word);
}

void init_bopname() {
	bopname = init_var_expression("bopname", init_type_expression());
	bopname_add = init_var_expression("bopname_add", bopname);
	bopname_sub = init_var_expression("bopname_sub", bopname);
	bopname_mul = init_var_expression("bopname_mul", bopname);
	bopname_mulhuu = init_var_expression("bopname_mulhuu", bopname);
	bopname_divu = init_var_expression("bopname_divu", bopname);
	bopname_remu = init_var_expression("bopname_remu", bopname);
	bopname_and = init_var_expression("bopname_and", bopname);
	bopname_or = init_var_expression("bopname_or", bopname);
	bopname_xor = init_var_expression("bopname_xor", bopname);
	bopname_sru = init_var_expression("bopname_sru", bopname);
	bopname_slu = init_var_expression("bopname_slu", bopname);
	bopname_srs = init_var_expression("bopname_srs", bopname);
	bopname_lts = init_var_expression("bopname_lts", bopname);
	bopname_ltu = init_var_expression("bopname_ltu", bopname);
	bopname_eq = init_var_expression("bopname_eq", bopname);
	interp_binop = init_var_expression("interp_binop", init_arrow_expression(bopname, init_arrow_expression(init_app_expression(option, word), init_arrow_expression(init_app_expression(option, word), word))));
	
	Expression *w1 = init_var_expression("w1", init_app_expression(option, word));
	Expression *w2 = init_var_expression("w2", init_app_expression(option, word));
	// eq (option word) (interp_binop bopname_add w1 w2) (word_add w1 w2)
	binop_add_to_word_add = init_var_expression("binop_add_to_word_add", 
		init_forall_expression(w1, init_forall_expression(w2, 
			init_app_expression(init_app_expression(init_app_expression(eq, word), 
				init_app_expression(init_app_expression(init_app_expression(interp_binop, bopname_add), w1), w2)),
				init_app_expression(init_app_expression(word_add, w1), w2))
			)));
}

void init_expr() {
	expr = init_var_expression("expr", init_type_expression());
	expr_literal = init_var_expression("expr_literal", init_forall_expression(init_var_expression("v", Z), expr));
	expr_var = init_var_expression("expr_var", init_forall_expression(init_var_expression("x", string), expr));
	expr_op = init_var_expression("expr_op", 
		init_forall_expression(init_var_expression("op", bopname), 
			init_forall_expression(init_var_expression("e1", expr), 
				init_forall_expression(init_var_expression("e2", expr), 
					expr))));
	expr_ite = init_var_expression("expr_ite", 
		init_forall_expression(init_var_expression("c", expr), 
			init_forall_expression(init_var_expression("e1", expr), 
				init_forall_expression(init_var_expression("e2", expr), 
					expr))));

	Expression *eval_expr_m = init_var_expression("me", mem);
	Expression *eval_expr_l = init_var_expression("le", locals);
	Expression *eval_expr_ref = init_var_expression("eval_expr", init_arrow_expression(expr, init_app_expression(option, word)));
	Expression *eval_expr_e = init_var_expression("e", expr);
	Expression *eval_expr_literal_v = init_var_expression("v", Z);
	Expression *eval_expr_var_x = init_var_expression("x", string);
	Expression *eval_expr_op_op = init_var_expression("op", bopname);
	Expression *eval_expr_op_e1 = init_var_expression("e1", expr);
	Expression *eval_expr_op_e2 = init_var_expression("e2", expr);
	Expression *eval_expr_fix = init_fix_expression(eval_expr_ref, eval_expr_e, 
			init_match_expr_expression(
				eval_expr_e,
				init_app_expression(expr_literal, eval_expr_literal_v),
				init_app_expression(init_app_expression(option_some, word), init_app_expression(word_of_Z, eval_expr_literal_v)),
				init_app_expression(expr_var, eval_expr_var_x),
				init_app_expression(init_app_expression(init_app_expression(init_app_expression(partial_map_get, string), word), eval_expr_l), eval_expr_var_x),
				init_app_expression(init_app_expression(init_app_expression(expr_op, eval_expr_op_op), eval_expr_op_e1), eval_expr_op_e2),
				init_app_expression(init_app_expression(option_some, word), init_app_expression(init_app_expression(init_app_expression(interp_binop, eval_expr_op_op), 
					init_app_expression(eval_expr_ref, eval_expr_op_e1)), 
					init_app_expression(eval_expr_ref, eval_expr_op_e2))),
				init_app_expression(option, word)
			)
	);
	eval_expr = init_lambda_expression(eval_expr_m, init_lambda_expression(eval_expr_l, eval_expr_fix));

}

void init_cmd() {
	cmd = init_var_expression("cmd", init_type_expression());
	cmd_skip = init_var_expression("cmd_skip", cmd);
	cmd_set = init_var_expression("cmd_set", 
		init_forall_expression(init_var_expression("lhs", string), 
			init_forall_expression(init_var_expression("rhs", expr),
				cmd)));
	cmd_unset = init_var_expression("cmd_unset", 
		init_forall_expression(init_var_expression("lhs", string), cmd));
	cmd_cond = init_var_expression("cmd_cond", 
		init_forall_expression(init_var_expression("condition", expr),
			init_forall_expression(init_var_expression("nonzero_branch", cmd), 
				init_forall_expression(init_var_expression("zero_branch", cmd), 
					cmd))));
	cmd_seq = init_var_expression("cmd_seq", 
		init_forall_expression(init_var_expression("s1", cmd), 
			init_forall_expression(init_var_expression("s2", cmd), 
				cmd)));
	cmd_input = init_var_expression("cmd_input", init_forall_expression(init_var_expression("lhs", string), cmd));
	cmd_output = init_var_expression("cmd_output", init_forall_expression(init_var_expression("arg", string), cmd));
}

void init_option() {   
    option = init_var_expression("option", init_arrow_expression(init_type_expression(), init_type_expression()));
    
	Expression *A1 = init_var_expression("A", init_type_expression());
    option_some = init_var_expression("option_some", 
        init_forall_expression(A1, init_arrow_expression(A1, init_app_expression(option, A1))));
    
	Expression *A2 = init_var_expression("A", init_type_expression());
    option_none = init_var_expression("option_none", 
        init_forall_expression(A2, init_app_expression(option, A2)));
}

void init_io_event() {
	IOEvent = init_var_expression("IOEvent", init_type_expression());
	IOEvent_IN = init_var_expression("IOEvent_IN", init_arrow_expression(word, IOEvent));
	IOEvent_OUT = init_var_expression("IOEvent_OUT", init_arrow_expression(word, IOEvent));
}


void init_partial_map() {

	partial_map = init_var_expression("partial_map", init_arrow_expression(init_type_expression(), init_arrow_expression(init_type_expression(), init_type_expression())));

	Expression *A1 = init_var_expression("A", init_type_expression());
	Expression *B1 = init_var_expression("B", init_type_expression());
	partial_map_empty = init_var_expression("partial_map_empty", init_forall_expression(A1, 
		init_forall_expression(B1, 
			init_app_expression(init_app_expression(partial_map, A1), B1))
		));
	
	Expression *A2 = init_var_expression("A", init_type_expression());
	Expression *B2 = init_var_expression("B", init_type_expression());
	partial_map_get = init_var_expression("partial_map_get", init_forall_expression(A2, 
		init_forall_expression(B2, 
			init_arrow_expression(init_app_expression(init_app_expression(partial_map, A2), B2), 
		init_arrow_expression(A2, init_app_expression(option, B2))))
	));
	
	Expression *A3 = init_var_expression("A", init_type_expression());
	Expression *B3 = init_var_expression("B", init_type_expression());
	partial_map_put = init_var_expression("partial_map_put", init_forall_expression(A3, 
		init_forall_expression(B3, 
			init_arrow_expression(init_app_expression(init_app_expression(partial_map, A3), B3), 
		init_arrow_expression(A3, init_arrow_expression(B3, init_app_expression(init_app_expression(partial_map, A3), B3)))))
	));
	
	Expression *A4 = init_var_expression("A", init_type_expression());
	Expression *B4 = init_var_expression("B", init_type_expression());
	partial_map_remove = init_var_expression("partial_map_remove", init_forall_expression(A4, 
		init_forall_expression(B4, 
			init_arrow_expression(init_app_expression(init_app_expression(partial_map, A4), B4), 
		init_arrow_expression(A4, init_app_expression(option, B4))))
	));

	// map.get_put_same : forall (key value : Type) (map : map key value)
	// (k : key) (v : value), eq (map.get (map.put m k v) k) (Some v)
	Expression *A5 = init_var_expression("A", init_type_expression());
	Expression *B5 = init_var_expression("B", init_type_expression());
	Expression *map5 = init_var_expression("map", init_app_expression(init_app_expression(partial_map, A5), B5));
	Expression *k5 = init_var_expression("k", A5);
	Expression *v5 = init_var_expression("v", B5);
	Expression *sub5_1 = init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(init_app_expression(partial_map_put, A5), B5), 
		map5), k5), v5);
	Expression *sub5_2 = init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(partial_map_get, A5), B5), sub5_1), k5);

	partial_map_get_put_same = init_var_expression("partial_map_get_put_same", init_forall_expression(A5, init_forall_expression(B5, 
		init_forall_expression(map5, init_forall_expression(k5, init_forall_expression(v5, 
			init_app_expression(init_app_expression(init_app_expression(
				eq, init_app_expression(option, B5)), 
				sub5_2),
				init_app_expression(init_app_expression(option_some, B5), v5)))))))); 
	partial_map_get_put_diff = NULL;
}

void init_exec() {
	// exec
	exec = init_var_expression("exec", init_arrow_expression(trace, 
		init_arrow_expression(cmd,
			init_arrow_expression(mem,
				init_arrow_expression(locals,
					init_arrow_expression(
						init_arrow_expression(trace, init_arrow_expression(mem, init_arrow_expression(locals, init_prop_expression()))), 
						init_prop_expression()))))));
	
	// exec_skip
	Expression *t1 = init_var_expression("t", trace);
	Expression *m1 = init_var_expression("m", mem);
	Expression *l1 = init_var_expression("l", locals);
	Expression *post1 = init_var_expression("post", init_arrow_expression(trace, init_arrow_expression(mem, init_arrow_expression(locals, init_prop_expression()))));
	exec_skip = init_var_expression("exec_skip", 
		init_forall_expression(t1, 
			init_forall_expression(m1, 
				init_forall_expression(l1, 
					init_forall_expression(post1, 
						init_arrow_expression(
							init_app_expression(init_app_expression(init_app_expression(post1, t1), m1), l1), 
							init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(exec, t1), cmd_skip), m1), l1), post1)
						))))));
	
	// exec_set
	Expression *x2 = init_var_expression("x", string);
	Expression *e2 = init_var_expression("e", expr);
	Expression *t2 = init_var_expression("t", trace);
	Expression *m2 = init_var_expression("m", mem);
	Expression *l2 = init_var_expression("l", locals);
	Expression *post2 = init_var_expression("post", init_arrow_expression(trace, init_arrow_expression(mem, init_arrow_expression(locals, init_prop_expression()))));
	Expression *v2 = init_var_expression("v", word);

	Expression *H2_1 = init_app_expression(
		init_app_expression(
			init_app_expression(eq, init_app_expression(option, word)),
			init_app_expression(init_app_expression(init_app_expression(eval_expr, m2), l2), e2)
		), 
		init_app_expression(init_app_expression(option_some, word), v2)
	);
	Expression *H2_2 = init_app_expression(init_app_expression(init_app_expression(post2, t2), m2), 
		init_app_expression(
			init_app_expression(
				init_app_expression(
					init_app_expression(
						init_app_expression(partial_map_put, string), word), l2), x2), v2)
	);
	Expression *C2 = init_app_expression(
		init_app_expression(
			init_app_expression(
				init_app_expression(init_app_expression(exec, t2), 
					init_app_expression(
						init_app_expression(cmd_set, x2), e2)
				), m2), l2), post2);

	exec_set = init_var_expression("exec_set", 
		init_forall_expression(t2, 
		init_forall_expression(x2, 
		init_forall_expression(e2, 
		init_forall_expression(m2, 
		init_forall_expression(l2, 
		init_forall_expression(post2, 
		init_forall_expression(v2, 
			init_arrow_expression(H2_1, 
				init_arrow_expression(H2_2, C2))
	))))))));

	// exec_seq
	Expression *t3 = init_var_expression("t", trace);
	Expression *c1_3 = init_var_expression("c1", cmd);
	Expression *c2_3 = init_var_expression("c2", cmd);
	Expression *m3 = init_var_expression("m", mem);
	Expression *l3 = init_var_expression("l", locals);
	Expression *post3 = init_var_expression("post", init_arrow_expression(trace, init_arrow_expression(mem, init_arrow_expression(locals, init_prop_expression()))));

	Expression *t3_p = init_var_expression("t'", trace);
	Expression *m3_p = init_var_expression("m'", mem);
	Expression *l3_p = init_var_expression("l'", locals);
	Expression *mid_post3 = init_lambda_expression(t3_p, init_lambda_expression(m3_p, 
		init_lambda_expression(l3_p,
			init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(
				init_app_expression(exec, t3_p), c2_3), m3_p), l3_p), post3
	))));
	Expression *H3 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(exec, t3), c1_3), m3), l3), mid_post3);

	Expression *C3 = init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(init_app_expression(exec, t3), 
			init_app_expression(init_app_expression(cmd_seq, c1_3), c2_3)),
		m3), l3), post3); 
	
	exec_seq = init_var_expression("exec_seq", 
		init_forall_expression(t3,
		init_forall_expression(c1_3,
		init_forall_expression(c2_3,
		init_forall_expression(m3,
		init_forall_expression(l3,
		init_forall_expression(post3,
			init_arrow_expression(H3, C3))
	))))));

	// exec_input : forall t lhs m l post,
	// 	(forall v : word, post (list_cons IOEvent (IOEvent_IN v) t) m (partial_map_put string word l lhs v)) ->
    //     exec t (cmd_input lhs) m l pos
	Expression *t4 = init_var_expression("t", trace);
	Expression *lhs4 = init_var_expression("lhs", string);
	Expression *m4 = init_var_expression("m", mem);
	Expression *l4 = init_var_expression("l", locals);
	Expression *post4 = init_var_expression("post", init_arrow_expression(trace, init_arrow_expression(mem, init_arrow_expression(locals, init_prop_expression()))));

	Expression *v4 = init_var_expression("v", word);
	Expression *t4_sub = init_app_expression(init_app_expression(init_app_expression(list_cons, IOEvent), init_app_expression(IOEvent_IN, v4)), t4);
	Expression *m4_sub = m4;
	Expression *l4_sub = init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(partial_map_put, string), word), l4), lhs4), v4);
	Expression *H4 = init_forall_expression(v4, 
		init_app_expression(init_app_expression(init_app_expression(post4, t4_sub), m4_sub), l4_sub));
	
	Expression *C4 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(
		exec, t4), init_app_expression(cmd_input, lhs4)), m4), l4), post4);
	
	exec_input = init_forall_expression(t4, init_forall_expression(lhs4, init_forall_expression(m4, 
		init_forall_expression(l4, init_forall_expression(post4, init_arrow_expression(H4, C4))))));
}

void init_properties() {
	Expression *x = init_var_expression("x", word);
	word_add_0_r = init_var_expression("word_add_0_r", init_forall_expression(x, 
		init_app_expression(init_app_expression(init_app_expression(
			eq, word), 
			init_app_expression(init_app_expression(word_add, init_app_expression(init_app_expression(option_some, word), x)), 
			init_app_expression(init_app_expression(option_some, word), init_app_expression(word_of_Z, Z0)))), 
			x)
	));
}

Expression *make_exec_test_1() {
	// prog := (cmd.seq (cmd.set "b" (expr.op bopname.add "a" 0)) (cmd.set "c" "b")).
	Expression *prog_sub1 = init_app_expression(init_app_expression(init_app_expression(expr_op, bopname_add), init_app_expression(expr_var, a_string)), init_app_expression(expr_literal, Z0));
	Expression *prog_sub2 = init_app_expression(init_app_expression(cmd_set, b_string), prog_sub1);
	Expression *prog_sub3 = init_app_expression(init_app_expression(cmd_set, c_string), init_app_expression(expr_var, b_string));
	Expression *prog = init_app_expression(init_app_expression(cmd_seq, prog_sub2), prog_sub3);

	Expression *t = init_var_expression("t", trace);
	Expression *m = init_var_expression("m", mem);
	Expression *a_val = init_var_expression("a_val", word);

	// l := (map.put string word) (map.empty string word) "a" a_val
	Expression *l_sub1 = init_app_expression(init_app_expression(partial_map_put, string), word);
	Expression *l_sub2 = init_app_expression(init_app_expression(partial_map_empty, string), word);
	Expression *l = init_app_expression(init_app_expression(init_app_expression(l_sub1, l_sub2), a_string), a_val);

	// post := (fun t m' l' => and (t' = t) and ((m' = m) (map.get l' "c" = Some a_val)))
	Expression *t_prime = init_var_expression("t'", trace);
	Expression *m_prime = init_var_expression("m'", mem);
	Expression *l_prime = init_var_expression("l'", locals);
	Expression *post_sub0 = init_app_expression(init_app_expression(init_app_expression(eq, trace), t_prime), t);
	Expression *post_sub1 = init_app_expression(init_app_expression(init_app_expression(eq, mem), m_prime), m);
	Expression *post_sub2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(partial_map_get, string), word), l_prime), c_string);
	Expression *post_sub3 = init_app_expression(init_app_expression(option_some, word), a_val);
	Expression *post_sub4 = init_app_expression(init_app_expression(init_app_expression(eq, init_app_expression(option, word)), post_sub2), post_sub3);
	Expression *post_sub5 = init_app_expression(init_app_expression(and, post_sub0), init_app_expression(init_app_expression(and, post_sub1), post_sub4));
	Expression *post = init_lambda_expression(t_prime, init_lambda_expression(m_prime, init_lambda_expression(l_prime, post_sub5)));

	// conclusion := exec t prog m l (fun t' m' l' => t' = t /\ (m' = m /\ map.get l' "c" = Some a_val))
	Expression *conclusion = init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(exec, t), prog), m), l), post);
	Expression *cmd_ok_theorem = init_forall_expression(t, init_forall_expression(m, init_forall_expression(a_val, conclusion)));
	return cmd_ok_theorem;
}


Expression *make_exec_test_2() {
	// forall t m l, exec t (cmd.seq cmd.skip cmd.skip) m l (fun t' m' l' => and (t' = t) (and (m' = m) (l' = l)))
	Expression *t = init_var_expression("t", trace);
	Expression *m = init_var_expression("m", mem);
	Expression *l = init_var_expression("l", locals);	
	Expression *cmd = init_app_expression(init_app_expression(cmd_seq, cmd_skip), cmd_skip);
	
	Expression *t_prime = init_var_expression("t'", trace);
	Expression *m_prime = init_var_expression("m'", mem);
	Expression *l_prime = init_var_expression("l'", locals);
	Expression *post_sub0 = init_app_expression(init_app_expression(init_app_expression(eq, trace), t_prime), t);
	Expression *post_sub1 = init_app_expression(init_app_expression(init_app_expression(eq, mem), m_prime), m);
	Expression *post_sub2 = init_app_expression(init_app_expression(init_app_expression(eq, locals), l_prime), l);
	Expression *post = init_lambda_expression(t_prime, init_lambda_expression(m_prime, init_lambda_expression(l_prime, 
		init_app_expression(init_app_expression(and, post_sub0), init_app_expression(init_app_expression(and, post_sub1), post_sub2)))));

	Expression *cmd_ok_theorem = init_forall_expression(t, init_forall_expression(m, init_forall_expression(l, 
		init_app_expression(init_app_expression(init_app_expression(init_app_expression(exec, cmd), m), l), post))));
	return cmd_ok_theorem;
}

Expression *make_exec_test_3() {
	// forall t m l, exec cmd.skip m l (fun m' l' => and (t' = t) (and (m' = m) (l' = l)))
	Expression *t = init_var_expression("t", trace);
	Expression *m = init_var_expression("m", mem);
	Expression *l = init_var_expression("l", locals);	
	Expression *cmd = cmd_skip;
	
	Expression *t_prime = init_var_expression("t'", trace);
	Expression *m_prime = init_var_expression("m'", mem);
	Expression *l_prime = init_var_expression("l'", locals);
	Expression *post_sub0 = init_app_expression(init_app_expression(init_app_expression(eq, trace), t_prime), t);
	Expression *post_sub1 = init_app_expression(init_app_expression(init_app_expression(eq, mem), m_prime), m);
	Expression *post_sub2 = init_app_expression(init_app_expression(init_app_expression(eq, locals), l_prime), l);
	Expression *post = init_lambda_expression(t_prime, init_lambda_expression(m_prime, init_lambda_expression(l_prime, 
		init_app_expression(init_app_expression(and, post_sub0), init_app_expression(init_app_expression(and, post_sub1), post_sub2)))));

	Expression *cmd_ok_theorem = init_forall_expression(t, init_forall_expression(m, init_forall_expression(l, 
		init_app_expression(init_app_expression(init_app_expression(init_app_expression(exec, cmd), m), l), post))));
	return cmd_ok_theorem;
}

Expression *get_cmd_type(Expression *cmd) {
	Expression *curr_cmd = cmd;
	while (curr_cmd->type == APP_EXPRESSION) {
		curr_cmd = curr_cmd->value.app.func;
	}
	return curr_cmd;
}

Expression *solve_skip(Expression *goal) {
	DoublyLinkedList *remaining_goals = apply(goal, exec_skip);
	if (dll_len(remaining_goals) != 1) {
		return NULL;
	}
	Expression *new_goal = dll_at(remaining_goals, 0)->data;
	dll_destroy(remaining_goals);

	normalize_hole_type(new_goal);

	return new_goal;
}

Expression *solve_seq(Expression *goal) {
	DoublyLinkedList *remaining_goals = apply(goal, exec_seq);
	if (dll_len(remaining_goals) != 1) {
		return NULL;
	}
	Expression *new_goal = dll_at(remaining_goals, 0)->data;
	dll_destroy(remaining_goals);

	normalize_hole_type(new_goal);

	return new_goal;
}

DoublyLinkedList *solve_and(Expression *goal) {
	DoublyLinkedList *remaining_goals = apply(goal, and_conj);
	return remaining_goals;
}

DoublyLinkedList *solve_eq(Expression *goal) {
	DoublyLinkedList *remaining_goals;

	remaining_goals = apply(goal, eq_refl);
	if (remaining_goals != NULL) return remaining_goals;

	RewrittenGoal *try2_1 = rewrite_transform(goal, word_add_0_r);
	RewrittenGoal *try2_2 = rewrite_transform(try2_1->new_goal, partial_map_get_put_same);
	remaining_goals = apply(try2_2->new_goal, eq_refl);
	if (remaining_goals != NULL) return remaining_goals;

	// all failed
	return NULL;
}

Expression *solve_set(Expression *goal) {
	DoublyLinkedList *remaining_goals = eapply(goal, exec_set);
	if (dll_len(remaining_goals) != 3) {
		return NULL;
	}

	Expression *eq_goal = (Expression *)dll_at(remaining_goals, 1)->data;
	normalize_hole_type(eq_goal);

	RewrittenGoal *result1 = rewrite_transform(eq_goal, binop_add_to_word_add);
	RewrittenGoal *result2  = rewrite_transform(result1->new_goal, partial_map_get_put_same);

	solve_eq(result2->new_goal);

	Expression *remaining_goal = dll_remove_tail(remaining_goals)->data;
	dll_destroy(remaining_goals);
	return remaining_goal;
}


Expression *_sym_solve(DoublyLinkedList *hypotheses, Expression *initial_goal) {

	DoublyLinkedList *goals_to_solve = dll_create();
	dll_insert_at_tail(goals_to_solve, dll_new_node(initial_goal));

	// TODO: This is a workaround to be able to retrieve the proof term after solving.
	Expression *temp = init_lambda_expression(init_var_expression("temp", init_type_expression()), initial_goal);

	while (dll_len(goals_to_solve) > 0) {
		Expression *goal = (Expression *)dll_remove_head(goals_to_solve)->data;
		Expression *goal_type = get_expression_type(goal);

		Expression *innermost = get_innermost_func(goal_type);
		if (innermost == exec) {
			Expression *post = goal_type->value.app.arg;
			Expression *locals = goal_type->value.app.func->value.app.arg;
			Expression *memory = goal_type->value.app.func->value.app.func->value.app.arg;
			Expression *cmd = goal_type->value.app.func->value.app.func->value.app.func->value.app.arg;
			Expression *cmd_type = get_cmd_type(cmd);

			if (cmd_type == cmd_skip) {
				dll_insert_at_tail(goals_to_solve, dll_new_node(solve_skip(goal)));
			} else if (cmd_type == cmd_seq) {
				dll_insert_at_tail(goals_to_solve, dll_new_node(solve_seq(goal)));
			} else if (cmd_type == cmd_set) {
				dll_insert_at_tail(goals_to_solve, dll_new_node(solve_set(goal)));
			}
		} else if (innermost == and) {
			DoublyLinkedList *new_goals = solve_and(goal);
			int n = dll_len(new_goals);
			for (int i = 0; i < n; i++) {
				dll_insert_at_tail(goals_to_solve, dll_new_node(dll_at(new_goals, i)->data));
			}
			dll_destroy(new_goals);
		} else if (innermost == eq) {
			DoublyLinkedList *new_goals = solve_eq(goal);
			int n = dll_len(new_goals);
			for (int i = 0; i < n; i++) {
				dll_insert_at_tail(goals_to_solve, dll_new_node(dll_at(new_goals, i)->data));
			}
			dll_destroy(new_goals);
		}
	}

	return temp->value.lambda.body;
}


Expression *straightline_solve(Expression *e) {
	Expression *proof = init_hole_expression("Goal", e, get_expression_context(e));
	IntrosReturn *intros_return = intros(proof);
	DoublyLinkedList *hypotheses = intros_return->hypotheses;
	Expression *unsolved_goal = intros_return->unsolved_goal;
	Expression *proof_term = _sym_solve(hypotheses, unsolved_goal);
	return intros_return->new_proof;
}

void run_symbolic() {
	init_word();
	init_option();
	init_positive();
	init_N();
	init_Z();
	init_string();
	init_list();
	init_partial_map();
	init_io_event();
	init_storage();
	init_bopname();
	init_expr();
	init_cmd();
	init_exec();
	init_properties();

	Expression *cmd_ok_theorem = make_exec_test_1();
	Expression *proof = straightline_solve(cmd_ok_theorem);

	printf("%s\n\n%s\n", stringify_expression2(cmd_ok_theorem), stringify_expression2(proof));
}