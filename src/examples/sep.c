#include "sep.h"

Expression *putmany = NULL;
Expression *disjoint = NULL;
Expression *split = NULL;
Expression *disjoint_comm = NULL;
Expression *disjoint_empty_l = NULL;
Expression *disjoint_empty_r = NULL;
Expression *putmany_comm = NULL;
Expression *putmany_empty_l = NULL;
Expression *putmany_empty_r = NULL;
Expression *emp = NULL;
Expression *sep = NULL;
Expression *ptsto = NULL;
Expression *read = NULL;
Expression *sep_cancel_r = NULL;
Expression *sep_cancel_l = NULL;
Expression *sep_comm = NULL;
Expression *sep_assoc = NULL;
Expression *sep_neutral_r = NULL;
Expression *sep_neutral_l = NULL;
Expression *sep_assoc4 = NULL;
Expression *sep_lift = NULL;
Expression *sep_cong = NULL;
Expression *sep_cancel = NULL;
Expression *sep_cancel_r_leaf = NULL;

Context *init_map_prop(Context *c) {
	// putmany : forall (A B : Type), (partial_map A B) -> (partial_map A B) -> (partial_map A B) 
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		putmany = init_var_expression("putmany", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(partial_map_A_B, 
					init_arrow_expression(partial_map_A_B, partial_map_A_B)))));
	}

	// disjoint : forall (A B : Type), (partial_map A B) -> (partial_map A B) -> Prop
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		disjoint = init_var_expression("disjoint", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(partial_map_A_B, 
					init_arrow_expression(partial_map_A_B, init_prop_expression())))));
	}

	// split : forall (A B : Type), (partial_map A B) -> (partial_map A B) -> (partial_map A B) -> Prop
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		split = init_var_expression("split", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(partial_map_A_B, 
					init_arrow_expression(partial_map_A_B, 
						init_arrow_expression(partial_map_A_B, init_prop_expression()))))));
	}

	return context_insert_n(c, 3, putmany, disjoint, split);
}

Context *init_disjoint_prop(Context *c) {
	// disjoint_comm : forall (A B: Type) (m1 m2 : partial_map A B), disjoint A B m1 m2 <-> disjoint A B m2 m1
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *m1 = init_var_expression("m1", partial_map_A_B);
		Expression *m2 = init_var_expression("m2", partial_map_A_B);
		Expression *disjoint_A_B_m1_m2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(disjoint, A), B), m1), m2);
		Expression *disjoint_A_B_m2_m1 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(disjoint, A), B), m2), m1);
		disjoint_comm = init_var_expression("disjoint_comm", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(m1, 
						init_forall_expression(m2, 
							iff(disjoint_A_B_m1_m2, disjoint_A_B_m2_m1))))));
	}
	
	// disjoint_empty_l : forall (A B : Type), forall (m : partial_map A B), disjoint A B (partial_map_empty A B) m
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *m = init_var_expression("m", init_app_expression(init_app_expression(partial_map, A), B));
		Expression *partial_map_empty_A_B = init_app_expression(init_app_expression(partial_map_empty, A), B);
		disjoint_empty_l = init_var_expression("disjoint_empty_l", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(m, 
						init_app_expression(init_app_expression(init_app_expression(init_app_expression(disjoint, A), B), partial_map_empty_A_B), m)))));
	}

	// disjoint_empty_r : forall (A B : Type), forall (m : partial_map A B), disjoint A B m (partial_map_empty A B)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *m = init_var_expression("m", init_app_expression(init_app_expression(partial_map, A), B));
		Expression *partial_map_empty_A_B = init_app_expression(init_app_expression(partial_map_empty, A), B);
		disjoint_empty_r = init_var_expression("disjoint_empty_r", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(m, 
						init_app_expression(init_app_expression(init_app_expression(init_app_expression(disjoint, A), B), m), partial_map_empty_A_B)))));
	}

	return context_insert_n(c, 3, disjoint_comm, disjoint_empty_l, disjoint_empty_r);
}

