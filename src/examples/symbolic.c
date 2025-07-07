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
Expression *not_eq_string_b_a = NULL;

Expression *list = NULL;
Expression *list_nil = NULL;
Expression *list_cons = NULL;

Expression *word = NULL;
Expression *word_of_Z = NULL;
Expression *word_add = NULL;
Expression *word_sub = NULL;
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
Expression *binop_add_to_word_sub = NULL;

Expression *expr = NULL;
Expression *expr_literal = NULL;
Expression *expr_var = NULL;
Expression *expr_op = NULL;
Expression *expr_ite = NULL;
Expression *eval_expr_ref = NULL;
Expression *eval_expr = NULL;

Expression *cmd = NULL;
Expression *cmd_skip = NULL;
Expression *cmd_set = NULL;
Expression *cmd_unset = NULL;
Expression *cmd_cond = NULL;
Expression *cmd_seq = NULL;
Expression *cmd_input = NULL;
Expression *cmd_output = NULL;

Expression *IOEvent = NULL;
Expression *IOEvent_IN = NULL;
Expression *IOEvent_OUT = NULL;

Expression *exec = NULL;
Expression *exec_skip = NULL;
Expression *exec_set = NULL;
Expression *exec_seq = NULL;
Expression *exec_input = NULL;

Expression *word_add_0_r = NULL;
Expression *word_add_sub_cancel = NULL;

Context *init_positive(Context *c) {
    positive = init_var_expression("positive", init_type_expression());

    xI = init_var_expression("xI", init_arrow_expression(positive, positive));
    xO = init_var_expression("xO", init_arrow_expression(positive, positive));
    xH = init_var_expression("xH", positive);

    return context_insert_n(c, 4, positive, xI, xO, xH);
}

Context *init_N(Context *c) {
    N = init_var_expression("N", init_type_expression());
    N0 = init_var_expression("N0", N);
    Npos = init_var_expression("Npos", init_arrow_expression(positive, N));

    return context_insert_n(c, 3, N, N0, Npos);
}

Context *init_Z(Context *c) {
    Z = init_var_expression("Z", init_type_expression());
    Z0 = init_var_expression("Z0", Z);
    Zpos = init_var_expression("Zpos", init_arrow_expression(positive, Z));
    Zneg = init_var_expression("Zneg", init_arrow_expression(positive, Z));

    return context_insert_n(c, 4, Z, Z0, Zpos, Zneg);
}

Context *init_string(Context *c) {
    string = init_var_expression("string", init_type_expression());
    a_string = init_var_expression("a", string);
    b_string = init_var_expression("b", string);
    c_string = init_var_expression("c", string);
    not_eq_string_b_a = init_var_expression(
        "not_eq_string_b_a",
        init_app_expression(
            not,
            init_app_expression(
                init_app_expression(init_app_expression(eq, string), b_string),
                a_string)));

    return context_insert_n(c, 5, string, a_string, b_string, c_string,
                            not_eq_string_b_a);
}

Context *init_list(Context *c) {
    list = init_var_expression(
        "list",
        init_arrow_expression(init_type_expression(), init_type_expression()));
    Expression *bind1 = init_var_expression("A", init_type_expression());
    list_nil = init_var_expression(
        "list_nil",
        init_forall_expression(bind1, init_app_expression(list, bind1)));

    Expression *bind2 = init_var_expression("A", init_type_expression());
    list_cons = init_var_expression(
        "list_cons",
        init_forall_expression(
            bind2, init_arrow_expression(
                       bind2, init_arrow_expression(
                                  init_app_expression(list, bind2),
                                  init_app_expression(list, bind2)))));

    return context_insert_n(c, 3, list, list_nil, list_cons);
}

Context *init_word(Context *c) {
    word = init_var_expression("word", init_type_expression());

    return context_insert_n(c, 1, word);
}

Context *init_storage(Context *c) {
    word_of_Z =
        init_var_expression("word_of_Z", init_arrow_expression(Z, word));
    word_add = init_var_expression(
        "word_add",
        init_arrow_expression(
            init_app_expression(option, word),
            init_arrow_expression(init_app_expression(option, word), word)));
    word_sub = init_var_expression(
        "word_sub",
        init_arrow_expression(
            init_app_expression(option, word),
            init_arrow_expression(init_app_expression(option, word), word)));
    byte = init_var_expression("byte", init_type_expression());
    trace = init_app_expression(list, IOEvent);
    mem = init_app_expression(init_app_expression(partial_map, word), byte);
    locals =
        init_app_expression(init_app_expression(partial_map, string), word);

    return context_insert_n(c, 4, word_of_Z, word_add, word_sub, byte);
}

