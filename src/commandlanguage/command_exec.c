#include "src/commandlanguage/command_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/commandlanguage/command_parser.h"
#include "src/common/color.h"
#include "src/common/doubly_linked_list.h"
#include "src/common/options.h"
#include "src/common/timing.h"
#include "src/engine/engine_api.h"
#include "src/kernel/kernel_api.h"
#include "src/runtime/runtime.h"
#include "src/tacticlanguage/compiled_tactics.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/termlanguage/ast_to_expression.h"

static int _handle_declaration_command(MEngineRuntime *rt, DeclarationCmd *decl_cmd) {
    Expression *var_type = ast_to_expression(decl_cmd->binder.type, rt->ctx);
    if (!var_type) {
        fprintf(stderr, ERROR "Failed to convert type for declaration '%s'.\n" CRESET,
                decl_cmd->binder.name);
        return 1;
    }
    Expression *new_var = kernel_var_create(decl_cmd->binder.name, var_type, rt->ctx);
    if (!new_var) {
        fprintf(stderr,
                ERROR
                "Failed to create variable '%s' (invalid type or "
                "context).\n" CRESET,
                decl_cmd->binder.name);
        return 1;
    }
    rt->ctx = new_var;

    if (!rt->options->quiet) {
        char *_type_str = kernel_expr_to_string(var_type);
        fprintf(stdout, UI "%s " CRESET "%s : %s declared.\n", decl_keyword_to_string(decl_cmd->kw),
                decl_cmd->binder.name, _type_str);
        free(_type_str);
    }
    return 0;
}

Expression *_create_definition_body(MEngineRuntime *rt, Binder **params, size_t param_count,
                                    AST *body, Expression **rendered_def_body,
                                    Context **rendered_type_ctx) {
    // Given params = {(p0: P0) (p1: P1) ... (pn: Pn)}, param_count = n, and a
    // body, convert it to the term fun (p0: P0) => ... => fun (pn: Pn) => body
    Context *c = rt->ctx;
    Expression **params_rendered = malloc(param_count * sizeof(Expression *));
    Context **contexts = malloc((param_count + 1) * sizeof(Context *));

    // The entire body must be valid in the runtime context
    contexts[0] = c;

    for (size_t i = 0; i < param_count; i++) {
        Binder *b = params[i];

        Expression *param_type = ast_to_expression(b->type, c);
        Expression *param_var = kernel_var_create(b->name, param_type, c);
        c = param_var;

        params_rendered[i] = param_var;
        contexts[i + 1] = c;
    }

    Expression *result = ast_to_expression(body, c);
    *rendered_def_body = result;
    *rendered_type_ctx = c;
    for (size_t i = param_count; i > 0; i--) {
        result = kernel_lambda_create(params_rendered[i - 1], result);
    }

    free(params_rendered);
    free(contexts);
    return result;
}

static int _handle_definition_command(MEngineRuntime *rt, DefinitionCmd *defn_cmd) {
    if (!rt || !defn_cmd) {
        return 1;
    }

    const char *name = defn_cmd->name;
    Binder **params = defn_cmd->params;
    size_t param_count = defn_cmd->param_count;

    Expression *rendered_def_body = NULL;
    Context *rendered_type_ctx = NULL;
    Expression *body = _create_definition_body(rt, params, param_count, defn_cmd->body,
                                               &rendered_def_body, &rendered_type_ctx);

    Expression *inferred_type_def_body = kernel_expr_type(rendered_def_body);
    Expression *expected_type_def_body = ast_to_expression(defn_cmd->type, rendered_type_ctx);
    if (!expected_type_def_body) {
        fprintf(stderr, ERROR "Error:" CRESET " definition '%s' has a invalid type.\n", name);
    }

    if (!kernel_expr_congruent(inferred_type_def_body, expected_type_def_body)) {
        fprintf(stderr, ERROR "Type error:" CRESET " definition '%s' has a mismatched type.\n",
                name);
        char *_s1 = kernel_expr_to_string(expected_type_def_body);
        char *_s2 = kernel_expr_to_string(inferred_type_def_body);
        fprintf(stderr, "  Declared type: %s\n", _s1);
        fprintf(stderr, "  Inferred type: %s\n", _s2);
        free(_s1);
        free(_s2);
        kernel_expr_free_excluding_ctx(expected_type_def_body, NULL, rendered_type_ctx);
        return 1;
    }
    kernel_expr_free_excluding_ctx(expected_type_def_body, NULL, rendered_type_ctx);

    Expression *defn_var = kernel_var_create_with_body(defn_cmd->name, body, rt->ctx);
    rt->ctx = defn_var;
    if (!rt->options->quiet) {
        char *_type_str = kernel_expr_to_string(kernel_expr_type(defn_var));
        fprintf(stdout, UI "Definition " CRESET "%s : %s defined.\n", name, _type_str);
        free(_type_str);
    }
    return 0;
}

static int _handle_statement_command(MEngineRuntime *rt, StatementCmd *stmt_cmd) {
    if (!rt || !stmt_cmd) {
        return 1;
    }

    Expression *statement_type = ast_to_expression(stmt_cmd->type, rt->ctx);
    if (!statement_type) {
        fprintf(stderr, ERROR "Failed to convert type for Statement %s\n" CRESET, stmt_cmd->name);
        return 1;
    }

    Expression *initial_goal = kernel_hole_create((char *)"Goal", statement_type, rt->ctx);
    Expression *pending_theorem =
        kernel_var_create_with_body(stmt_cmd->name, initial_goal, rt->ctx);
    mengine_runtime_proof_mode(rt, pending_theorem);

    if (!rt->options->quiet) {
        char *_type_str = kernel_expr_to_string(statement_type);
        fprintf(stdout, UI "%s " CRESET "%s : %s stated.\n", stmt_keyword_to_string(stmt_cmd->kw),
                stmt_cmd->name, _type_str);
        free(_type_str);
    }

    debug_print_mode(rt);
    return 0;
}