Context *init_putmany_prop(Context *c) {
	// putmany_comm : forall (A B : Type), forall (x y : partial_map A B), disjoint A B x y -> putmany A B x y = putmany A B y x
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *x = init_var_expression("x", partial_map_A_B);
		Expression *y = init_var_expression("y", partial_map_A_B);
		Expression *disjoint_A_B_x_y = init_app_expression(init_app_expression(init_app_expression(init_app_expression(disjoint, A), B), x), y);
		Expression *putmany_A_B_x_y = init_app_expression(init_app_expression(init_app_expression(init_app_expression(putmany, A), B), x), y);
		Expression *putmany_A_B_y_x = init_app_expression(init_app_expression(init_app_expression(init_app_expression(putmany, A), B), y), x);
		
		putmany_comm = init_var_expression("putmany_comm", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(x, 
						init_forall_expression(y, 
							init_arrow_expression(disjoint_A_B_x_y, 
								init_app_expression(init_app_expression(init_app_expression(eq, partial_map_A_B), putmany_A_B_x_y), putmany_A_B_y_x)))))));
	}

	// putmany_empty_l : forall (A B : Type), forall (m : partial_map A B), putmany A B (partial_map_empty A B) m = m
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *m = init_var_expression("m", partial_map_A_B);
		Expression *partial_map_empty_A_B = init_app_expression(init_app_expression(partial_map_empty, A), B);
		Expression *putmany_A_B_empty_m = init_app_expression(init_app_expression(init_app_expression(init_app_expression(putmany, A), B), partial_map_empty_A_B), m);
		putmany_empty_l = init_var_expression("putmany_empty_l", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(m, 
						init_app_expression(init_app_expression(init_app_expression(eq, partial_map_A_B), putmany_A_B_empty_m), m)))));
	}

	// putmany_empty_r : forall (A B : Type), forall (m : partial_map A B), putmany A B m (partial_map_empty A B) = m
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *m = init_var_expression("m", partial_map_A_B);
		Expression *partial_map_empty_A_B = init_app_expression(init_app_expression(partial_map_empty, A), B);
		Expression *putmany_A_B_empty_m = init_app_expression(init_app_expression(init_app_expression(init_app_expression(putmany, A), B), m), partial_map_empty_A_B);
		putmany_empty_r = init_var_expression("putmany_empty_r", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(m, 
						init_app_expression(init_app_expression(init_app_expression(eq, partial_map_A_B), putmany_A_B_empty_m), m)))));
	}
	return context_insert_n(c, 3, putmany_comm, putmany_empty_l, putmany_empty_r);
}

Context *init_sep(Context *c) {
	// emp : forall A B : Type, Prop -> partial_map A B -> Prop
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		emp = init_var_expression("emp", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(init_prop_expression(), init_arrow_expression(partial_map_A_B, init_prop_expression())))));
	}

	// sep : forall A B : Type, (partial_map A B -> Prop) -> (partial_map A B -> Prop) -> partial_map A B -> Prop 
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		sep = init_var_expression("sep", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(partial_map_A_B_prop, 
					init_arrow_expression(partial_map_A_B_prop, 
						init_arrow_expression(partial_map_A_B, init_prop_expression()))))));
	}

	// ptsto : forall A B : Type, A -> B -> partial_map A B -> Prop
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B_prop = init_arrow_expression(init_app_expression(init_app_expression(partial_map, A), B), init_prop_expression());
		ptsto = init_var_expression("ptsto", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(A, 
					init_arrow_expression(B, partial_map_A_B_prop)))));
	}

	// read : forall A B : Type, A -> (B -> (partial_map A B) -> Prop) -> Prop
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		read = init_var_expression("read", init_forall_expression(A, 
			init_forall_expression(B, 
				init_arrow_expression(A, 
					init_arrow_expression(init_arrow_expression(B, partial_map_A_B), init_prop_expression())))));
	}

	return context_insert_n(c, 4, emp, sep, ptsto, read);
}