Context *init_bopname(Context *c) {
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
    interp_binop = init_var_expression(
        "interp_binop",
        init_arrow_expression(
            bopname, init_arrow_expression(
                         init_app_expression(option, word),
                         init_arrow_expression(
                             init_app_expression(option, word), word))));

    {
        Expression *w1 =
            init_var_expression("w1", init_app_expression(option, word));
        Expression *w2 =
            init_var_expression("w2", init_app_expression(option, word));
        // eq (option word) (interp_binop bopname_add w1 w2) (word_add w1 w2)
        binop_add_to_word_add = init_var_expression(
            "binop_add_to_word_add",
            init_forall_expression(
                w1, init_forall_expression(
                        w2, init_app_expression(
                                init_app_expression(
                                    init_app_expression(eq, word),
                                    init_app_expression(
                                        init_app_expression(
                                            init_app_expression(interp_binop,
                                                                bopname_add),
                                            w1),
                                        w2)),
                                init_app_expression(
                                    init_app_expression(word_add, w1), w2)))));
    }

    {
        Expression *w1 =
            init_var_expression("w1", init_app_expression(option, word));
        Expression *w2 =
            init_var_expression("w2", init_app_expression(option, word));
        // eq (option word) (interp_binop bopname_sub w1 w2) (word_sub w1 w2)
        binop_add_to_word_sub = init_var_expression(
            "binop_add_to_word_sub",
            init_forall_expression(
                w1, init_forall_expression(
                        w2, init_app_expression(
                                init_app_expression(
                                    init_app_expression(eq, word),
                                    init_app_expression(
                                        init_app_expression(
                                            init_app_expression(interp_binop,
                                                                bopname_sub),
                                            w1),
                                        w2)),
                                init_app_expression(
                                    init_app_expression(word_sub, w1), w2)))));
    }
    return context_insert_n(c, 6, bopname, bopname_add, bopname_sub,
                            interp_binop, binop_add_to_word_add,
                            binop_add_to_word_sub);
}

Context *init_expr(Context *c) {
    expr = init_var_expression("expr", init_type_expression());
    expr_literal = init_var_expression(
        "expr_literal",
        init_forall_expression(init_var_expression("v", Z), expr));
    expr_var = init_var_expression(
        "expr_var",
        init_forall_expression(init_var_expression("x", string), expr));
    expr_op = init_var_expression(
        "expr_op", init_forall_expression(
                       init_var_expression("op", bopname),
                       init_forall_expression(
                           init_var_expression("e1", expr),
                           init_forall_expression(
                               init_var_expression("e2", expr), expr))));
    expr_ite = init_var_expression(
        "expr_ite", init_forall_expression(
                        init_var_expression("c", expr),
                        init_forall_expression(
                            init_var_expression("e1", expr),
                            init_forall_expression(
                                init_var_expression("e2", expr), expr))));

    Expression *eval_expr_m = init_var_expression("m", mem);
    Expression *eval_expr_l = init_var_expression("l", locals);
    eval_expr_ref = init_var_expression(
        "eval_expr",
        init_arrow_expression(expr, init_app_expression(option, word)));
    Expression *eval_expr_e = init_var_expression("e", expr);
    Expression *eval_expr_literal_v = init_var_expression("v", Z);
    Expression *eval_expr_var_x = init_var_expression("x", string);
    Expression *eval_expr_op_op = init_var_expression("op", bopname);
    Expression *eval_expr_op_e1 = init_var_expression("e1", expr);
    Expression *eval_expr_op_e2 = init_var_expression("e2", expr);
    Expression *eval_expr_fix = init_fix_expression(
        eval_expr_ref, eval_expr_e,
        init_match_expr_expression(
            eval_expr_e,
            init_lambda_expression(
                eval_expr_literal_v,
                init_app_expression(expr_literal, eval_expr_literal_v)),
            init_lambda_expression(
                eval_expr_literal_v,
                init_app_expression(
                    init_app_expression(option_some, word),
                    init_app_expression(word_of_Z, eval_expr_literal_v))),
            init_lambda_expression(
                eval_expr_var_x,
                init_app_expression(expr_var, eval_expr_var_x)),
            init_lambda_expression(
                eval_expr_var_x,
                init_app_expression(
                    init_app_expression(
                        init_app_expression(
                            init_app_expression(partial_map_get, string), word),
                        eval_expr_l),
                    eval_expr_var_x)),
            init_lambda_expression(
                eval_expr_op_op,
                init_lambda_expression(
                    eval_expr_op_e1,
                    init_lambda_expression(
                        eval_expr_op_e2,
                        init_app_expression(
                            init_app_expression(
                                init_app_expression(expr_op, eval_expr_op_op),
                                eval_expr_op_e1),
                            eval_expr_op_e2)))),
            init_lambda_expression(
                eval_expr_op_op,
                init_lambda_expression(
                    eval_expr_op_e1,
                    init_lambda_expression(
                        eval_expr_op_e2,
                        init_app_expression(
                            init_app_expression(option_some, word),
                            init_app_expression(
                                init_app_expression(
                                    init_app_expression(interp_binop,
                                                        eval_expr_op_op),
                                    init_app_expression(eval_expr_ref,
                                                        eval_expr_op_e1)),
                                init_app_expression(eval_expr_ref,
                                                    eval_expr_op_e2)))))),
            init_app_expression(option, word)));
    eval_expr = init_lambda_expression(
        eval_expr_m, init_lambda_expression(eval_expr_l, eval_expr_fix));

    return context_insert_n(c, 6, expr, expr_literal, expr_var, expr_op,
                            expr_ite, eval_expr_ref);
}