static int _handle_check_command(MEngineRuntime *rt, CheckCmd *check_cmd) {
    if (!rt || !check_cmd) {
        return 1;
    }

    // If we're in proof mode, we should render this Expression against the
    // current goal's context rather than the runtime context.
    Context *ctx = (rt->mode == MENGINE_RUNTIME_PROOF_MODE)
                       ? kernel_expr_context(engine_proof_state_current_goal(rt->proof_state))
                       : rt->ctx;
    Expression *expr = ast_to_expression(check_cmd->term, ctx);
    if (!expr) {
        fprintf(stderr, ERROR "Failed to convert term in Check command.\n" CRESET);
        return 1;
    }

    Expression *expr_type = kernel_expr_type(expr);
    if (!expr_type) {
        fprintf(stderr, ERROR "Failed to get type of expression in Check command.\n" CRESET);
        return 1;
    }

    if (!rt->options->quiet) {
        char *_s1 = kernel_expr_to_string(expr);
        char *_s2 = kernel_expr_to_string(expr_type);
        fprintf(stdout, DIMTEXT "%s\n\t: %s\n" CRESET, _s1, _s2);
        free(_s1);
        free(_s2);
    }
    // expr shares children (context vars) with ctx - use the context-safe free.
    kernel_expr_free_excluding_ctx(expr, NULL, ctx);
    return 0;
}

void _print_inductive_definition(MEngineRuntime *rt, Expression *expr) {
    if (!expr || rt->options->quiet) {
        return;
    }

    // Inductive <name> : <type> :=
    char *_name_str = kernel_expr_to_string(expr);
    char *_type_str = kernel_expr_to_string(kernel_expr_type(expr));
    MPRINT(rt->options->quiet, stdout, DIMTEXT "Inductive " CRESET "%s : %s := ", _name_str,
           _type_str);
    free(_name_str);
    free(_type_str);

    // | cons : type
    int constructor_count;
    Expression **constructors = kernel_inductive_constructors(expr, &constructor_count);
    for (int i = 0; i < constructor_count; i++) {
        Expression *ctor = constructors[i];
        char *_ctor_str = kernel_expr_to_string(ctor);
        char *_ctor_type_str = kernel_expr_to_string(kernel_expr_type(ctor));
        MPRINT(rt->options->quiet, stdout, "\n\t| %s : %s", _ctor_str, _ctor_type_str);
        free(_ctor_str);
        free(_ctor_type_str);
    }

    MPRINT(rt->options->quiet, stdout, ".\n")
}

static int _handle_print_command(MEngineRuntime *rt, PrintCmd *print_cmd) {
    if (!rt || !print_cmd) {
        return 1;
    }

    // If we're in proof mode, we should render this Expression against the
    // current goal's context rather than the runtime context.
    Context *ctx = (rt->mode == MENGINE_RUNTIME_PROOF_MODE)
                       ? kernel_expr_context(engine_proof_state_current_goal(rt->proof_state))
                       : rt->ctx;
    Expression *expr = kernel_context_lookup(ctx, print_cmd->name);
    if (!expr) {
        fprintf(stderr, ERROR "Failed to convert term in Print command.\n" CRESET);
        return 1;
    }

    Expression *expr_type = kernel_expr_type(expr);
    if (!expr_type) {
        fprintf(stderr, ERROR "Failed to get type of expression in Print command.\n" CRESET);
        return 1;
    }

    // Check if it's an inductive type
    if (kernel_expr_is_inductive(expr)) {
        _print_inductive_definition(rt, expr);
        return 0;
    }

    // Attempt to get the variable's body
    Expression *expr_body = kernel_var_body(expr);
    if (!expr_body) {
        char *_expr_str = kernel_expr_to_string(expr);
        fprintf(stderr, ERROR "%s is an opaque variable.\n" CRESET, _expr_str);
        free(_expr_str);
        return 1;
    }

    if (!rt->options->quiet) {
        char *_s1 = kernel_expr_to_string(expr);
        char *_s2 = kernel_expr_to_string(expr_body);
        char *_s3 = kernel_expr_to_string(expr_type);
        fprintf(stdout, DIMTEXT "%s := %s\n\t: %s\n" CRESET, _s1, _s2, _s3);
        free(_s1);
        free(_s2);
        free(_s3);
    }
    return 0;
}

static Expression *_build_constructor_case_type(Expression *ctor_expr, Expression *ctor_type,
                                                Expression *motive_var, Expression *ind_var,
                                                Expression **param_vars, size_t param_count,
                                                size_t index_count, Context *elim_ctx);