Context *init_sep_prop(Context *c) {
	// sep_cancel_r : forall (A B: Type) (P Q R: partial_map A B -> Prop), iff1 P Q -> iff1 (sep A B P R) (sep A B Q R)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R = init_var_expression("R", partial_map_A_B_prop);
		
		Expression *iff1_P_Q = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), P), Q);
		Expression *sep_A_B_P_R = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), R);
		Expression *sep_A_B_Q_R = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), Q), R);
		Expression *iff1_P_Q_R = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_P_R), sep_A_B_Q_R);
		sep_cancel_r = init_var_expression("sep_cancel_r", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R, 
								init_arrow_expression(iff1_P_Q, iff1_P_Q_R)))))));
	}

	// sep_cancel_l : forall (A B: Type) (P Q R: partial_map A B -> Prop), iff1 Q R -> iff1 (sep A B P Q) (sep A B P R)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R = init_var_expression("R", partial_map_A_B_prop);

		Expression *iff1_P_Q = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), Q), R);
		Expression *sep_A_B_P_Q = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), Q);
		Expression *sep_A_B_P_R = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), R);
		Expression *iff1_P_Q_R = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_P_Q), sep_A_B_P_R);
		sep_cancel_l = init_var_expression("sep_cancel_l", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R, 
								init_arrow_expression(iff1_P_Q, iff1_P_Q_R)))))));
	}

	// sep_comm : forall (A B: Type) (p q: partial_map A B -> Prop), iff1 (sep A B p q) (sep A B q p)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *p = init_var_expression("p", partial_map_A_B_prop);
		Expression *q = init_var_expression("q", partial_map_A_B_prop);
		Expression *sep_A_B_p_q = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), p), q);
		Expression *sep_A_B_q_p = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), q), p);
		sep_comm = init_var_expression("sep_comm", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(p, 
						init_forall_expression(q, 
							init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_p_q), sep_A_B_q_p))))));
	}

	// sep_assoc : forall (A B: Type) (p q r: partial_map A B -> Prop), iff1 (sep A B (sep A B p q) r) (sep A B p (sep A B q r))
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *p = init_var_expression("p", partial_map_A_B_prop);
		Expression *q = init_var_expression("q", partial_map_A_B_prop);
		Expression *r = init_var_expression("r", partial_map_A_B_prop);
		
		Expression *sep_A_B_p_q = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), p), q);
		Expression *sep_A_B_q_r = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), q), r);
		
		Expression *sep_A_B_sep_p_q_r = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), sep_A_B_p_q), r);
		Expression *sep_A_B_p_sep_q_r = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), p), sep_A_B_q_r);

		sep_assoc = init_var_expression("sep_assoc", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(p, 
						init_forall_expression(q, 
							init_forall_expression(r, 
								init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_sep_p_q_r), sep_A_B_p_sep_q_r)))))));
	}

	// sep_neutral_r : forall (A B: Type) (p: partial_map A B -> Prop), iff1 (sep A B p (emp A B True)) p
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *p = init_var_expression("p", partial_map_A_B_prop);
		
		Expression *emp_A_B_True = init_app_expression(init_app_expression(init_app_expression(emp, A), B), True);
		Expression *sep_A_B_p_emp = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), p), emp_A_B_True);
		
		sep_neutral_r = init_var_expression("sep_neutral_r", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(p, 
						init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_p_emp), p)))));
	}

	// sep_neutral_l : forall (A B: Type) (p: partial_map A B -> Prop), iff1 (sep A B (emp A B True) p) p
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *p = init_var_expression("p", partial_map_A_B_prop);
		
		Expression *emp_A_B_True = init_app_expression(init_app_expression(init_app_expression(emp, A), B), True);
		Expression *sep_A_B_emp_p = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), emp_A_B_True), p);
		
		sep_neutral_l = init_var_expression("sep_neutral_l", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(p, 
						init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_emp_p), p)))));
	}

	// sep_assoc4 : forall (A B: Type) (P Q R S: partial_map A B -> Prop), iff1 (sep (sep P Q) (sep R S)) (sep P (sep R (sep Q S)))
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R = init_var_expression("R", partial_map_A_B_prop);
		Expression *S = init_var_expression("S", partial_map_A_B_prop);

		Expression *sep_A_B = init_app_expression(init_app_expression(sep, A), B);
		Expression *sep_A_B_P_Q = init_app_expression(init_app_expression(sep_A_B, P), Q);
		Expression *sep_A_B_R_S = init_app_expression(init_app_expression(sep_A_B, R), S);
		Expression *iff1_lhs = init_app_expression(init_app_expression(sep_A_B, sep_A_B_P_Q), sep_A_B_R_S);

		Expression *sep_A_B_Q_S = init_app_expression(init_app_expression(sep_A_B, Q), S);
		Expression *sep_A_B_R_sep_Q_S = init_app_expression(init_app_expression(sep_A_B, R), sep_A_B_Q_S);
		Expression *iff1_rhs = init_app_expression(init_app_expression(sep_A_B, P), sep_A_B_R_sep_Q_S);

		sep_assoc4 = init_var_expression("sep_assoc4", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R, 
								init_forall_expression(S, 
									init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), iff1_lhs), iff1_rhs))))))));
	}


	// sep_lift : forall (A B: Type) (P Q R: partial_map A B -> Prop), iff1 (sep P (sep Q R)) (sep Q (sep P R))
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *sep_A_B = init_app_expression(init_app_expression(sep, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R = init_var_expression("R", partial_map_A_B_prop);

		Expression *sep_A_B_Q_R = init_app_expression(init_app_expression(sep_A_B, Q), R);
		Expression *sep_A_B_P_R = init_app_expression(init_app_expression(sep_A_B, P), R);
		
		Expression *iff1_lhs = init_app_expression(init_app_expression(sep_A_B, P), sep_A_B_Q_R);
		Expression *iff1_rhs = init_app_expression(init_app_expression(sep_A_B, Q), sep_A_B_P_R);

		sep_lift = init_var_expression("sep_lift", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R, 
								init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), iff1_lhs), iff1_rhs)))))));		
	}

	// sep_cong: iff1 P Q -> iff1 R S -> iff1 (sep P R) (sep Q S)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R = init_var_expression("R", partial_map_A_B_prop);
		Expression *S = init_var_expression("S", partial_map_A_B_prop);

		Expression *iff1_P_Q = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), P), Q);
		Expression *iff1_R_S = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), R), S);
		
		Expression *sep_A_B_P_R = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), R);
		Expression *sep_A_B_Q_S = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), Q), S);

		sep_cong = init_var_expression("sep_cong", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R, 
								init_forall_expression(S, 
									init_arrow_expression(iff1_P_Q, 
										init_arrow_expression(iff1_R_S, 
											init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_A_B_P_R), sep_A_B_Q_S))))))))));
	}


	// sep_cancel : forall (A B: Type) (P Q R1 R2: partial_map A B -> Prop),
	//   iff1 Q (sep R1 R2) -> iff1 (sep R1 (sep P R2)) (sep P Q)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R1 = init_var_expression("R1", partial_map_A_B_prop);
		Expression *R2 = init_var_expression("R2", partial_map_A_B_prop);

		Expression *sep_R1_R2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), R1), R2);
		Expression *iff1_Q_R1_R2 = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), Q), sep_R1_R2);
		
		Expression *sep_P_Q = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), Q);
		Expression *sep_P_R2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), R2);
		Expression *sep_R1_sep_P_R2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), R1), sep_P_R2);
		Expression *iff1_sep_P_Q_R1_sep_P_R2 = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_R1_sep_P_R2), sep_P_Q);
		sep_cancel = init_var_expression("sep_cancel", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R1, 
								init_forall_expression(R2, 
									init_arrow_expression(iff1_Q_R1_R2, iff1_sep_P_Q_R1_sep_P_R2))))))));
	}

	// sep_cancel_r_leaf:  forall (A B: Type) (P Q R: partial_map A B -> Prop), iff1 R Q -> iff1 (sep R P) (sep P Q)
	{
		Expression *A = init_var_expression("A", init_type_expression());
		Expression *B = init_var_expression("B", init_type_expression());
		Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
		Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
		Expression *P = init_var_expression("P", partial_map_A_B_prop);
		Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
		Expression *R = init_var_expression("R", partial_map_A_B_prop);

		Expression *iff1_R_Q = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), R), Q);
		Expression *sep_R_P = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), R), P);
		Expression *sep_P_Q = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, A), B), P), Q);
		
		sep_cancel_r_leaf = init_var_expression("sep_cancel_r_leaf", 
			init_forall_expression(A, 
				init_forall_expression(B, 
					init_forall_expression(P, 
						init_forall_expression(Q, 
							init_forall_expression(R, 
								init_arrow_expression(iff1_R_Q, 
									init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), sep_R_P), sep_P_Q))))))));
	}

	return context_insert_n(c, 11, sep_cancel_r, sep_cancel_l, sep_comm, sep_assoc, sep_neutral_r, sep_neutral_l, sep_assoc4, sep_lift, sep_cong, sep_cancel, sep_cancel_r_leaf);
}