Context *init_cmd(Context *c) {
    cmd = init_var_expression("cmd", init_type_expression());
    cmd_skip = init_var_expression("cmd_skip", cmd);
    cmd_set = init_var_expression(
        "cmd_set",
        init_forall_expression(
            init_var_expression("lhs", string),
            init_forall_expression(init_var_expression("rhs", expr), cmd)));
    cmd_unset = init_var_expression(
        "cmd_unset",
        init_forall_expression(init_var_expression("lhs", string), cmd));
    cmd_cond = init_var_expression(
        "cmd_cond",
        init_forall_expression(
            init_var_expression("condition", expr),
            init_forall_expression(
                init_var_expression("nonzero_branch", cmd),
                init_forall_expression(init_var_expression("zero_branch", cmd),
                                       cmd))));
    cmd_seq = init_var_expression(
        "cmd_seq",
        init_forall_expression(
            init_var_expression("s1", cmd),
            init_forall_expression(init_var_expression("s2", cmd), cmd)));
    cmd_input = init_var_expression(
        "cmd_input",
        init_forall_expression(init_var_expression("lhs", string), cmd));
    cmd_output = init_var_expression(
        "cmd_output",
        init_forall_expression(init_var_expression("arg", string), cmd));

    return context_insert_n(c, 8, cmd, cmd_skip, cmd_set, cmd_unset, cmd_cond,
                            cmd_seq, cmd_input, cmd_output);
}

Context *init_io_event(Context *c) {
    IOEvent = init_var_expression("IOEvent", init_type_expression());
    IOEvent_IN =
        init_var_expression("IOEvent_IN", init_arrow_expression(word, IOEvent));
    IOEvent_OUT = init_var_expression("IOEvent_OUT",
                                      init_arrow_expression(word, IOEvent));

    return context_insert_n(c, 3, IOEvent, IOEvent_IN, IOEvent_OUT);
}

Context *init_exec(Context *c) {
    // exec
    exec = init_var_expression(
        "exec",
        init_arrow_expression(
            cmd,
            init_arrow_expression(
                trace,
                init_arrow_expression(
                    mem,
                    init_arrow_expression(
                        locals,
                        init_arrow_expression(
                            init_arrow_expression(
                                trace,
                                init_arrow_expression(
                                    mem, init_arrow_expression(
                                             locals, init_prop_expression()))),
                            init_prop_expression()))))));

    // exec_skip
    Expression *t1 = init_var_expression("t", trace);
    Expression *m1 = init_var_expression("m", mem);
    Expression *l1 = init_var_expression("l", locals);
    Expression *post1 = init_var_expression(
        "post", init_arrow_expression(
                    trace, init_arrow_expression(
                               mem, init_arrow_expression(
                                        locals, init_prop_expression()))));
    exec_skip = init_var_expression(
        "exec_skip",
        init_forall_expression(
            t1,
            init_forall_expression(
                m1,
                init_forall_expression(
                    l1,
                    init_forall_expression(
                        post1,
                        init_arrow_expression(
                            init_app_expression(
                                init_app_expression(
                                    init_app_expression(post1, t1), m1),
                                l1),
                            init_app_expression(
                                init_app_expression(
                                    init_app_expression(
                                        init_app_expression(
                                            init_app_expression(exec, cmd_skip),
                                            t1),
                                        m1),
                                    l1),
                                post1)))))));

    // exec_set
    Expression *x2 = init_var_expression("x", string);
    Expression *e2 = init_var_expression("e", expr);
    Expression *t2 = init_var_expression("t", trace);
    Expression *m2 = init_var_expression("m", mem);
    Expression *l2 = init_var_expression("l", locals);
    Expression *post2 = init_var_expression(
        "post", init_arrow_expression(
                    trace, init_arrow_expression(
                               mem, init_arrow_expression(
                                        locals, init_prop_expression()))));
    Expression *v2 = init_var_expression("v", word);

    Expression *H2_1 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(option, word)),
            init_app_expression(
                init_app_expression(init_app_expression(eval_expr, m2), l2),
                e2)),
        init_app_expression(init_app_expression(option_some, word), v2));
    Expression *H2_2 = init_app_expression(
        init_app_expression(init_app_expression(post2, t2), m2),
        init_app_expression(
            init_app_expression(
                init_app_expression(
                    init_app_expression(
                        init_app_expression(partial_map_put, string), word),
                    l2),
                x2),
            v2));
    Expression *C2 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(
                    init_app_expression(
                        exec, init_app_expression(
                                  init_app_expression(cmd_set, x2), e2)),
                    t2),
                m2),
            l2),
        post2);

    exec_set = init_var_expression(
        "exec_set",
        init_forall_expression(
            t2,
            init_forall_expression(
                x2,
                init_forall_expression(
                    e2, init_forall_expression(
                            m2, init_forall_expression(
                                    l2, init_forall_expression(
                                            post2,
                                            init_forall_expression(
                                                v2, init_arrow_expression(
                                                        H2_1,
                                                        init_arrow_expression(
                                                            H2_2, C2))))))))));

    // exec_seq
    Expression *t3 = init_var_expression("t", trace);
    Expression *c1_3 = init_var_expression("c1", cmd);
    Expression *c2_3 = init_var_expression("c2", cmd);
    Expression *m3 = init_var_expression("m", mem);
    Expression *l3 = init_var_expression("l", locals);
    Expression *post3 = init_var_expression(
        "post", init_arrow_expression(
                    trace, init_arrow_expression(
                               mem, init_arrow_expression(
                                        locals, init_prop_expression()))));

    Expression *t3_p = init_var_expression("t'", trace);
    Expression *m3_p = init_var_expression("m'", mem);
    Expression *l3_p = init_var_expression("l'", locals);
    Expression *mid_post3 = init_lambda_expression(
        t3_p,
        init_lambda_expression(
            m3_p,
            init_lambda_expression(
                l3_p, init_app_expression(
                          init_app_expression(
                              init_app_expression(
                                  init_app_expression(
                                      init_app_expression(exec, c2_3), t3_p),
                                  m3_p),
                              l3_p),
                          post3))));
    Expression *H3 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression(exec, c1_3), t3), m3),
            l3),
        mid_post3);

    Expression *C3 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(
                    init_app_expression(
                        exec, init_app_expression(
                                  init_app_expression(cmd_seq, c1_3), c2_3)),
                    t3),
                m3),
            l3),
        post3);

    exec_seq = init_var_expression(
        "exec_seq",
        init_forall_expression(
            t3, init_forall_expression(
                    c1_3,
                    init_forall_expression(
                        c2_3, init_forall_expression(
                                  m3, init_forall_expression(
                                          l3, init_forall_expression(
                                                  post3, init_arrow_expression(
                                                             H3, C3))))))));

    // exec_input : forall t lhs m l post,
    // 	(forall v : word, post (list_cons IOEvent (IOEvent_IN v) t) m
    // (partial_map_put string word l lhs v)) ->
    //     exec (cmd_input lhs) t m l pos
    Expression *t4 = init_var_expression("t", trace);
    Expression *lhs4 = init_var_expression("lhs", string);
    Expression *m4 = init_var_expression("m", mem);
    Expression *l4 = init_var_expression("l", locals);
    Expression *post4 = init_var_expression(
        "post", init_arrow_expression(
                    trace, init_arrow_expression(
                               mem, init_arrow_expression(
                                        locals, init_prop_expression()))));

    Expression *v4 = init_var_expression("v", word);
    Expression *t4_sub = init_app_expression(
        init_app_expression(init_app_expression(list_cons, IOEvent),
                            init_app_expression(IOEvent_IN, v4)),
        t4);
    Expression *m4_sub = m4;
    Expression *l4_sub = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(
                    init_app_expression(partial_map_put, string), word),
                l4),
            lhs4),
        v4);
    Expression *H4 = init_forall_expression(
        v4, init_app_expression(
                init_app_expression(init_app_expression(post4, t4_sub), m4_sub),
                l4_sub));

    Expression *C4 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(
                    init_app_expression(exec,
                                        init_app_expression(cmd_input, lhs4)),
                    t4),
                m4),
            l4),
        post4);

    exec_input = init_var_expression(
        "exec_input",
        init_forall_expression(
            t4,
            init_forall_expression(
                lhs4,
                init_forall_expression(
                    m4, init_forall_expression(
                            l4, init_forall_expression(
                                    post4, init_arrow_expression(H4, C4)))))));

    return context_insert_n(c, 5, exec, exec_skip, exec_set, exec_seq,
                            exec_input);
}