static Expression *_build_motive_type(Expression *ind_var, Expression **param_vars,
                                      size_t param_count, Context *ctx,
                                      Expression ***index_vars_out, size_t *index_count_out) {
    Expression *ind_applied = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_applied = kernel_app_create(ind_applied, param_vars[i], ctx);
        if (!ind_applied) {
            fprintf(stderr, ERROR "Failed to apply parameter %zu in motive type\n" CRESET, i);
            return NULL;
        }
    }

    // Check if the TYPE of ind_applied is a forall - if so, we have indices
    Expression *ind_applied_type = kernel_expr_type(ind_applied);
    Expression *current = ind_applied_type;
    size_t index_count = 0;
    while (kernel_forall_var(current) != NULL) {
        index_count++;
        current = kernel_forall_body(current);
    }

    if (index_count == 0) {
        // No indices: motive is (ind params) -> Prop
        *index_vars_out = NULL;
        *index_count_out = 0;
        return kernel_arrow_create(ind_applied, kernel_prop_create(), ctx);
    }

    // With indices: motive is forall (indices), (ind params indices) -> Prop
    Expression **index_vars = malloc(index_count * sizeof(Expression *));
    Context *motive_ctx = ctx;
    current = ind_applied_type;

    for (size_t i = 0; i < index_count; i++) {
        Expression *bound_var = kernel_forall_var(current);
        if (!bound_var) {
            fprintf(stderr, ERROR "Failed to read index binder from inductive type.\n" CRESET);
            free(index_vars);
            return NULL;
        }
        Expression *index_type = kernel_expr_type(bound_var);
        char index_name[32];
        sprintf(index_name, "i%zu", i);

        index_vars[i] = kernel_var_create(index_name, index_type, motive_ctx);
        motive_ctx = index_vars[i];
        current = kernel_forall_body(current);
    }

    // Build (ind params index_0 ... index_n)
    Expression *ind_with_indices = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_with_indices = kernel_app_create(ind_with_indices, param_vars[i], motive_ctx);
    }
    for (size_t i = 0; i < index_count; i++) {
        ind_with_indices = kernel_app_create(ind_with_indices, index_vars[i], motive_ctx);
    }

    // Build forall (i0 : T0) ... (in : Tn), (ind params i0 ... in) -> Prop
    Expression *motive_type =
        kernel_arrow_create(ind_with_indices, kernel_prop_create(), motive_ctx);
    for (size_t i = index_count; i > 0; i--) {
        motive_type = kernel_forall_create(index_vars[i - 1], motive_type);
        motive_ctx = kernel_expr_context(index_vars[i - 1]);
    }

    *index_vars_out = index_vars;
    *index_count_out = index_count;
    return motive_type;
}