Expression *example_1() {
	// Returns a goal of the form:
	// forall (A B: Type) (P Q R S: partial_map A B -> Prop),
	// iff1 (partial_map A B) (sep P (sep Q (sep R S))) (sep (sep P Q) (sep R S))
	Expression *A = init_var_expression("A", init_type_expression());
	Expression *B = init_var_expression("B", init_type_expression());
	Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
	Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
	Expression *P = init_var_expression("P", partial_map_A_B_prop);
	Expression *Q = init_var_expression("Q", partial_map_A_B_prop);
	Expression *R = init_var_expression("R", partial_map_A_B_prop);
	Expression *S = init_var_expression("S", partial_map_A_B_prop);
	Expression *sep_A_B = init_app_expression(init_app_expression(sep, A), B);
	Expression *LHS = init_app_expression(init_app_expression(sep_A_B, P), 
		init_app_expression(init_app_expression(sep_A_B, Q), init_app_expression(init_app_expression(sep_A_B, R), S)));
	Expression *RHS = init_app_expression(init_app_expression(sep_A_B,
		init_app_expression(init_app_expression(sep_A_B, P), Q)), 
		init_app_expression(init_app_expression(sep_A_B, R), S));
	return init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), LHS), RHS);
}

Expression *example_2() {
	// Returns a goal of the form:
	// forall (A B: Type) (x: A) (y: B) (m: partial_map A B -> Prop),
	// 	exists (P: A) (Q: B) (R: partial_map A B -> Prop),
	// 	iff1 (partial_map A B) (sep m (ptsto x y)) (sep (ptsto P Q) R)
	Expression *A = init_var_expression("A", init_type_expression());
	Expression *B = init_var_expression("B", init_type_expression());
	Expression *partial_map_A_B = init_app_expression(init_app_expression(partial_map, A), B);
	Expression *partial_map_A_B_prop = init_arrow_expression(partial_map_A_B, init_prop_expression());
	Expression *x = init_var_expression("x", A);
	Expression *y = init_var_expression("y", B);
	Expression *m = init_var_expression("m", partial_map_A_B_prop);
	Expression *P = init_var_expression("P", A);
	Expression *Q = init_var_expression("Q", B);
	Expression *R = init_var_expression("R", partial_map_A_B_prop);

	Expression *sep_A_B = init_app_expression(init_app_expression(sep, A), B);
	Expression *ptsto_A_B = init_app_expression(init_app_expression(ptsto, A), B);

	Expression *lhs = init_app_expression(init_app_expression(sep_A_B, m), 
		init_app_expression(init_app_expression(ptsto_A_B, x), y));

	Expression *rhs = init_app_expression(init_app_expression(sep_A_B,
		init_app_expression(init_app_expression(ptsto_A_B, P), Q)), R);

	return init_app_expression(init_app_expression(ex, A), init_lambda_expression(P, 
		init_app_expression(init_app_expression(ex, B), init_lambda_expression(Q, 
			init_app_expression(init_app_expression(ex, partial_map_A_B_prop), init_lambda_expression(R,
				init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), lhs), rhs)))))));
}