Context *init_properties(Context *c) {
    Expression *x = init_var_expression("x", word);
    word_add_0_r = init_var_expression(
        "word_add_0_r",
        init_forall_expression(
            x, init_app_expression(
                   init_app_expression(
                       init_app_expression(eq, word),
                       init_app_expression(
                           init_app_expression(
                               word_add,
                               init_app_expression(
                                   init_app_expression(option_some, word), x)),
                           init_app_expression(
                               init_app_expression(option_some, word),
                               init_app_expression(word_of_Z, Z0)))),
                   x)));

    Expression *w = init_var_expression("w", word);
    Expression *option_some_w =
        init_app_expression(init_app_expression(option_some, word), w);
    Expression *some_add_result = init_app_expression(
        init_app_expression(option_some, word),
        init_app_expression(init_app_expression(word_add, option_some_w),
                            option_some_w));
    Expression *some_sub_result = init_app_expression(
        init_app_expression(option_some, word),
        init_app_expression(init_app_expression(word_sub, some_add_result),
                            option_some_w));
    Expression *equality = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(option, word)),
            some_sub_result),
        option_some_w);
    word_add_sub_cancel = init_var_expression(
        "word_add_sub_cancel", init_forall_expression(w, equality));

    return context_insert_n(c, 2, word_add_0_r, word_add_sub_cancel);
}