static Expression *_build_induction_principle_type(InductiveCmd *ind_cmd, Expression *ind_var,
                                                   Expression **param_vars, size_t param_count,
                                                   Context **contexts) {
    Context *elim_ctx = contexts[param_count];

    Expression **index_vars = NULL;
    size_t index_count = 0;
    Expression *motive_type =
        _build_motive_type(ind_var, param_vars, param_count, elim_ctx, &index_vars, &index_count);
    Expression *motive_var = kernel_var_create("P", motive_type, elim_ctx);
    elim_ctx = motive_var;

    size_t ctor_count = ind_cmd->constructor_count;
    Expression **case_vars = malloc(ctor_count * sizeof(Expression *));
    Context **case_contexts = malloc((ctor_count + 1) * sizeof(Context *));
    case_contexts[0] = elim_ctx;

    for (size_t i = 0; i < ctor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];
        Expression *ctor_expr = kernel_context_lookup(elim_ctx, ctor->name);
        if (!ctor_expr) {
            fprintf(stderr, ERROR "Constructor %s not found in context\n" CRESET, ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }

        Expression *ctor_type = kernel_expr_type(ctor_expr);
        Expression *case_type =
            _build_constructor_case_type(ctor_expr, ctor_type, motive_var, ind_var, param_vars,
                                         param_count, index_count, case_contexts[i]);

        if (!case_type) {
            fprintf(stderr, ERROR "Failed to build case type for %s\n" CRESET, ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }

        char case_name[64];
        sprintf(case_name, "case_%s", ctor->name);
        case_vars[i] = kernel_var_create(case_name, case_type, case_contexts[i]);
        if (!case_vars[i]) {
            fprintf(stderr, ERROR "Failed to create case variable for %s\n" CRESET, ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
        case_contexts[i + 1] = case_vars[i];
    }

    // Build conclusion: forall (indices) (target : ind params indices), P
    // indices target
    Context *final_ctx = case_contexts[ctor_count];

    // Create fresh index variables for the conclusion
    Expression **concl_index_vars = NULL;
    if (index_count > 0) {
        concl_index_vars = malloc(index_count * sizeof(Expression *));
        for (size_t i = 0; i < index_count; i++) {
            Expression *index_type = kernel_expr_type(index_vars[i]);
            char index_name[32];
            sprintf(index_name, "y%zu", i);
            concl_index_vars[i] = kernel_var_create(index_name, index_type, final_ctx);
            final_ctx = concl_index_vars[i];
        }
    }

    // Build (ind params concl_index_0 ... concl_index_n)
    Expression *ind_applied = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_applied = kernel_app_create(ind_applied, param_vars[i], final_ctx);
        if (!ind_applied) {
            fprintf(stderr,
                    ERROR
                    "Failed to apply parameter %zu to inductive in "
                    "conclusion\n" CRESET,
                    i);
            free(case_vars);
            free(case_contexts);
            if (index_vars) {
                free(index_vars);
            }
            if (concl_index_vars) {
                free(concl_index_vars);
            }
            return NULL;
        }
    }
    for (size_t i = 0; i < index_count; i++) {
        ind_applied = kernel_app_create(ind_applied, concl_index_vars[i], final_ctx);
    }

    Expression *target_var = kernel_var_create("e", ind_applied, final_ctx);
    Context *target_ctx = target_var;

    // Apply motive to indices and target: P concl_index_0 ... concl_index_n
    // target
    Expression *target_applied = motive_var;
    for (size_t i = 0; i < index_count; i++) {
        target_applied = kernel_app_create(target_applied, concl_index_vars[i], target_ctx);
    }
    target_applied = kernel_app_create(target_applied, target_var, target_ctx);
    if (!target_applied) {
        fprintf(stderr, ERROR "Failed to apply motive to target\n" CRESET);
        free(case_vars);
        free(case_contexts);
        if (index_vars) {
            free(index_vars);
        }
        if (concl_index_vars) {
            free(concl_index_vars);
        }
        return NULL;
    }

    Expression *result = kernel_forall_create(target_var, target_applied);

    // Wrap with foralls for conclusion indices
    for (size_t i = index_count; i > 0; i--) {
        result = kernel_forall_create(concl_index_vars[i - 1], result);
        final_ctx = kernel_expr_context(concl_index_vars[i - 1]);
    }

    if (concl_index_vars) {
        free(concl_index_vars);
    }

    for (size_t i = ctor_count; i > 0; i--) {
        result = kernel_forall_create(case_vars[i - 1], result);
        if (!result) {
            fprintf(stderr, ERROR "Failed to wrap with constructor case %zu\n" CRESET, i - 1);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
    }

    result = kernel_forall_create(motive_var, result);
    if (!result) {
        fprintf(stderr, ERROR "Failed to wrap with motive P\n" CRESET);
        free(case_vars);
        free(case_contexts);
        return NULL;
    }

    for (size_t i = param_count; i > 0; i--) {
        result = kernel_forall_create(param_vars[i - 1], result);
        if (!result) {
            fprintf(stderr, ERROR "Failed to wrap with parameter %zu\n" CRESET, i - 1);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
    }

    free(case_vars);
    free(case_contexts);
    if (index_vars) {
        free(index_vars);
    }
    return result;
}

// Build the induction hypothesis for a single constructor argument, or NULL when
// the argument is not a recursive occurrence of the inductive being defined.
//
// An argument `arg : I p0..p_{m-1} x0..x_{k-1}` (the inductive applied to its
// parameters and indices) yields the hypothesis `P x0..x_{k-1} arg` — the motive
// applied to the argument's own indices and then the argument itself. This is
// what turns the generated principle from plain case analysis into induction.
//
// Only first-order recursion is recognised; a higher-order argument such as
// `(nat -> I)` is treated as non-recursive and gets no hypothesis.
static Expression *_build_recursive_arg_hypothesis(Expression *arg_var, Expression *arg_type,
                                                   Expression *ind_var, Expression *motive_var,
                                                   size_t param_count, size_t index_count,
                                                   Context *ctx) {
    if (!kernel_expr_congruent(kernel_expr_head(arg_type), ind_var)) {
        return NULL;
    }

    Expression *hypothesis = motive_var;
    if (index_count > 0) {
        // arg_type is the spine `I p0..p_{m-1} x0..x_{k-1}`; the indices are the
        // arguments past the parameters.
        DoublyLinkedList *spine = dll_create();
        Expression *head = arg_type;
        while (kernel_app_func(head) != NULL) {
            dll_insert_at_head(spine, dll_new_node(kernel_app_arg(head)));
            head = kernel_app_func(head);
        }
        for (size_t i = 0; i < index_count; i++) {
            Expression *index = (Expression *)dll_at(spine, param_count + i)->data;
            hypothesis = kernel_app_create(hypothesis, index, ctx);
        }
        dll_destroy(spine);
    }
    return kernel_app_create(hypothesis, arg_var, ctx);
}

static Expression *_build_constructor_case_type(Expression *ctor_expr, Expression *ctor_type,
                                                Expression *motive_var, Expression *ind_var,
                                                Expression **param_vars, size_t param_count,
                                                size_t index_count, Context *elim_ctx) {
    Expression *core_type = ctor_type;
    for (size_t i = 0; i < param_count; i++) {
        if (kernel_forall_var(core_type) != NULL) {
            core_type = kernel_forall_body(core_type);
        }
    }

    DoublyLinkedList *arg_types = dll_create();
    Expression *current = core_type;
    while (kernel_forall_var(current) != NULL) {
        Expression *arg_type = kernel_expr_type(kernel_forall_var(current));
        dll_insert_at_tail(arg_types, dll_new_node(arg_type));
        current = kernel_forall_body(current);
    }

    Expression *ctor_app = ctor_expr;
    for (size_t i = 0; i < param_count; i++) {
        ctor_app = kernel_app_create(ctor_app, param_vars[i], elim_ctx);
        if (!ctor_app) {
            fprintf(stderr, ERROR "Failed to apply parameter %zu to constructor\n" CRESET, i);
            dll_destroy(arg_types);
            return NULL;
        }
    }

    size_t arg_count = dll_len(arg_types);
    Expression **arg_vars = malloc(arg_count * sizeof(Expression *));
    Context *case_ctx = elim_ctx;

    for (size_t i = 0; i < arg_count; i++) {
        Expression *arg_type = (Expression *)dll_at(arg_types, i)->data;
        char arg_name[32];
        sprintf(arg_name, "arg%zu", i);

        arg_vars[i] = kernel_var_create(arg_name, arg_type, case_ctx);
        case_ctx = arg_vars[i];
        ctor_app = kernel_app_create(ctor_app, arg_vars[i], case_ctx);
        if (!ctor_app) {
            fprintf(stderr, ERROR "Failed to apply constructor arg %zu\n" CRESET, i);
            dll_destroy(arg_types);
            free(arg_vars);
            return NULL;
        }
    }

    // Extract indices from constructor return type
    // current holds the return type after stripping foralls
    Expression **ctor_indices = NULL;
    if (index_count > 0) {
        ctor_indices = malloc(index_count * sizeof(Expression *));

        // Parse the return type as a spine of applications
        // For eq_refl: (((eq A) x) x) - we want to extract the indices (the
        // trailing applications)
        DoublyLinkedList *spine = dll_create();
        Expression *head = current;
        while (kernel_app_func(head) != NULL) {
            dll_insert_at_head(spine, dll_new_node(kernel_app_arg(head)));
            head = kernel_app_func(head);
        }

        // The spine now has all the arguments. Skip param_count, take
        // index_count
        size_t total_args = dll_len(spine);
        if (total_args < param_count + index_count) {
            fprintf(stderr, ERROR "Constructor return type has too few arguments\n" CRESET);
            dll_destroy(spine);
            dll_destroy(arg_types);
            free(arg_vars);
            free(ctor_indices);
            return NULL;
        }

        // Extract the indices (skip parameters)
        for (size_t i = 0; i < index_count; i++) {
            ctor_indices[i] = (Expression *)dll_at(spine, param_count + i)->data;
        }

        dll_destroy(spine);
    }

    // Apply motive to indices first, then to constructor application
    Expression *case_result = motive_var;
    for (size_t i = 0; i < index_count; i++) {
        case_result = kernel_app_create(case_result, ctor_indices[i], case_ctx);
        if (!case_result) {
            fprintf(stderr, ERROR "Failed to apply motive to index %zu\n" CRESET, i);
            dll_destroy(arg_types);
            free(arg_vars);
            if (ctor_indices) {
                free(ctor_indices);
            }
            return NULL;
        }
    }

    case_result = kernel_app_create(case_result, ctor_app, case_ctx);
    if (!case_result) {
        fprintf(stderr, ERROR "Failed to apply motive to constructor application\n" CRESET);
        dll_destroy(arg_types);
        free(arg_vars);
        if (ctor_indices) {
            free(ctor_indices);
        }
        return NULL;
    }

    if (ctor_indices) {
        free(ctor_indices);
    }
    Expression *case_type = case_result;

    // Wrap the conclusion with an induction hypothesis per recursive argument,
    // giving `IH_0 -> .. -> IH_k -> P (c args)`. These sit inside the argument
    // binders below because each hypothesis mentions its argument.
    for (size_t i = arg_count; i > 0; i--) {
        Expression *arg_type = (Expression *)dll_at(arg_types, i - 1)->data;
        Expression *hypothesis = _build_recursive_arg_hypothesis(
            arg_vars[i - 1], arg_type, ind_var, motive_var, param_count, index_count, case_ctx);
        if (hypothesis) {
            case_type = kernel_arrow_create(hypothesis, case_type, case_ctx);
        }
    }

    for (size_t i = arg_count; i > 0; i--) {
        case_type = kernel_forall_create(arg_vars[i - 1], case_type);
    }

    dll_destroy(arg_types);
    free(arg_vars);
    return case_type;
}

static int _handle_inductive_command(MEngineRuntime *rt, InductiveCmd *ind_cmd) {
    if (!rt || !ind_cmd) {
        return 1;
    }

    const char *name = ind_cmd->name;
    Binder **params = ind_cmd->params;
    size_t param_count = ind_cmd->param_count;

    Context *c = rt->ctx;
    Expression **param_vars = malloc(param_count * sizeof(Expression *));
    Context **contexts = malloc((param_count + 1) * sizeof(Context *));

    contexts[0] = c;

    for (size_t i = 0; i < param_count; i++) {
        Expression *param_type = ast_to_expression(params[i]->type, c);
        Expression *param_var = kernel_var_create(params[i]->name, param_type, c);
        c = param_var;

        param_vars[i] = param_var;
        contexts[i + 1] = c;
    }

    Expression *ind_return_type = ast_to_expression(ind_cmd->type, c);
    Expression *ind_type = ind_return_type;
    for (size_t i = param_count; i > 0; i--) {
        ind_type = kernel_forall_create(param_vars[i - 1], ind_type);
    }

    Expression *ind_var = kernel_var_create(name, ind_type, rt->ctx);
    rt->ctx = ind_var;
    c = ind_var;

    for (size_t i = 0; i <= param_count; i++) {
        contexts[i] = ind_var;
    }

    if (!rt->options->quiet) {
        char *_ind_type_str = kernel_expr_to_string(ind_type);
        fprintf(stdout, UI "Inductive " CRESET "%s : %s defined.\n", name, _ind_type_str);
        free(_ind_type_str);
    }

    size_t ctor_count = ind_cmd->constructor_count;

    Expression **ctor_vars = malloc(ctor_count * sizeof(Expression *));
    if (!ctor_vars) {
        fprintf(stderr, ERROR "Failed to allocate constructor array.\n" CRESET);
        free(param_vars);
        free(contexts);
        return 1;
    }

    for (size_t i = 0; i < ctor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];

        /* For parametric inductives, constructor types must reference both the
         * inductive type (ind_var) and the parameters.  Since ind_var was added
         * to the context AFTER the original params, the original param_vars are
         * not accessible from c = ind_var (or the last ctor_var).  We create
         * fresh copies of each parameter variable in order, anchored AFTER the
         * current context (c), so that:
         *   - the params are accessible by name when parsing the constructor type
         *   - fresh_params[k].context is in the ancestor chain of ctor_core_type
         *   - kernel_forall_create(fresh_params[k], ...) passes valid_in_context
         * For non-parametric inductives (param_count == 0) this loop does nothing. */
        Context *parse_ctx = c;
        Expression **fresh_params = NULL;
        if (param_count > 0) {
            fresh_params = malloc(param_count * sizeof(Expression *));
            if (!fresh_params) {
                fprintf(stderr, ERROR "Failed to allocate fresh_params.\n" CRESET);
                free(ctor_vars);
                free(param_vars);
                free(contexts);
                return 1;
            }
            for (size_t k = 0; k < param_count; k++) {
                Expression *fp_type = ast_to_expression(params[k]->type, parse_ctx);
                if (!fp_type) {
                    fprintf(stderr, ERROR "Failed to convert fresh param type for %s.\n" CRESET,
                            params[k]->name);
                    free(fresh_params);
                    free(ctor_vars);
                    free(param_vars);
                    free(contexts);
                    return 1;
                }
                Expression *fp = kernel_var_create(params[k]->name, fp_type, parse_ctx);
                if (!fp) {
                    fprintf(stderr, ERROR "Failed to create fresh param %s.\n" CRESET,
                            params[k]->name);
                    free(fresh_params);
                    free(ctor_vars);
                    free(param_vars);
                    free(contexts);
                    return 1;
                }
                fresh_params[k] = fp;
                parse_ctx = fp;
            }
        }

        Expression *ctor_core_type = ast_to_expression(ctor->type, parse_ctx);
        if (!ctor_core_type) {
            fprintf(stderr, ERROR "Failed to convert constructor type for %s\n" CRESET, ctor->name);
            if (fresh_params) {
                free(fresh_params);
            }
            free(ctor_vars);
            free(param_vars);
            free(contexts);
            return 1;
        }

        /* Wrap the constructor type with foralls for each parameter, using the
         * fresh param copies.  Because ctor_core_type was parsed in context
         * parse_ctx (which descends from fresh_params[last]), valid_in_context
         * holds at every wrapping step. */
        Expression *ctor_type = ctor_core_type;
        for (size_t j = param_count; j > 0; j--) {
            ctor_type = kernel_forall_create(fresh_params[j - 1], ctor_type);
            if (!ctor_type) {
                fprintf(stderr, ERROR "Failed to wrap constructor type with parameter %zu\n" CRESET,
                        j - 1);
                free(fresh_params);
                free(ctor_vars);
                free(param_vars);
                free(contexts);
                return 1;
            }
        }
        if (fresh_params) {
            free(fresh_params);
            fresh_params = NULL;
        }

        Expression *ctor_var = kernel_var_create(ctor->name, ctor_type, rt->ctx);
        if (!ctor_var) {
            fprintf(stderr, ERROR "Failed to create constructor variable for %s\n" CRESET,
                    ctor->name);
            free(ctor_vars);
            free(param_vars);
            free(contexts);
            return 1;
        }

        ctor_vars[i] = ctor_var;

        rt->ctx = ctor_var;
        c = ctor_var;

        for (size_t j = 0; j <= param_count; j++) {
            contexts[j] = ctor_var;
        }

        if (!rt->options->quiet) {
            char *_ctor_type_str = kernel_expr_to_string(ctor_type);
            fprintf(stdout, UI "Constructor " CRESET "%s : %s defined.\n", ctor->name,
                    _ctor_type_str);
            free(_ctor_type_str);
        }
    }

    char *ind_principle_name = malloc(strlen(name) + 5);
    sprintf(ind_principle_name, "%s_ind", name);

    /* The induction principle builder uses the original param_vars, but
     * constructor types now use fresh parameter copies.  The resulting case
     * types would fail the application type check (fresh_param != original_param).
     * Skip the induction principle for parametric inductives; it is not needed
     * by the tactics that use parametric types (e.g., sep_list in cancel). */
    Expression *ind_principle_type = NULL;
    if (param_count == 0) {
        ind_principle_type =
            _build_induction_principle_type(ind_cmd, ind_var, param_vars, param_count, contexts);
    }

    Expression *ind_principle_var = NULL;
    if (ind_principle_type) {
        ind_principle_var = kernel_var_create(ind_principle_name, ind_principle_type, rt->ctx);
        rt->ctx = ind_principle_var;

        if (!rt->options->quiet) {
            char *_ind_p_str = kernel_expr_to_string(ind_principle_type);
            fprintf(stdout, UI "Induction principle " CRESET "%s : %s generated.\n",
                    ind_principle_name, _ind_p_str);
            free(_ind_p_str);
        }
    }

    // Register the inductive type
    if (!kernel_inductive_register(ind_var, ctor_vars, ctor_count, ind_principle_var)) {
        fprintf(stderr, ERROR "Failed to register inductive type %s.\n" CRESET, name);
    } else {
        MPRINT(rt->options->quiet, stdout,
               UI "Registered " CRESET "inductive %s with %zu constructor(s).\n", name, ctor_count);
    }

    free(ctor_vars);
    free(ind_principle_name);
    free(param_vars);
    free(contexts);
    return 0;
}

static int _handle_fixpoint_command(MEngineRuntime *rt, FixpointCmd *fix_cmd) {
    if (!rt || !fix_cmd) {
        return 1;
    }

    // Convert the fixpoint AST to a fix expression
    AST *fix_ast = malloc(sizeof(AST));
    fix_ast->tag = AST_FIX;
    fix_ast->value.fix.name = fix_cmd->name;
    fix_ast->value.fix.binders = fix_cmd->binders;
    fix_ast->value.fix.binder_count = fix_cmd->binder_count;
    fix_ast->value.fix.decreasing_arg_name = fix_cmd->decreasing_arg_name;
    fix_ast->value.fix.return_type = fix_cmd->return_type;
    fix_ast->value.fix.body = fix_cmd->body;

    Expression *fixpoint_var = ast_to_expression(fix_ast, rt->ctx);
    free(fix_ast);
    if (!fixpoint_var) {
        fprintf(stderr, ERROR "Failed to convert fixpoint '%s' to expression.\n" CRESET,
                fix_cmd->name);
        return 1;
    }

    rt->ctx = fixpoint_var;

    if (!rt->options->quiet) {
        char *_type_str = kernel_expr_to_string(kernel_expr_type(fixpoint_var));
        fprintf(stdout, UI "Fixpoint " CRESET "%s : %s defined.\n", fix_cmd->name, _type_str);
        free(_type_str);
    }
    return 0;
}

static int _handle_eval_command(MEngineRuntime *rt, EvalCmd *eval_cmd) {
    if (!rt || !eval_cmd) {
        return 1;
    }

    // Convert AST to expression
    Context *ctx = (rt->mode == MENGINE_RUNTIME_PROOF_MODE)
                       ? kernel_expr_context(engine_proof_state_current_goal(rt->proof_state))
                       : rt->ctx;
    Expression *expr = ast_to_expression(eval_cmd->term, ctx);
    if (!expr) {
        fprintf(stderr, ERROR "Failed to convert term in Eval command.\n" CRESET);
        return 1;
    }

    // Determine reduction flags based on strategy and flags
    unsigned int flags = 0;
    Expression *result = NULL;

    switch (eval_cmd->strategy) {
        case EVAL_STRATEGY_CBV:
            // For cbv, use the flags if specified, otherwise use all reductions
            if (eval_cmd->beta_flag || eval_cmd->delta_flag || eval_cmd->iota_flag ||
                eval_cmd->fix_flag) {
                if (eval_cmd->beta_flag) {
                    flags |= KERNEL_REDUCE_BETA;
                }
                if (eval_cmd->delta_flag) {
                    flags |= KERNEL_REDUCE_DELTA;
                }
                if (eval_cmd->iota_flag) {
                    flags |= KERNEL_REDUCE_IOTA;
                }
                if (eval_cmd->fix_flag) {
                    flags |= KERNEL_REDUCE_FIX;
                }
            } else {
                flags = KERNEL_REDUCE_ALL;
            }
            result = kernel_normalize_cbv(expr, flags);
            break;

        case EVAL_STRATEGY_COMPUTE:
            result = kernel_normalize_compute(expr);
            break;

        default:
            fprintf(stderr, ERROR "Unknown evaluation strategy.\n" CRESET);
            return 1;
    }

    if (!result) {
        fprintf(stderr, ERROR "Normalization failed.\n" CRESET);
        return 1;
    }

    if (!rt->options->quiet) {
        char *_s1 = kernel_expr_to_string(result);
        char *_s2 = kernel_expr_to_string(kernel_expr_type(result));
        fprintf(stdout, DIMTEXT "\t= %s\n\t: %s\n" CRESET, _s1, _s2);
        free(_s1);
        free(_s2);
    }
    // Free result and expr together (result may share sub-trees with expr).
    // Both may also share context vars with ctx - use the context-safe free.
    Expression *b = (result != expr) ? expr : NULL;
    kernel_expr_free_excluding_ctx(result, b, ctx);
    return 0;
}

static int _handle_show_command(MEngineRuntime *rt, ShowCmd *show_cmd) {
    if (!rt || !show_cmd) {
        return 1;
    }

    switch (show_cmd->kw) {
        case SHOW_KW_CONTEXT: {
            Context *ctx = mengine_runtime_context(rt);
            MPRINT(rt->options->quiet, stdout, HEADER "Context:" CRESET "\n%s\n",
                   kernel_context_to_string(ctx));
            break;
        }
        case SHOW_KW_PROOF: {
            if (rt->mode == MENGINE_RUNTIME_COMMAND_MODE) {
                fprintf(stderr, RED
                        "Error: 'Show Proof' can only be used in Proof "
                        "Mode.\n" CRESET);
                return 1;
            }

            if (!rt->proof_state) {
                fprintf(stderr, ERROR " No active proof state.\n" CRESET);
                return 1;
            }

            Expression *current_goal = engine_proof_state_current_goal(rt->proof_state);
            if (!current_goal) {
                fprintf(stderr, ERROR " No current goal.\n" CRESET);
                return 1;
            }

            // Show proof term
            Expression *pending_theorem = engine_proof_state_pending_theorem(rt->proof_state);
            Expression *pending_theorem_body = kernel_var_body(pending_theorem);
            Expression *pending_theorem_type = kernel_expr_type(pending_theorem);
            if (!rt->options->quiet) {
                char *_s1 = kernel_expr_to_string(pending_theorem_body);
                char *_s2 = kernel_expr_to_string(pending_theorem_type);
                fprintf(stdout, HEADER "Proof Term:" CRESET "\n%s : %s\n", _s1, _s2);
                free(_s1);
                free(_s2);
            }
            return 0;
        }
        case SHOW_KW_GOAL: {
            if (rt->mode == MENGINE_RUNTIME_COMMAND_MODE) {
                fprintf(stderr, RED
                        "Error: 'Show Goal' can only be used in Proof "
                        "Mode.\n" CRESET);
                return 1;
            }

            if (!rt->proof_state) {
                fprintf(stderr, ERROR " No active proof state.\n" CRESET);
                return 1;
            }

            Expression *current_goal = engine_proof_state_current_goal(rt->proof_state);
            if (!current_goal) {
                fprintf(stderr, ERROR " No current goal.\n" CRESET);
                return 1;
            }

            Context *goal_ctx = kernel_expr_context(current_goal);
            if (!rt->options->quiet) {
                if (goal_ctx) {
                    char *ctx_str = kernel_context_to_string(goal_ctx);
                    fprintf(stdout, HEADER "Goal Context:" CRESET "\n%s\n", ctx_str);
                    free(ctx_str);
                }
                char *_goal_str = kernel_expr_to_string(kernel_expr_type(current_goal));
                fprintf(stdout, HEADER "Goal:" CRESET "\n%s\n", _goal_str);
                free(_goal_str);
            }
            return 0;
        }
        case SHOW_KW_STATE: {
            if (rt->mode == MENGINE_RUNTIME_COMMAND_MODE) {
                fprintf(stderr, RED
                        "Error: 'Show State' can only be used in Proof "
                        "Mode.\n" CRESET);
                return 1;
            }

            if (!rt->proof_state) {
                fprintf(stderr, ERROR " No active proof state.\n" CRESET);
                return 1;
            }

            Expression *current_goal = engine_proof_state_current_goal(rt->proof_state);
            if (!current_goal) {
                fprintf(stderr, ERROR " No current goal.\n" CRESET);
                return 1;
            }

            // Show proof term
            Expression *pending_theorem = engine_proof_state_pending_theorem(rt->proof_state);
            Expression *pending_theorem_body = kernel_var_body(pending_theorem);
            if (!rt->options->quiet) {
                char *_proof_str = kernel_expr_to_string(pending_theorem_body);
                fprintf(stdout, HEADER "Proof Term:" CRESET "\n%s\n", _proof_str);
                free(_proof_str);

                // Show current goal context
                Context *goal_ctx = kernel_expr_context(current_goal);
                if (goal_ctx) {
                    char *ctx_str = kernel_context_to_string(goal_ctx);
                    fprintf(stdout, CYN "Goal Context:" CRESET "\n%s\n", ctx_str);
                    free(ctx_str);
                }

                // Show goal
                char *_goal_str = kernel_expr_to_string(kernel_expr_type(current_goal));
                fprintf(stdout, CYN "Goal:" CRESET "\n%s\n", _goal_str);
                free(_goal_str);
            }
            return 0;
        }
    }
    return 0;
}

static int _handle_tactic_def_command(MEngineRuntime *rt, TacticDefCmd *tc) {
    TacticDef *def = malloc(sizeof(TacticDef));
    def->name = tc->name;
    def->params = tc->params;
    def->param_count = tc->param_count;
    def->fn = NULL;
    def->body = tc->body;
    def->compiled_env = NULL;
    tactic_def_attach_compiled(rt, def);
    tactic_env_add(rt->tactic_env, def);
    MPRINT(rt->options->quiet, stdout, UI "Tactic " CRESET "%s defined.\n", tc->name);
    return 0;
}

static int _handle_register_relation_command(MEngineRuntime *rt, RegisterRelationCmd *rr) {
    Expression *relation = ast_to_expression(rr->relation, rt->ctx);
    Expression *refl = ast_to_expression(rr->refl, rt->ctx);
    Expression *trans = ast_to_expression(rr->trans, rt->ctx);
    Expression *congr = ast_to_expression(rr->congr, rt->ctx);

    if (!relation || !refl || !trans || !congr) {
        fprintf(stderr, ERROR "Failed to resolve terms in Register Relation command.\n" CRESET);
        return 1;
    }

    EngineRelationInfo info = {.relation = relation, .refl = refl, .trans = trans, .congr = congr};
    if (!engine_relation_registry_add(rt->relation_registry, info)) {
        fprintf(stderr, ERROR "Failed to register relation.\n" CRESET);
        return 1;
    }

    char *_rel_str = kernel_expr_to_string(relation);
    MPRINT(rt->options->quiet, stdout, UI "Relation " CRESET "%s registered.\n", _rel_str);
    free(_rel_str);
    return 0;
}

int mengine_execute_command(MEngineRuntime *rt, Command *cmd) {
    if (!rt || !cmd) {
        return 1;
    }
    timer_push(TIMER_COMMAND);
    int rc;
    switch (cmd->tag) {
        case CMD_DECLARATION: {
            rc = _handle_declaration_command(rt, &cmd->as.decl);
            break;
        }
        case CMD_DEFINITION: {
            rc = _handle_definition_command(rt, &cmd->as.defn);
            break;
        }
        case CMD_STATEMENT: {
            rc = _handle_statement_command(rt, &cmd->as.stmt);
            break;
        }
        case CMD_CHECK: {
            rc = _handle_check_command(rt, &cmd->as.check);
            break;
        }
        case CMD_PRINT: {
            rc = _handle_print_command(rt, &cmd->as.print);
            break;
        }
        case CMD_EVAL: {
            rc = _handle_eval_command(rt, &cmd->as.eval);
            break;
        }
        case CMD_INDUCTIVE: {
            rc = _handle_inductive_command(rt, &cmd->as.inductive);
            break;
        }
        case CMD_FIXPOINT: {
            rc = _handle_fixpoint_command(rt, &cmd->as.fixpoint);
            break;
        }
        case CMD_SHOW: {
            rc = _handle_show_command(rt, &cmd->as.show);
            break;
        }
        case CMD_TACTIC_DEF: {
            rc = _handle_tactic_def_command(rt, &cmd->as.tactic_def);
            break;
        }
        case CMD_REGISTER_RELATION: {
            rc = _handle_register_relation_command(rt, &cmd->as.register_relation);
            break;
        }
        default:
            rc = 1;
            break;
    }
    timer_pop();
    return rc;
}