Expression *get_sep_lhs(Expression *adj) {
	// Given an adjunction of the form:
	// 	(sep A B P Q)
	// returns the LHS, i.e., P.
	if (adj->type != APP_EXPRESSION || get_innermost_func(adj) != sep) {
		fprintf(stderr, "Error: Expected innermost func to be sep.\n");
		exit(EXIT_FAILURE);
	}
	return adj->value.app.func->value.app.arg;
}

Expression *get_sep_rhs(Expression *adj) {
	// Given an adjunction of the form:
	// 	(sep A B P Q)
	// returns the RHS, i.e., Q.
	if (adj->type != APP_EXPRESSION || get_innermost_func(adj) != sep) {
		fprintf(stderr, "Error: Expected innermost func to be sep.\n");
		exit(EXIT_FAILURE);
	}
	return adj->value.app.arg;
}

Expression *get_sep_prefix(Expression *adj) {
	// Given an adjunction of the form:
	// 	(sep A B P Q)
	// returns the sep prefix, i.e., (sep A B).
	if (adj->type != APP_EXPRESSION || get_innermost_func(adj) != sep) {
		fprintf(stderr, "Error: Expected innermost func to be sep.\n");
		exit(EXIT_FAILURE);
	}
	return adj->value.app.func->value.app.func;
}

Expression *get_sep_A(Expression *adj) {
	// Given an adjunction of the form:
	// 	(sep A B P Q)
	// returns A.
	if (adj->type != APP_EXPRESSION || get_innermost_func(adj) != sep) {
		fprintf(stderr, "Error: Expected innermost func to be sep.\n");
		exit(EXIT_FAILURE);
	}
	return adj->value.app.func->value.app.func->value.app.func->value.app.arg;
}

Expression *get_sep_B(Expression *adj) {
	// Given an adjunction of the form:
	// 	(sep A B P Q)
	// returns B.
	if (adj->type != APP_EXPRESSION || get_innermost_func(adj) != sep) {
		fprintf(stderr, "Error: Expected innermost func to be sep.\n");
		exit(EXIT_FAILURE);
	}
	return adj->value.app.func->value.app.func->value.app.arg;
}

bool is_leaf(Expression *expr) {
	return get_innermost_func(expr) != sep;
}

FlattenProof *init_flatten_proof(Expression *rewritten_expr, Expression *equality_proof) {
	FlattenProof *fp = malloc(sizeof(FlattenProof));
	if (!fp) {
		fprintf(stderr, "Error: Memory allocation failed for FlattenProof.\n");
		exit(EXIT_FAILURE);
	}
	fp->rewritten_expr = rewritten_expr;
	fp->equality_proof = equality_proof;
	return fp;
}

FlattenProof *free_flatten_proof(FlattenProof *fp) {
	if (fp) {
		free(fp->rewritten_expr);
		free(fp->equality_proof);
		free(fp);
	}
	return NULL;
}