Expression *make_exec_test_3(Context *ctx) {
    // prog := (cmd.seq (cmd.set "b" (expr.op bopname.add "a" 0)) (cmd.set "c"
    // "b")).
    Expression *prog_sub1 = init_app_expression(
        init_app_expression(init_app_expression(expr_op, bopname_add),
                            init_app_expression(expr_var, a_string)),
        init_app_expression(expr_literal, Z0));
    Expression *prog_sub2 =
        init_app_expression(init_app_expression(cmd_set, b_string), prog_sub1);
    Expression *prog_sub3 =
        init_app_expression(init_app_expression(cmd_set, c_string),
                            init_app_expression(expr_var, b_string));
    Expression *prog =
        init_app_expression(init_app_expression(cmd_seq, prog_sub2), prog_sub3);

    Expression *t = init_var_expression("t", trace);
    Expression *m = init_var_expression("m", mem);
    Expression *a_val = init_var_expression("a_val", word);

    // l := (map.put string word) (map.empty string word) "a" a_val
    Expression *l_sub1 =
        init_app_expression(init_app_expression(partial_map_put, string), word);
    Expression *l_sub2 = init_app_expression(
        init_app_expression(partial_map_empty, string), word);
    Expression *l = init_app_expression(
        init_app_expression(init_app_expression(l_sub1, l_sub2), a_string),
        a_val);

    // post := (fun t m' l' => and (t' = t) and ((m' = m) (map.get l' "c" = Some
    // a_val)))
    Expression *t_prime = init_var_expression("t'", trace);
    Expression *m_prime = init_var_expression("m'", mem);
    Expression *l_prime = init_var_expression("l'", locals);
    Expression *post_sub0 = init_app_expression(
        init_app_expression(init_app_expression(eq, trace), t_prime), t);
    Expression *post_sub1 = init_app_expression(
        init_app_expression(init_app_expression(eq, mem), m_prime), m);
    Expression *post_sub2 = init_app_expression(
        init_app_expression(
            init_app_expression(init_app_expression(partial_map_get, string),
                                word),
            l_prime),
        c_string);
    Expression *post_sub3 =
        init_app_expression(init_app_expression(option_some, word), a_val);
    Expression *post_sub4 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(option, word)),
            post_sub2),
        post_sub3);
    Expression *post_sub5 = init_app_expression(
        init_app_expression(and, post_sub0),
        init_app_expression(init_app_expression(and, post_sub1), post_sub4));
    Expression *post = init_lambda_expression(
        t_prime, init_lambda_expression(
                     m_prime, init_lambda_expression(l_prime, post_sub5)));

    // conclusion := exec t prog m l (fun t' m' l' => t' = t /\ (m' = m
    // /\ map.get l' "c" = Some a_val))
    Expression *conclusion = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression_wc(exec, prog, ctx), t),
                m),
            l),
        post);
    Expression *cmd_ok_theorem = init_forall_expression(
        t,
        init_forall_expression(m, init_forall_expression(a_val, conclusion)));
    return cmd_ok_theorem;
}

Expression *make_exec_test_2(Context *ctx) {
    // forall t m l, exec t (cmd.seq cmd.skip cmd.skip) m l (fun t' m' l' => and
    // (t' = t) (and (m' = m) (l' = l)))
    Expression *t = init_var_expression("t", trace);
    Expression *m = init_var_expression("m", mem);
    Expression *l = init_var_expression("l", locals);
    Expression *cmd =
        init_app_expression(init_app_expression(cmd_seq, cmd_skip), cmd_skip);

    Expression *t_prime = init_var_expression("t'", trace);
    Expression *m_prime = init_var_expression("m'", mem);
    Expression *l_prime = init_var_expression("l'", locals);
    Expression *post_sub0 = init_app_expression(
        init_app_expression(init_app_expression(eq, trace), t_prime), t);
    Expression *post_sub1 = init_app_expression(
        init_app_expression(init_app_expression(eq, mem), m_prime), m);
    Expression *post_sub2 = init_app_expression(
        init_app_expression(init_app_expression(eq, locals), l_prime), l);
    Expression *post = init_lambda_expression(
        t_prime,
        init_lambda_expression(
            m_prime, init_lambda_expression(
                         l_prime, init_app_expression(
                                      init_app_expression(and, post_sub0),
                                      init_app_expression(
                                          init_app_expression(and, post_sub1),
                                          post_sub2)))));

    Expression *cmd_ok_theorem = init_forall_expression(
        t,
        init_forall_expression(
            m,
            init_forall_expression(
                l, init_app_expression(
                       init_app_expression(
                           init_app_expression(
                               init_app_expression(
                                   init_app_expression_wc(exec, cmd, ctx), t),
                               m),
                           l),
                       post))));
    return cmd_ok_theorem;
}

Expression *make_exec_test_1(Context *ctx) {
    // forall t m l, exec cmd.skip m l (fun m' l' => and (t' = t) (and (m' = m)
    // (l' = l)))
    Expression *t = init_var_expression("t", trace);
    Expression *m = init_var_expression("m", mem);
    Expression *l = init_var_expression("l", locals);
    Expression *cmd = cmd_skip;

    Expression *t_prime = init_var_expression("t'", trace);
    Expression *m_prime = init_var_expression("m'", mem);
    Expression *l_prime = init_var_expression("l'", locals);
    Expression *post_sub0 = init_app_expression(
        init_app_expression(init_app_expression(eq, trace), t_prime), t);
    Expression *post_sub1 = init_app_expression(
        init_app_expression(init_app_expression(eq, mem), m_prime), m);
    Expression *post_sub2 = init_app_expression(
        init_app_expression(init_app_expression(eq, locals), l_prime), l);
    Expression *post = init_lambda_expression(
        t_prime,
        init_lambda_expression(
            m_prime, init_lambda_expression(
                         l_prime, init_app_expression(
                                      init_app_expression(and, post_sub0),
                                      init_app_expression(
                                          init_app_expression(and, post_sub1),
                                          post_sub2)))));

    Expression *cmd_ok_theorem = init_forall_expression(
        t,
        init_forall_expression(
            m,
            init_forall_expression(
                l, init_app_expression(
                       init_app_expression(
                           init_app_expression(
                               init_app_expression(
                                   init_app_expression_wc(exec, cmd, ctx), t),
                               m),
                           l),
                       post))));
    return cmd_ok_theorem;
}

Expression *make_exec_test_4(Context *ctx) {
    Expression *m = init_var_expression("m", mem);
    Expression *l = init_var_expression("l", locals);
    Expression *t = init_app_expression(list_nil, IOEvent);
    Expression *cmd = init_app_expression(cmd_input, a_string);

    Expression *t_prime = init_var_expression("t'", trace);
    Expression *m_prime = init_var_expression("m'", mem);
    Expression *l_prime = init_var_expression("l'", locals);
    Expression *post_sub0 = init_app_expression(
        init_app_expression(init_app_expression(eq, mem), m_prime), m);

    Expression *v = init_var_expression("v", word);
    Expression *post_sub1 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(list, IOEvent)),
            t_prime),
        init_app_expression(
            init_app_expression(init_app_expression(list_cons, IOEvent),
                                init_app_expression(IOEvent_IN, v)),
            init_app_expression(list_nil, IOEvent)));
    Expression *post = init_lambda_expression(
        t_prime,
        init_lambda_expression(
            m_prime, init_lambda_expression(
                         l_prime, init_app_expression(
                                      init_app_expression(ex, word),
                                      init_lambda_expression(v, post_sub1)))));

    Expression *cmd_ok_theorem = init_forall_expression(
        m, init_forall_expression(
               l, init_app_expression(
                      init_app_expression(
                          init_app_expression(
                              init_app_expression(
                                  init_app_expression_wc(exec, cmd, ctx), t),
                              m),
                          l),
                      post)));
    return cmd_ok_theorem;
}

Expression *make_exec_test_5(Context *ctx) {
    // forall m l, let t := (list_nil IOEvent) in
    // exec (cmd_input a) m l (fun t' m' l' =>
    // 		and (eq (partial_map word byte) m' m)
    // 			(ex word (fun v =>
    //              			  ((and (eq (list IOEvent) t' (list_cons
    //              IOEvent (IOEvent_IN v) (list_nil IOEvent)))))
    // 	 				  		  	    (eq (option
    // word) (partial_map_get string word l' a) (option_some word v)))
    Expression *m = init_var_expression("m", mem);
    Expression *l = init_var_expression("l", locals);
    Expression *t = init_app_expression(list_nil, IOEvent);
    Expression *cmd = init_app_expression(cmd_input, a_string);

    Expression *t_prime = init_var_expression("t'", trace);
    Expression *m_prime = init_var_expression("m'", mem);
    Expression *l_prime = init_var_expression("l'", locals);
    Expression *post_sub0 = init_app_expression(
        init_app_expression(init_app_expression(eq, mem), m_prime), m);

    Expression *v = init_var_expression("v", word);
    Expression *post_sub1 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(list, IOEvent)),
            t_prime),
        init_app_expression(
            init_app_expression(init_app_expression(list_cons, IOEvent),
                                init_app_expression(IOEvent_IN, v)),
            init_app_expression(list_nil, IOEvent)));
    Expression *post_sub2 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(option, word)),
            init_app_expression(
                init_app_expression(
                    init_app_expression(
                        init_app_expression(partial_map_get, string), word),
                    l_prime),
                a_string)),
        init_app_expression(init_app_expression(option_some, word), v));
    Expression *post = init_lambda_expression(
        t_prime,
        init_lambda_expression(
            m_prime,
            init_lambda_expression(
                l_prime, init_app_expression(
                             init_app_expression(and, post_sub0),
                             init_app_expression(
                                 init_app_expression(ex, word),
                                 init_lambda_expression(
                                     v, init_app_expression(
                                            init_app_expression(and, post_sub1),
                                            post_sub2)))))));

    Expression *cmd_ok_theorem = init_forall_expression(
        m, init_forall_expression(
               l, init_app_expression(
                      init_app_expression(
                          init_app_expression(
                              init_app_expression(
                                  init_app_expression_wc(exec, cmd, ctx), t),
                              m),
                          l),
                      post)));
    return cmd_ok_theorem;
}

Expression *make_test_0() {
    Expression *v = init_var_expression("v", word);
    Expression *P = init_lambda_expression(
        v, init_app_expression(
               init_app_expression(init_app_expression(eq, word), v),
               init_app_expression(word_of_Z, Z0)));
    return init_app_expression(init_app_expression(ex, word), P);
}

Expression *_repeated_cmds(int n, Expression *t, Expression *x) {
    Expression *expr_vart = init_app_expression(expr_var, t);
    Expression *expr_varx = init_app_expression(expr_var, x);
    Expression *c1 = init_app_expression(
        init_app_expression(cmd_set, t),
        init_app_expression(
            init_app_expression(init_app_expression(expr_op, bopname_add),
                                expr_vart),
            expr_vart));
    Expression *c2_1 = init_app_expression(
        init_app_expression(cmd_set, t),
        init_app_expression(
            init_app_expression(init_app_expression(expr_op, bopname_sub),
                                expr_vart),
            expr_varx));

    Expression *curr = cmd_skip;
    for (int i = 0; i < n; i++) {
        Expression *c2 =
            init_app_expression(init_app_expression(cmd_seq, c2_1), curr);
        curr = init_app_expression(init_app_expression(cmd_seq, c1), c2);
    }
    return curr;
}