FlattenProof *flatten_sep_adjunction(Expression *adj, Context *ctx) {
	// Given an arbitrary adjunction of sep expressions,
	// returns an equivalent adjunction where the sep expressions are flattened.

	Expression *partial_map_A_B = get_expression_type(adj)->value.forall.bound_variable->value.var.type;

	if (is_leaf(adj)) {
		return init_flatten_proof(adj, init_app_expression(init_app_expression(iff1_refl, partial_map_A_B), adj));
	} 

	Expression *sep_A = get_sep_A(adj);
	Expression *sep_B = get_sep_B(adj);
	Expression *lhs = get_sep_lhs(adj);
	Expression *rhs = get_sep_rhs(adj);

	if (is_leaf(lhs)) {
		// adj has form sep lhs rhs where lhs is a leaf.
		FlattenProof *new_rhs = flatten_sep_adjunction(rhs, ctx);
		Expression *new_adj = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), lhs), new_rhs->rewritten_expr);

		// need a proof that iff1 (sep lhs rhs) (sep lhs new_rhs) given a proof of iff1 rhs.
		// sep_cancel_l A B lhs rhs new_rhs->rewritten_expr new_rhs->equality_proof
		Expression *equality_proof = init_app_expression(init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(sep_cancel_l, sep_A), sep_B), lhs), rhs), new_rhs->rewritten_expr), new_rhs->equality_proof);

		return init_flatten_proof(new_adj, equality_proof);
	} else if (is_leaf(rhs)) {
		// adj has form sep lhs rhs where rhs is a leaf.
		FlattenProof *new_lhs = flatten_sep_adjunction(lhs, ctx);
		Expression *new_adj = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), new_lhs->rewritten_expr), rhs);

		// need a proof that iff1 (sep lhs rhs) (sep new_lhs rhs) given a proof of iff1 lhs.
		// sep_cancel_r A B lhs rhs new_lhs->rewritten_expr new_lhs->equality_proof
		Expression *equality_proof = init_app_expression(init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(sep_cancel_r, sep_A), sep_B), lhs), rhs), new_lhs->rewritten_expr), new_lhs->equality_proof);


		Expression *flipped_adj = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), rhs), new_lhs->rewritten_expr);
		// now need a proof that we can flip the adjunction:
		// sep_comm A B new_lhs->rewritten_expr rhs
		Expression *flipped_equality_proof = init_app_expression(init_app_expression(init_app_expression(sep_comm, sep_A), sep_B), new_lhs->rewritten_expr);
		
		// finally, a proof that the flipped adjunction is equal to the original adjunction:
		// iff1_trans partial_map_A_B adj new_adj flipped_adj equality_proof flipped_equality_proof
		Expression *final_equality_proof = init_app_expression(init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), adj), new_adj), flipped_adj), equality_proof), flipped_equality_proof);
		return init_flatten_proof(flipped_adj, final_equality_proof);
	} else {
		// adj has form sep lhs rhs where both lhs and rhs are not leaves.
		FlattenProof *new_lhs = flatten_sep_adjunction(lhs, ctx);
		FlattenProof *new_rhs = flatten_sep_adjunction(rhs, ctx);

		Expression *mid = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), new_lhs->rewritten_expr), new_rhs->rewritten_expr);
		Expression *adj_to_mid = init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(
			sep_cong, sep_A), sep_B), lhs), new_lhs->rewritten_expr), rhs), new_rhs->rewritten_expr), new_lhs->equality_proof), new_rhs->equality_proof);

		Expression *new_lhs_1 = get_sep_lhs(new_lhs->rewritten_expr);
		Expression *new_lhs_2 = get_sep_rhs(new_lhs->rewritten_expr);
		Expression *new_rhs_1 = get_sep_lhs(new_rhs->rewritten_expr);
		Expression *new_rhs_2 = get_sep_rhs(new_rhs->rewritten_expr);
		Expression *inner_1 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), new_lhs_2), new_rhs_2);
		Expression *inner_2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), new_rhs_1), inner_1);
		Expression *mid2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep, sep_A), sep_B), new_lhs_1), inner_2);

		// sep_assoc4 A B new_lhs_1 new_lhs_2 new_rhs_1 new_rhs_2
		Expression *mid_to_mid2 = init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(
			sep_assoc4, sep_A), sep_B), new_lhs_1), new_lhs_2), new_rhs_1), new_rhs_2);

		// We've reduced the problem by at least 2 leaves, so we call flatten_sep_adjunction recursively on the rearranged expression.
		FlattenProof *reduced_proof = flatten_sep_adjunction(mid2, ctx);
		Expression *final = reduced_proof->rewritten_expr;
		Expression *mid2_to_final = reduced_proof->equality_proof;

		// Now we need to combine the proofs...
		// iff1_trans adj mid final orig_to_mid (iff1_trans mid mid2 final mid_to_mid2 mid2_to_final)
		Expression *mid_to_final = init_app_expression(init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), mid), mid2), final), mid_to_mid2), mid2_to_final);
		Expression *adj_to_final = init_app_expression(init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), adj), mid), final), adj_to_mid), mid_to_final);
		
		return init_flatten_proof(final, adj_to_final);
	}
}


Expression *flatten(Expression *goal) {
	// Given a goal (hole) of the form:
	// 	iff1 (partial_map A B) LHS RHS
	// returns an equivalent goal where LHS and RHS are "flattened", i.e., 
	// it is of the form:
	// 	sep M1 (sep M2 (sep M3 ...)) where each Mi is leaf/not a sep expression.

	Expression *goal_type = get_expression_type(goal);
	if (goal_type->type != APP_EXPRESSION || get_innermost_func(goal_type) != iff1) {
		fprintf(stderr, "Error: Expected goal to be of type iff1.\n");
		exit(EXIT_FAILURE);
	}

	Expression *rest = goal_type->value.app.func;
	Expression *LHS = rest->value.app.arg;
	Expression *RHS = goal_type->value.app.arg;
	FlattenProof *fp_l = flatten_sep_adjunction(LHS, get_expression_context(goal));
	FlattenProof *fp_r = flatten_sep_adjunction(RHS, get_expression_context(goal));

	Expression *partial_map_A_B = rest->value.app.func->value.app.arg;
	Expression *rewritten_expr = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), fp_l->rewritten_expr), fp_r->rewritten_expr);

	// iff1_cong partial_map_A_B LHS fp_l->rewritten_expr RHS fp_r->rewritten_expr fp_l->equality_proof fp_r->equality_proof ?hole 
	Expression *hole = init_hole_expression("Goal", rewritten_expr, get_expression_context(goal));
	Expression *proof = init_app_expression(init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(init_app_expression(init_app_expression(init_app_expression(
			iff1_cong, partial_map_A_B), LHS), fp_l->rewritten_expr), RHS), fp_r->rewritten_expr), fp_l->equality_proof), fp_r->equality_proof), hole);
	
	if (can_fill(goal, proof)) {
		fillHole(goal, proof);
		return hole;
	} 
	
	printf("Error: Flattening failed.\n");
	exit(EXIT_FAILURE);
}

FlattenProof *pull_to_front(Expression *v, Expression *adj) {
	// Given a right-associated adjunction of the form:
	// 	(sep A B P Q)
	// where the left hand of each sep is a leave, returns
	// an equivalent adjunction where the variable v is pulled to the top-most level.
	if (adj->type != APP_EXPRESSION || get_innermost_func(adj) != sep) {
		fprintf(stderr, "Error: Expected innermost func to be sep.\n");
		exit(EXIT_FAILURE);
	}
	
	Expression *sep_A_B = get_sep_prefix(adj);
	Expression *lhs = get_sep_lhs(adj);
	Expression *rhs = get_sep_rhs(adj);
	Expression *partial_map_A_B = get_expression_type(adj)->value.forall.bound_variable->value.var.type;
	Expression *sep_A = get_sep_A(adj);
	Expression *sep_B = get_sep_B(adj);

	if (congruence2(lhs, v)) {
		// If lhs is already v, we can just return the adjunction as is.
		return init_flatten_proof(adj, init_app_expression(init_app_expression(iff1_refl, partial_map_A_B), adj));
	} else if (congruence2(rhs, v)) {
		// If rhs is v, we can just swap lhs and rhs.
		Expression *new_adj = init_app_expression(init_app_expression(sep_A_B, rhs), lhs);
		return init_flatten_proof(new_adj, init_app_expression(init_app_expression(init_app_expression(init_app_expression(sep_comm, sep_A), sep_B), lhs), rhs));
	}

	Expression *curr_leaf = lhs;

	// If neither lhs nor rhs is v, we need to pull v out of the rhs. 
	FlattenProof *rhs_rearranged = pull_to_front(v, rhs);
	Expression *rhs__new_rhs = rhs_rearranged->equality_proof;
	Expression *new_rhs = rhs_rearranged->rewritten_expr;
	// At this point, rhs_rearranged->rewritten_expr is of the form:
	// 	(sep A B v Q)
	// So now we can just pull v out of this and prove the equivalence.
	Expression *mid = init_app_expression(init_app_expression(sep_A_B, curr_leaf), new_rhs);
	Expression *adj_to_mid = init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(init_app_expression(
		sep_cancel_l, sep_A), sep_B), curr_leaf), rhs), new_rhs), rhs__new_rhs);

	Expression *swap_leaf = get_sep_lhs(rhs_rearranged->rewritten_expr);
	Expression *rest_of_rhs = get_sep_rhs(rhs_rearranged->rewritten_expr);
	
	Expression *final = init_app_expression(init_app_expression(sep_A_B, swap_leaf), init_app_expression(init_app_expression(sep_A_B, curr_leaf), rest_of_rhs));
	Expression *mid_to_final = init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(init_app_expression(sep_lift, sep_A), sep_B), curr_leaf), swap_leaf), rest_of_rhs);
	
	Expression *full_proof = init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), adj), mid), final), adj_to_mid), mid_to_final);
	return init_flatten_proof(final, full_proof);
}