Expression *_make_test_6_cmd(int n, Expression *t, Expression *x) {
    return init_app_expression(
        init_app_expression(cmd_seq, init_app_expression(cmd_input, x)),
        init_app_expression(
            init_app_expression(
                cmd_seq, init_app_expression(init_app_expression(cmd_set, t),
                                             init_app_expression(expr_var, x))),
            _repeated_cmds(n, t, x)));
}

Expression *make_exec_test_6(Context *ctx, int n) {
    // forall m l, let t := (list_nil IOEvent) in
    // exec (cmd_input a) m l (fun t' m' l' =>
    // 		and (eq (partial_map word byte) m' m)
    //		 (ex word (fun v : word =>
    //		 	and (eq (list IOEvent) t' (list_cons IOEvent (IOEvent_IN
    // v) (list_nil IOEvent))) 		 		(eq (option word)
    // (partial_map_get string word l' a) 		 						(option_some word v))))).
    Expression *m = init_var_expression("m", mem);
    Expression *l = init_var_expression("l", locals);
    Expression *t = init_app_expression(list_nil, IOEvent);
    Expression *cmd = _make_test_6_cmd(n, a_string, b_string);

    Expression *t_prime = init_var_expression("t'", trace);
    Expression *m_prime = init_var_expression("m'", mem);
    Expression *l_prime = init_var_expression("l'", locals);
    Expression *post_sub0 = init_app_expression(
        init_app_expression(init_app_expression(eq, mem), m_prime), m);

    Expression *v = init_var_expression("v", word);
    Expression *post_sub1 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(list, IOEvent)),
            t_prime),
        init_app_expression(
            init_app_expression(init_app_expression(list_cons, IOEvent),
                                init_app_expression(IOEvent_IN, v)),
            init_app_expression(list_nil, IOEvent)));
    Expression *post_sub2 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_app_expression(option, word)),
            init_app_expression(
                init_app_expression(
                    init_app_expression(
                        init_app_expression(partial_map_get, string), word),
                    l_prime),
                a_string)),
        init_app_expression(init_app_expression(option_some, word), v));
    Expression *post = init_lambda_expression(
        t_prime,
        init_lambda_expression(
            m_prime,
            init_lambda_expression(
                l_prime, init_app_expression(
                             init_app_expression(and, post_sub0),
                             init_app_expression(
                                 init_app_expression(ex, word),
                                 init_lambda_expression(
                                     v, init_app_expression(
                                            init_app_expression(and, post_sub1),
                                            post_sub2)))))));

    Expression *cmd_ok_theorem = init_forall_expression(
        m, init_forall_expression(
               l, init_app_expression(
                      init_app_expression(
                          init_app_expression(
                              init_app_expression(
                                  init_app_expression_wc(exec, cmd, ctx), t),
                              m),
                          l),
                      post)));
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
    if (remaining_goals == NULL)
        printf("solve_skip failed to find a solution\n");
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
    if (remaining_goals == NULL)
        printf("solve_seq failed to find a solution\n");
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
    if (remaining_goals == NULL)
        printf("solve_and failed to find a solution\n");
    return remaining_goals;
}

DoublyLinkedList *solve_eq(Expression *goal) {
    DoublyLinkedList *remaining_goals;

    RewrittenGoal *rewrites_result = rewrites_transform(
        goal, 5, partial_map_get_put_same, binop_add_to_word_add,
        binop_add_to_word_sub, partial_map_get_put_diff, word_add_sub_cancel);
    remaining_goals = apply(rewrites_result->new_goal, eq_refl);
    if (remaining_goals != NULL)
        return dll_merge(rewrites_result->remaining_open, remaining_goals);
    if (remaining_goals == NULL) printf("solve_eq failed to find a solution\n");

    return NULL;
}

Expression *solve_ex(Expression *goal) {
    DoublyLinkedList *remaining_goals = eapply(goal, ex_intro);
    if (remaining_goals == NULL) printf("solve_ex failed to find a solution\n");
    if (dll_len(remaining_goals) != 2) {
        return NULL;
    }

    Expression *goal_to_solve = dll_at(remaining_goals, 1)->data;
    dll_destroy(remaining_goals);
    return goal_to_solve;
}

void solve_not(Expression *goal) {
    DoublyLinkedList *remaining_goals = apply(goal, not_eq_string_b_a);
    if (remaining_goals == NULL)
        printf("solve_not failed to find a solution\n");
    if (remaining_goals != NULL) {
        dll_destroy(remaining_goals);
        return;
    }
}

DoublyLinkedList *solve_set(Expression *goal) {
    RewrittenGoal *simplified_locals =
        rewrite_transform(goal, partial_map_put_put_same);
    DoublyLinkedList *remaining_goals =
        eapply(simplified_locals->new_goal, exec_set);
    // DoublyLinkedList *remaining_goals = eapply(goal, exec_set);
    if (remaining_goals == NULL)
        printf("solve_set failed to find a solution\n");
    if (dll_len(remaining_goals) != 3) {
        return NULL;
    }

    Expression *eq_goal = (Expression *)dll_at(remaining_goals, 1)->data;
    normalize_hole_type(eq_goal);

    DoublyLinkedList *goals_from_eq = solve_eq(eq_goal);

    Expression *remaining_goal = dll_remove_tail(remaining_goals)->data;
    dll_destroy(remaining_goals);

    dll_insert_at_tail(goals_from_eq, dll_new_node(remaining_goal));
    return goals_from_eq;
}