Expression *reorder(Expression *goal) {
	DoublyLinkedList *no_holes = dll_create();
	DoublyLinkedList *with_holes = dll_create();
	DoublyLinkedList *are_holes = dll_create();

	Expression *goal_type = get_expression_type(goal);
	if (goal_type->type != APP_EXPRESSION || get_innermost_func(goal_type) != iff1) {
		fprintf(stderr, "Error: Expected goal to be of type iff1.\n");
		exit(EXIT_FAILURE);
	}

	Expression *rest = goal_type->value.app.func;
	Expression *partial_map_A_B = rest->value.app.func->value.app.arg;
	Expression *original_lhs = rest->value.app.arg;
	Expression *original_rhs = goal_type->value.app.arg;

	Expression *curr_clause = goal_type->value.app.arg;
		while (true) {
		Expression *v = get_sep_lhs(curr_clause);
		Expression *rhs = get_sep_rhs(curr_clause);

		if (is_hole(v)) {
			dll_insert_at_tail(are_holes, dll_new_node(v));
		} else if (has_holes(v)) {
			dll_insert_at_tail(with_holes, dll_new_node(v));
		} else {
			dll_insert_at_tail(no_holes, dll_new_node(v));
		}

		if (is_leaf(rhs)) {
			if (is_hole(rhs)) {
				dll_insert_at_tail(are_holes, dll_new_node(rhs));
			} else if (has_holes(rhs)) {
				dll_insert_at_tail(with_holes, dll_new_node(rhs));
			} else {
				dll_insert_at_tail(no_holes, dll_new_node(rhs));
			}
			break;
		}

		curr_clause = get_sep_rhs(rhs);
	}

	Expression *current_rhs = original_rhs;
	Expression *original_to_current_rhs_proof = init_app_expression(
		init_app_expression(iff1_refl, partial_map_A_B), original_rhs);

	for (int i = 0; i < dll_len(are_holes); i++) {
		Expression *v = dll_at(are_holes, i)->data;
		FlattenProof *fp = pull_to_front(v, current_rhs);
		original_to_current_rhs_proof = init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(
				init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), original_rhs), current_rhs), fp->rewritten_expr), original_to_current_rhs_proof), fp->equality_proof);
		current_rhs = fp->rewritten_expr;
	}

	for (int i = 0; i < dll_len(with_holes); i++) {
		Expression *v = dll_at(with_holes, i)->data;
		FlattenProof *fp = pull_to_front(v, current_rhs);
		original_to_current_rhs_proof = init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(
				init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), original_rhs), current_rhs), fp->rewritten_expr), original_to_current_rhs_proof), fp->equality_proof);
		current_rhs = fp->rewritten_expr;
	}

	for (int i = 0; i < dll_len(no_holes); i++) {
		Expression *v = dll_at(no_holes, i)->data;
		FlattenProof *fp = pull_to_front(v, current_rhs);
		original_to_current_rhs_proof = init_app_expression(
			init_app_expression(init_app_expression(init_app_expression(
				init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), original_rhs), current_rhs), fp->rewritten_expr), original_to_current_rhs_proof), fp->equality_proof);
		current_rhs = fp->rewritten_expr;
	}

	Expression *new_goal_type = init_app_expression(init_app_expression(init_app_expression(iff1, partial_map_A_B), original_lhs), current_rhs);
	Expression *new_goal = init_hole_expression("Goal", new_goal_type, get_expression_context(goal));

	Expression *current_to_original_rhs_proof = init_app_expression(init_app_expression(init_app_expression(
		init_app_expression(iff1_sym, partial_map_A_B), original_rhs), current_rhs), original_to_current_rhs_proof);
	
	Expression *proof_of_reorder = init_app_expression(
		init_app_expression(init_app_expression(init_app_expression(
			init_app_expression(init_app_expression(iff1_trans, partial_map_A_B), original_lhs), current_rhs), original_rhs), new_goal), current_to_original_rhs_proof);

	if (can_fill(goal, proof_of_reorder)) {
		fillHole(goal, proof_of_reorder);
	} else {
		fprintf(stderr, "Error: Cannot fill the goal with the proof of reorder.\n");
		exit(EXIT_FAILURE);
	}

	return new_goal;
}

void new_cancel(Expression *goal) {
	Expression *curr = goal;
	while (true) {
		printf("Current goal: %s\n", stringify_expression2(get_expression_type(curr)));
		DoublyLinkedList *result = apply(curr, sep_cancel_l);
		if (result && dll_len(result) == 1) {
			printf(" - Applying sep_cancel_l...\n");
			curr = dll_at(result, 0)->data;
			continue;
		} 

		result = apply(curr, sep_cancel);
		if (result && dll_len(result) == 1) {
			printf(" - Applying sep_cancel...\n");
			curr = dll_at(result, 0)->data;
			continue;
			
		}

		result = apply(curr, sep_cancel_r_leaf);
		if (result && dll_len(result) == 1) {
			printf(" - Applying sep_cancel_r_leaf...\n");
			curr = dll_at(result, 0)->data;
			continue;
		}

		result = apply(curr, iff1_refl);
		if (result && dll_len(result) == 0) {
			printf(" - Applying iff1_refl...\n");
			printf("Successfully cancelled.\n");
			return;
		}

		printf("Unable to cancel further. Final goal: %s\n", stringify_expression2(get_expression_type(curr)));
		exit(EXIT_FAILURE);
	}
}

void solve_sep(Expression *goal) {
	// Given a goal of the form:
	// 	iff1 (partial_map A B) LHS RHS
	// where LHS and RHS are sep expressions, this function attempts to solve it.
	// It will reorder the goal, flatten the sep expressions, and apply cancellation rules.
	
	printf("Solving goal: %s\n", stringify_expression2(get_expression_type(goal)));

	Expression *curr_goal = goal;
	Expression *innermost = get_innermost_func(get_expression_type(curr_goal));
	while (innermost != iff1) {
		if (innermost == ex) {
			printf(" -- eexist...\n");
			curr_goal = eexists(curr_goal);
		}

		innermost = get_innermost_func(get_expression_type(curr_goal));
	}


	Expression *flattened = reorder(flatten(curr_goal));
	printf("Reordered and flattened goal: %s : %s\n", stringify_expression2(flattened), stringify_expression2(get_expression_type(flattened)));
	new_cancel(flattened);
}

void run_sep(void) {
	Context *c = std_lib_ctx;

 	c = init_map_prop(c);
	c = init_disjoint_prop(c);
	c = init_putmany_prop(c);
	c = init_sep(c);
	c = init_sep_prop(c);

	printf("Initialized SEP library with %d expressions.\n", c->length);

	Expression *example = example_1();
	Context *hole_ctx = context_add(c, get_expression_context(example));
	Expression *goal = init_hole_expression("Goal", example, hole_ctx);
	// TODO: This is a workaround to be able to retrieve the proof term after solving.
	Expression *temp = init_lambda_expression(init_var_expression("temp", init_type_expression()), goal);
	
	solve_sep(goal);

	exit(EXIT_SUCCESS);
}