Expression *solve_input(Expression *goal) {
    DoublyLinkedList *remaining_goals = apply(goal, exec_input);
    if (remaining_goals == NULL)
        printf("solve_input failed to find a solution\n");
    Expression *remaining_goal = dll_remove_tail(remaining_goals)->data;
    dll_destroy(remaining_goals);
    return remaining_goal;
}

Expression *_sym_solve(Expression *initial_goal) {
    // TODO: This is a workaround to be able to retrieve the proof term after
    // solving.
    Expression *temp = init_lambda_expression(
        init_var_expression("temp", init_type_expression()), initial_goal);
    DoublyLinkedList *hypotheses = dll_create();

    DoublyLinkedList *goals_to_solve = dll_create();
    dll_insert_at_tail(goals_to_solve, dll_new_node(initial_goal));

    while (dll_len(goals_to_solve) > 0) {
        Expression *goal = (Expression *)dll_remove_head(goals_to_solve)->data;
        Expression *goal_type = get_expression_type(goal);

        switch (goal_type->type) {
            case (FORALL_EXPRESSION): {
                IntroReturn *intro_return = intro(goal);
                dll_insert_at_tail(
                    hypotheses, dll_new_node(intro_return->proof_of_old->value
                                                 .lambda.bound_variable));
                dll_insert_at_tail(goals_to_solve,
                                   dll_new_node(intro_return->new_goal));
                free_intro_return(intro_return);
                break;
            }
            case (APP_EXPRESSION): {
                Expression *innermost = get_innermost_func(goal_type);
                if (innermost == exec) {
                    Expression *post = goal_type->value.app.arg;
                    Expression *locals =
                        goal_type->value.app.func->value.app.arg;
                    Expression *memory = goal_type->value.app.func->value.app
                                             .func->value.app.arg;
                    Expression *trace =
                        goal_type->value.app.func->value.app.func->value.app
                            .func->value.app.arg;
                    Expression *cmd =
                        goal_type->value.app.func->value.app.func->value.app
                            .func->value.app.func->value.app.arg;
                    Expression *cmd_type = get_cmd_type(cmd);

                    if (cmd_type == cmd_skip) {
                        dll_insert_at_tail(goals_to_solve,
                                           dll_new_node(solve_skip(goal)));
                    } else if (cmd_type == cmd_seq) {
                        dll_insert_at_tail(goals_to_solve,
                                           dll_new_node(solve_seq(goal)));
                    } else if (cmd_type == cmd_set) {
                        DoublyLinkedList *new_goals = solve_set(goal);
                        int n = dll_len(new_goals);
                        for (int i = 0; i < n; i++) {
                            dll_insert_at_tail(
                                goals_to_solve,
                                dll_new_node(dll_at(new_goals, i)->data));
                        }
                        dll_destroy(new_goals);
                    } else if (cmd_type == cmd_input) {
                        dll_insert_at_head(goals_to_solve,
                                           dll_new_node(solve_input(goal)));
                    }
                } else if (innermost == and) {
                    DoublyLinkedList *new_goals = solve_and(goal);
                    int n = dll_len(new_goals);
                    for (int i = 0; i < n; i++) {
                        dll_insert_at_tail(
                            goals_to_solve,
                            dll_new_node(dll_at(new_goals, i)->data));
                    }
                    dll_destroy(new_goals);
                } else if (innermost == eq) {
                    DoublyLinkedList *new_goals = solve_eq(goal);
                    int n = dll_len(new_goals);
                    for (int i = 0; i < n; i++) {
                        dll_insert_at_tail(
                            goals_to_solve,
                            dll_new_node(dll_at(new_goals, i)->data));
                    }
                    dll_destroy(new_goals);
                } else if (innermost == ex) {
                    // This is what we're interested in, so return immediately.
                    dll_insert_at_tail(goals_to_solve,
                                       dll_new_node(solve_ex(goal)));
                } else if (innermost == not ) {
                    solve_not(goal);
                }
                break;
            }
        }
    }

    return temp->value.lambda.body;
}

Expression *straightline_solve(Expression *e) {
    Expression *goal =
        init_hole_expression("Goal", e, get_expression_context(e));
    Expression *result = _sym_solve(goal);
    return result;
}

void run_symbolic(int n) {
    Context *c = std_lib_ctx;

    c = init_word(c);
    c = init_positive(c);
    c = init_N(c);
    c = init_Z(c);
    c = init_string(c);
    c = init_list(c);
    c = init_io_event(c);
    c = init_storage(c);
    c = init_bopname(c);
    c = init_expr(c);
    c = init_cmd(c);
    c = init_exec(c);
    c = init_properties(c);

    Expression *cmd_ok_theorem = make_exec_test_6(c, n);
    Expression *proof = straightline_solve(cmd_ok_theorem);
    // printf("%s\n\n%s\n", stringify_expression2(cmd_ok_theorem),
    // stringify_expression2(proof));
    printf("%s\n", stringify_expression2(get_expression_type(proof)));
}