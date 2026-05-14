#include "ast_to_expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/doubly_linked_list.h"
#include "src/common/lexer.h"
#include "src/kernel/kernel_api.h"
#include "src/termlanguage/parser.h"

/**
 * Simple linked list node for storing let bindings with string keys.
 */
typedef struct LetBinding {
    char *name;
    Expression *value;
} LetBinding;

typedef struct LocalDepName {
    const char *name;
    int index;
    struct LocalDepName *next;
} LocalDepName;

typedef struct SourceLetDeps {
    const char *name;
    struct DepInfo *deps;
    struct SourceLetDeps *next;
} SourceLetDeps;

typedef struct DepInfo {
    Context *ambient;
    bool *local_deps;
    int *local_indices;
    size_t local_dep_count;
    size_t local_count;
    int max_local_dep;
} DepInfo;

typedef struct Telescope {
    ASTTag tag;
    Binder **binders;
    size_t binder_count;
    AST *body;
} Telescope;

/**
 * Internal helper for recursive conversion.
 * Maintains a working context that includes newly bound variables.
 *
 * @param ast The AST node to convert.
 * @param context The current context including any bound variables.
 * @param letbindings A linked list of LetBinding structs for variable
 * substitutions.
 * @return The converted Expression, or NULL on failure.
 */
static Expression *_ast_to_expression(AST *ast, Context *context, DoublyLinkedList *letbindings,
                                      TacticEnvEntry *tac_env);

/**
 * Helper to look up a let binding by name.
 */
static Expression *letbindings_get(DoublyLinkedList *letbindings, const char *name) {
    if (!letbindings || !name) {
        return NULL;
    }

    DLLNode *current = letbindings->head;
    while (current) {
        LetBinding *binding = (LetBinding *)current->data;
        if (binding && strcmp(binding->name, name) == 0) {
            return binding->value;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * Helper to add a let binding.
 */
static void letbindings_set(DoublyLinkedList *letbindings, const char *name, Expression *value) {
    if (!letbindings || !name) {
        return;
    }

    LetBinding *binding = (LetBinding *)malloc(sizeof(LetBinding));
    if (!binding) {
        return;
    }

    binding->name = strdup(name);
    binding->value = value;
    dll_insert_at_tail(letbindings, dll_new_node(binding));
}

static bool is_anonymous_name(const char *name) {
    return name && name[0] == '_' && name[1] == '\0';
}

static Expression *dependency_context_lookup(Context *context, const char *name) {
    bool target_is_anonymous = is_anonymous_name(name);
    Context *current = context;
    while (current && current != kernel_context_empty()) {
        const char *current_name = kernel_var_name(current);
        if ((!target_is_anonymous && is_anonymous_name(current_name)) ||
            strcmp(current_name, name) != 0) {
            current = kernel_expr_context(current);
            continue;
        }
        return current;
    }
    return NULL;
}

static Context *join_required_contexts(Context *lhs, Context *rhs) {
    if (!lhs || !rhs) {
        return NULL;
    }
    if (lhs == rhs) {
        return lhs;
    }
    if (kernel_context_is_ancestor(lhs, rhs)) {
        return rhs;
    }
    if (kernel_context_is_ancestor(rhs, lhs)) {
        return lhs;
    }
    fprintf(stderr, ERROR "Cannot join incomparable required contexts.\n" CRESET);
    return NULL;
}

static Context *require_context_valid(Context *required, Context *lexical_context,
                                      const char *source) {
    if (!required) {
        return NULL;
    }
    if (!kernel_context_is_ancestor(required, lexical_context)) {
        fprintf(stderr, ERROR "%s is not valid in the current lexical context.\n" CRESET, source);
        return NULL;
    }
    return required;
}

static bool dep_init(DepInfo *deps, size_t local_count) {
    deps->ambient = kernel_context_empty();
    deps->local_count = local_count;
    deps->local_dep_count = 0;
    deps->max_local_dep = -1;
    deps->local_deps = local_count == 0 ? NULL : calloc(local_count, sizeof(bool));
    deps->local_indices = local_count == 0 ? NULL : malloc(local_count * sizeof(int));
    if (local_count == 0 || (deps->local_deps != NULL && deps->local_indices != NULL)) {
        return true;
    }
    free(deps->local_deps);
    free(deps->local_indices);
    deps->local_deps = NULL;
    deps->local_indices = NULL;
    return false;
}

static void dep_free(DepInfo *deps) {
    free(deps->local_deps);
    free(deps->local_indices);
    deps->local_deps = NULL;
    deps->local_indices = NULL;
    deps->local_dep_count = 0;
    deps->local_count = 0;
    deps->max_local_dep = -1;
    deps->ambient = NULL;
}

static bool dep_add_ambient(DepInfo *deps, Context *context) {
    context = require_context_valid(context, context, "Dependency context");
    if (!context) {
        return false;
    }
    deps->ambient = join_required_contexts(deps->ambient, context);
    return deps->ambient != NULL;
}

static bool dep_union(DepInfo *dst, const DepInfo *src) {
    dst->ambient = join_required_contexts(dst->ambient, src->ambient);
    if (!dst->ambient) {
        return false;
    }
    if (dst->local_count != src->local_count) {
        fprintf(stderr, ERROR "Mismatched dependency sets.\n" CRESET);
        return false;
    }
    for (size_t i = 0; i < src->local_dep_count; i++) {
        int index = src->local_indices[i];
        if (!dst->local_deps[index]) {
            dst->local_deps[index] = true;
            dst->local_indices[dst->local_dep_count++] = index;
        }
    }
    if (src->max_local_dep > dst->max_local_dep) {
        dst->max_local_dep = src->max_local_dep;
    }
    return true;
}

static void dep_add_local(DepInfo *deps, int index) {
    if (index >= 0 && (size_t)index < deps->local_count) {
        if (!deps->local_deps[index]) {
            deps->local_deps[index] = true;
            deps->local_indices[deps->local_dep_count++] = index;
        }
        if (index > deps->max_local_dep) {
            deps->max_local_dep = index;
        }
    }
}

static int dep_max_local_before(const DepInfo *deps, size_t limit) {
    if (limit > deps->local_count) {
        limit = deps->local_count;
    }
    for (size_t i = limit; i > 0; i--) {
        if (deps->local_deps[i - 1]) {
            return (int)(i - 1);
        }
    }
    return -1;
}

static LocalDepName *local_dep_lookup(LocalDepName *locals, const char *name) {
    if (is_anonymous_name(name)) {
        return NULL;
    }
    for (LocalDepName *it = locals; it; it = it->next) {
        if (strcmp(it->name, name) == 0) {
            return it;
        }
    }
    return NULL;
}

static LocalDepName *push_local_dep(LocalDepName *head, LocalDepName *node, const char *name,
                                    int index) {
    if (is_anonymous_name(name)) {
        return head;
    }
    node->name = name;
    node->index = index;
    node->next = head;
    return node;
}

static bool dep_context_for_name(const char *name, Context *lexical_context,
                                 DoublyLinkedList *letbindings, TacticEnvEntry *tac_env,
                                 LocalDepName *locals, SourceLetDeps *source_lets,
                                 DepInfo *deps);

static bool ast_collect_deps(AST *ast, Context *lexical_context, DoublyLinkedList *letbindings,
                             TacticEnvEntry *tac_env, LocalDepName *locals,
                             SourceLetDeps *source_lets, DepInfo *deps);

static bool dep_context_for_name(const char *name, Context *lexical_context,
                                 DoublyLinkedList *letbindings, TacticEnvEntry *tac_env,
                                 LocalDepName *locals, SourceLetDeps *source_lets,
                                 DepInfo *deps) {
    Expression *letbinding = letbindings_get(letbindings, name);
    if (letbinding) {
        return dep_add_ambient(deps, kernel_expr_context(letbinding));
    }

    for (SourceLetDeps *it = source_lets; it; it = it->next) {
        if (strcmp(it->name, name) == 0) {
            return dep_union(deps, it->deps);
        }
    }

    for (TacticEnvEntry *e = tac_env; e; e = e->next) {
        if (strcmp(e->name, name) != 0) {
            continue;
        }
        if (!e->val) {
            fprintf(stderr, ERROR "Tactic binding %s has no value.\n" CRESET, name);
            return false;
        }
        if (engine_tactic_value_kind(e->val) == ENGINE_TVAL_EXPRESSION) {
            Expression *value = engine_tactic_value_as_expr(e->val);
            return value && dep_add_ambient(deps, kernel_expr_context(value));
        }
        if (engine_tactic_value_kind(e->val) == ENGINE_TVAL_AST) {
            return ast_collect_deps(engine_tactic_value_as_ast(e->val), lexical_context,
                                    letbindings, e->next, locals, source_lets, deps);
        }
        fprintf(stderr, ERROR "Tactic binding %s is not a term.\n" CRESET, name);
        return false;
    }

    LocalDepName *local = local_dep_lookup(locals, name);
    if (local) {
        dep_add_local(deps, local->index);
        return true;
    }

    Expression *var = dependency_context_lookup(lexical_context, name);
    if (!var) {
        fprintf(stderr, ERROR "Variable %s not found in context.\n" CRESET, name);
        return false;
    }
    return dep_add_ambient(deps, var);
}

static bool ast_collect_branch_deps(AST *branch_ast, Context *lexical_context,
                                    DoublyLinkedList *letbindings, TacticEnvEntry *tac_env,
                                    LocalDepName *locals, SourceLetDeps *source_lets,
                                    DepInfo *deps) {
    if (!branch_ast || branch_ast->tag != AST_MATCHBRANCH) {
        fprintf(stderr, ERROR "Expected match branch in dependency analysis.\n" CRESET);
        return false;
    }

    Pattern *pattern = branch_ast->value.matchbranch.pattern;
    if (!dep_context_for_name(pattern->constructor_name, lexical_context, letbindings, tac_env,
                              locals, source_lets, deps)) {
        return false;
    }

    LocalDepName *pattern_locals = locals;
    LocalDepName *pattern_nodes = NULL;
    if (pattern->argument_count > 0) {
        pattern_nodes = calloc((size_t)pattern->argument_count, sizeof(LocalDepName));
        if (!pattern_nodes) {
            fprintf(stderr, ERROR "Memory allocation failed during dependency analysis.\n" CRESET);
            return false;
        }
        for (int i = 0; i < pattern->argument_count; i++) {
            pattern_locals =
                push_local_dep(pattern_locals, &pattern_nodes[i], pattern->argument_names[i], -1);
        }
    }

    bool ok = ast_collect_deps(branch_ast->value.matchbranch.body, lexical_context, letbindings,
                               tac_env, pattern_locals, source_lets, deps);
    free(pattern_nodes);
    return ok;
}

static bool ast_collect_deps(AST *ast, Context *lexical_context, DoublyLinkedList *letbindings,
                             TacticEnvEntry *tac_env, LocalDepName *locals,
                             SourceLetDeps *source_lets, DepInfo *deps) {
    if (!ast) {
        return true;
    }

    switch (ast->tag) {
        case AST_VAR:
            return dep_context_for_name(ast->value.var.name, lexical_context, letbindings, tac_env,
                                        locals, source_lets, deps);
        case AST_TYPE:
        case AST_PROP:
            return true;
        case AST_PATVAR:
            fprintf(stderr, ERROR "Pattern variable %s is not a term.\n" CRESET,
                    ast->value.patvar.name);
            return false;
        case AST_EXPR_REF: {
            Expression *expr = engine_tactic_value_as_expr(ast->value.expr_ref.tval);
            return expr && dep_add_ambient(deps, kernel_expr_context(expr));
        }
        case AST_APP:
            return ast_collect_deps(ast->value.app.func, lexical_context, letbindings, tac_env,
                                    locals, source_lets, deps) &&
                   ast_collect_deps(ast->value.app.arg, lexical_context, letbindings, tac_env,
                                    locals, source_lets, deps);
        case AST_LAMBDA: {
            LocalDepName node;
            bool ok = ast_collect_deps(ast->value.lambda.binder.type, lexical_context, letbindings,
                                       tac_env, locals, source_lets, deps);
            LocalDepName *body_locals =
                push_local_dep(locals, &node, ast->value.lambda.binder.name, -1);
            return ok && ast_collect_deps(ast->value.lambda.body, lexical_context, letbindings,
                                          tac_env, body_locals, source_lets, deps);
        }
        case AST_FORALL: {
            LocalDepName node;
            bool ok = ast_collect_deps(ast->value.forall.binder.type, lexical_context, letbindings,
                                       tac_env, locals, source_lets, deps);
            LocalDepName *body_locals =
                push_local_dep(locals, &node, ast->value.forall.binder.name, -1);
            return ok && ast_collect_deps(ast->value.forall.body, lexical_context, letbindings,
                                          tac_env, body_locals, source_lets, deps);
        }
        case AST_LET: {
            DepInfo value_deps;
            if (!dep_init(&value_deps, deps->local_count)) {
                return false;
            }
            bool ok = ast_collect_deps(ast->value.let.type, lexical_context, letbindings, tac_env,
                                       locals, source_lets, deps) &&
                      ast_collect_deps(ast->value.let.value, lexical_context, letbindings, tac_env,
                                       locals, source_lets, &value_deps) &&
                      dep_union(deps, &value_deps);
            SourceLetDeps source_let = {ast->value.let.name, &value_deps, source_lets};
            ok = ok && ast_collect_deps(ast->value.let.body, lexical_context, letbindings, tac_env,
                                        locals, &source_let, deps);
            dep_free(&value_deps);
            return ok;
        }
        case AST_MATCH: {
            if (!ast_collect_deps(ast->value.match.scrutinee, lexical_context, letbindings,
                                  tac_env, locals, source_lets, deps)) {
                return false;
            }
            for (size_t i = 0; i < ast->value.match.branch_count; i++) {
                if (!ast_collect_branch_deps(ast->value.match.branches[i], lexical_context,
                                             letbindings, tac_env, locals, source_lets, deps)) {
                    return false;
                }
            }
            return true;
        }
        case AST_MATCHBRANCH:
            return ast_collect_branch_deps(ast, lexical_context, letbindings, tac_env, locals,
                                           source_lets, deps);
        case AST_FIX: {
            LocalDepName *fix_locals = locals;
            LocalDepName recursive_node;
            LocalDepName *binder_nodes = NULL;
            size_t binder_count = ast->value.fix.binder_count;
            if (binder_count > 0) {
                binder_nodes = calloc(binder_count, sizeof(LocalDepName));
                if (!binder_nodes) {
                    fprintf(stderr,
                            ERROR "Memory allocation failed during dependency analysis.\n" CRESET);
                    return false;
                }
            }

            bool ok = true;
            for (size_t i = 0; ok && i < binder_count; i++) {
                ok = ast_collect_deps(ast->value.fix.binders[i]->type, lexical_context,
                                      letbindings, tac_env, fix_locals, source_lets, deps);
                fix_locals =
                    push_local_dep(fix_locals, &binder_nodes[i], ast->value.fix.binders[i]->name,
                                   -1);
            }
            ok = ok && ast_collect_deps(ast->value.fix.return_type, lexical_context, letbindings,
                                        tac_env, fix_locals, source_lets, deps);
            LocalDepName *body_locals =
                push_local_dep(fix_locals, &recursive_node, ast->value.fix.name, -1);
            ok = ok && ast_collect_deps(ast->value.fix.body, lexical_context, letbindings, tac_env,
                                        body_locals, source_lets, deps);
            free(binder_nodes);
            return ok;
        }
    }

    fprintf(stderr, ERROR "Unhandled AST tag in dependency analysis.\n" CRESET);
    return false;
}

static Telescope collect_telescope(AST *ast) {
    Telescope telescope = {ast->tag, NULL, 0, ast};
    AST *cursor = ast;
    while (cursor && cursor->tag == ast->tag) {
        telescope.binder_count++;
        cursor = ast->tag == AST_LAMBDA ? cursor->value.lambda.body : cursor->value.forall.body;
    }

    telescope.binders = malloc(telescope.binder_count * sizeof(Binder *));
    if (!telescope.binders) {
        telescope.binder_count = 0;
        telescope.body = NULL;
        return telescope;
    }

    cursor = ast;
    for (size_t i = 0; i < telescope.binder_count; i++) {
        if (ast->tag == AST_LAMBDA) {
            telescope.binders[i] = &cursor->value.lambda.binder;
            cursor = cursor->value.lambda.body;
        } else {
            telescope.binders[i] = &cursor->value.forall.binder;
            cursor = cursor->value.forall.body;
        }
    }
    telescope.body = cursor;
    return telescope;
}

static Context *context_for_deps(DepInfo *deps, size_t local_limit, Expression **locals) {
    Context *context = deps->ambient;
    int local_index = deps->max_local_dep;
    if (local_index >= (int)local_limit) {
        local_index = dep_max_local_before(deps, local_limit);
    }
    if (local_index >= 0) {
        context = join_required_contexts(context, locals[local_index]);
    }
    return context;
}

static Context *context_from_summary(Context *ambient, int max_local_dep, Expression **locals) {
    Context *context = ambient;
    if (max_local_dep >= 0) {
        context = join_required_contexts(context, locals[max_local_dep]);
    }
    return context;
}

static bool init_dep_array(DepInfo *deps, size_t count, size_t local_count) {
    for (size_t i = 0; i < count; i++) {
        if (!dep_init(&deps[i], local_count)) {
            for (size_t j = 0; j < i; j++) {
                dep_free(&deps[j]);
            }
            return false;
        }
    }
    return true;
}

static void free_dep_array(DepInfo *deps, size_t count) {
    if (!deps) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        dep_free(&deps[i]);
    }
}

static bool compute_tail_summaries(DepInfo *type_deps, size_t count, DepInfo *body_deps,
                                   Context **tail_ambient_after, int *tail_max_before) {
    Context *ambient = body_deps->ambient;
    for (size_t i = count; i > 0; i--) {
        size_t idx = i - 1;
        tail_ambient_after[idx] = ambient;
        ambient = join_required_contexts(type_deps[idx].ambient, ambient);
        if (!ambient) {
            return false;
        }
    }

    int *active_until = malloc(count * sizeof(int));
    int *active_stack = malloc(count * sizeof(int));
    if ((!active_until || !active_stack) && count > 0) {
        free(active_until);
        free(active_stack);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        active_until[i] = -1;
        tail_max_before[i] = -1;
    }

    for (size_t i = 0; i < body_deps->local_dep_count; i++) {
        int k = body_deps->local_indices[i];
        active_until[k] = (int)count;
    }
    for (size_t j = 0; j < count; j++) {
        for (size_t dep_i = 0; dep_i < type_deps[j].local_dep_count; dep_i++) {
            int k = type_deps[j].local_indices[dep_i];
            if ((size_t)k < j && active_until[k] < (int)j - 1) {
                active_until[k] = (int)j - 1;
            } else if ((size_t)k >= j) {
                fprintf(stderr, ERROR "Binder type depends on an out-of-scope binder.\n" CRESET);
                free(active_until);
                free(active_stack);
                return false;
            }
        }
    }

    size_t active_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            size_t k = i - 1;
            if (active_until[k] >= (int)i) {
                active_stack[active_count++] = (int)k;
            }
        }
        while (active_count > 0 && active_until[active_stack[active_count - 1]] < (int)i) {
            active_count--;
        }
        if (active_count > 0) {
            tail_max_before[i] = active_stack[active_count - 1];
        }
    }

    free(active_until);
    free(active_stack);
    return true;
}

static Expression *elaborate_binder_telescope(AST *ast, Context *context,
                                              DoublyLinkedList *letbindings,
                                              TacticEnvEntry *tac_env) {
    Telescope telescope = collect_telescope(ast);
    if (!telescope.binders || telescope.binder_count == 0) {
        fprintf(stderr, ERROR "Failed to collect binder telescope.\n" CRESET);
        free(telescope.binders);
        return NULL;
    }

    size_t count = telescope.binder_count;
    DepInfo *type_deps = calloc(count, sizeof(DepInfo));
    DepInfo body_deps;
    bool body_deps_initialized = false;
    Context **tail_ambient_after = calloc(count, sizeof(Context *));
    int *tail_max_before = malloc(count * sizeof(int));
    Expression **vars = calloc(count, sizeof(Expression *));
    LocalDepName *local_nodes = calloc(count, sizeof(LocalDepName));
    if (!type_deps || !tail_ambient_after || !tail_max_before || !vars || !local_nodes ||
        !init_dep_array(type_deps, count, count) || !dep_init(&body_deps, count)) {
        fprintf(stderr, ERROR "Memory allocation failed during telescope elaboration.\n" CRESET);
        free_dep_array(type_deps, count);
        free(type_deps);
        free(tail_ambient_after);
        free(tail_max_before);
        free(vars);
        free(local_nodes);
        free(telescope.binders);
        return NULL;
    }
    body_deps_initialized = true;

    bool ok = true;
    LocalDepName *locals = NULL;
    for (size_t i = 0; ok && i < count; i++) {
        ok = ast_collect_deps(telescope.binders[i]->type, context, letbindings, tac_env, locals,
                              /*source_lets*/ NULL, &type_deps[i]);
        locals = push_local_dep(locals, &local_nodes[i], telescope.binders[i]->name, (int)i);
    }
    ok = ok && ast_collect_deps(telescope.body, context, letbindings, tac_env, locals,
                                /*source_lets*/ NULL, &body_deps);
    ok = ok && compute_tail_summaries(type_deps, count, &body_deps, tail_ambient_after,
                                      tail_max_before);

    for (size_t i = 0; ok && i < count; i++) {
        Context *type_ctx = context_for_deps(&type_deps[i], i, vars);
        Context *body_ctx =
            context_from_summary(tail_ambient_after[i], tail_max_before[i], vars);
        Context *scope_context = join_required_contexts(type_ctx, body_ctx);
        if (!scope_context) {
            ok = false;
            break;
        }

        Expression *binder_type =
            _ast_to_expression(telescope.binders[i]->type, scope_context, letbindings, tac_env);
        if (!binder_type) {
            fprintf(stderr, ERROR "Failed to create telescope binder type.\n" CRESET);
            ok = false;
            break;
        }

        vars[i] = kernel_var_create(telescope.binders[i]->name, binder_type, scope_context);
        if (!vars[i]) {
            fprintf(stderr, ERROR "Failed to create telescope bound var.\n" CRESET);
            ok = false;
            break;
        }
    }

    Expression *result = NULL;
    if (ok) {
        Context *body_context = context_for_deps(&body_deps, count, vars);
        result = _ast_to_expression(telescope.body, body_context, letbindings, tac_env);
        if (!result) {
            fprintf(stderr, ERROR "Failed to create telescope body.\n" CRESET);
            ok = false;
        }
    }

    for (size_t i = count; ok && i > 0; i--) {
        if (telescope.tag == AST_LAMBDA) {
            result = kernel_lambda_create(vars[i - 1], result);
        } else {
            result = kernel_forall_create(vars[i - 1], result);
        }
        if (!result) {
            fprintf(stderr, ERROR "Failed to close telescope binder.\n" CRESET);
            ok = false;
        }
    }

    free_dep_array(type_deps, count);
    if (body_deps_initialized) {
        dep_free(&body_deps);
    }
    free(type_deps);
    free(tail_ambient_after);
    free(tail_max_before);
    free(vars);
    free(local_nodes);
    free(telescope.binders);
    return ok ? result : NULL;
}

static Expression *_ast_to_expression(AST *ast, Context *context, DoublyLinkedList *letbindings,
                                      TacticEnvEntry *tac_env) {
    if (!ast) {
        return NULL;
    }

    switch (ast->tag) {
        case AST_VAR: {
            Expression *letbinding = letbindings_get(letbindings, ast->value.var.name);
            if (letbinding) {
                return letbinding;
            }

            // Check tactic let-binding environment (from TAC_LET / TAC_MATCH_*)
            for (TacticEnvEntry *e = tac_env; e; e = e->next) {
                if (strcmp(e->name, ast->value.var.name) == 0) {
                    if (!e->val) {
                        return NULL;
                    }
                    if (engine_tactic_value_kind(e->val) == ENGINE_TVAL_EXPRESSION) {
                        return engine_tactic_value_as_expr(e->val);
                    }
                    if (engine_tactic_value_kind(e->val) == ENGINE_TVAL_AST) {
                        /* Evaluate in lexical parent env to avoid self-recursion. */
                        return _ast_to_expression(engine_tactic_value_as_ast(e->val), context,
                                                  letbindings, e->next);
                    }
                    /* TVAL_PAIR: cannot appear in term position */
                    return NULL;
                }
            }

            Expression *val = kernel_context_lookup(context, ast->value.var.name);
            if (!val) {
                fprintf(stderr, ERROR "Variable %s not found in context.\n" CRESET,
                        ast->value.var.name);
                return NULL;
            }
            return val;
        }

        case AST_TYPE:
            return kernel_type_create();

        case AST_PROP:
            return kernel_prop_create();

        case AST_EXPR_REF:
            return engine_tactic_value_as_expr(ast->value.expr_ref.tval);

        case AST_LAMBDA: {
            return elaborate_binder_telescope(ast, context, letbindings, tac_env);
        }

        case AST_FORALL: {
            return elaborate_binder_telescope(ast, context, letbindings, tac_env);
        }

        case AST_APP: {
            Expression *func =
                _ast_to_expression(ast->value.app.func, context, letbindings, tac_env);
            if (!func) {
                fprintf(stderr, ERROR "Failed to create app func.\n" CRESET);
                return NULL;
            }

            Expression *arg = _ast_to_expression(ast->value.app.arg, context, letbindings, tac_env);
            if (!arg) {
                fprintf(stderr, ERROR "Failed to create app arg.\n" CRESET);
                return NULL;
            }

            return kernel_app_create(func, arg, context);
        }

        case AST_LET: {
            Expression *type =
                _ast_to_expression(ast->value.let.type, context, letbindings, tac_env);
            if (!type) {
                fprintf(stderr, ERROR "Failed to create let type.\n" CRESET);
                return NULL;
            }

            Expression *value =
                _ast_to_expression(ast->value.let.value, context, letbindings, tac_env);
            if (!value) {
                fprintf(stderr, ERROR "Failed to create let value.\n" CRESET);
                return NULL;
            }

            // Note: something to consider is the fact that we could using an
            // identifier that is already in use in the context. wrt the kernel,
            // this doesn't matter. But for the user, this could be confusing so
            // we'll fail here.
            if (kernel_context_has_name(context, ast->value.let.name)) {
                return NULL;
            }

            letbindings_set(letbindings, ast->value.let.name, value);

            return _ast_to_expression(ast->value.let.body, context, letbindings, tac_env);
        }

        case AST_MATCH: {
            Expression *scrutinee =
                _ast_to_expression(ast->value.match.scrutinee, context, letbindings, tac_env);
            if (!scrutinee) {
                return NULL;
            }

            int branch_count = ast->value.match.branch_count;
            void **branches = malloc(branch_count * sizeof(void *));
            if (!branches) {
                return NULL;
            }

            for (int i = 0; i < branch_count; i++) {
                AST *branch_ast = ast->value.match.branches[i];
                Pattern *pattern = branch_ast->value.matchbranch.pattern;

                Expression *constructor = kernel_context_lookup(context, pattern->constructor_name);
                if (!constructor) {
                    fprintf(stderr, ERROR "Constructor %s not found in context.\n" CRESET,
                            pattern->constructor_name);
                    free(branches);
                    return NULL;
                }

                // Create pattern variables
                int pattern_var_count = pattern->argument_count;
                Expression **pattern_variables = NULL;

                if (pattern_var_count > 0) {
                    pattern_variables = malloc(pattern_var_count * sizeof(Expression *));

                    Expression *ctor_type = kernel_expr_type(constructor);
                    Context *extended_context = context;

                    /* For parametric inductives, the first few foralls in the
                     * constructor type are type-parameter binders (e.g., A in
                     * sl_cons : ∀(A:Type). ∀(h:A). ...).  When matching on a
                     * scrutinee of type (sep_list A_fix), the type argument A_fix
                     * is already determined by the scrutinee.  We extract the type
                     * arguments from the scrutinee's type so that subsequent pattern
                     * variables get types that reference A_fix (via a delta-reducible
                     * alias), making `comb h` type-check correctly. */
                    Expression *scrutinee_type = kernel_expr_type(scrutinee);
                    DoublyLinkedList *type_args = dll_create();
                    {
                        Expression *head = scrutinee_type;
                        while (head != NULL && kernel_app_func(head) != NULL) {
                            dll_insert_at_head(type_args, dll_new_node(kernel_app_arg(head)));
                            head = kernel_app_func(head);
                        }
                    }
                    int scrutinee_param_count = dll_len(type_args);

                    for (int j = 0; j < pattern_var_count; j++) {
                        Expression *bound_var = kernel_forall_var(ctor_type);
                        if (!bound_var) {
                            fprintf(stderr, ERROR
                                    "Constructor has fewer parameters than pattern.\n" CRESET);
                            dll_destroy(type_args);
                            free(pattern_variables);
                            free(branches);
                            return NULL;
                        }

                        Expression *pattern_var;
                        if (j < scrutinee_param_count) {
                            /* Type-parameter slot: create a delta-reducible alias that
                             * expands to the corresponding type argument from the
                             * scrutinee. This makes the alias convertible with type_arg
                             * so later pattern variables' types unify with the outer type
                             * parameter used by functions like `comb`. */
                            Expression *type_arg = (Expression *)dll_at(type_args, j)->data;
                            pattern_var = kernel_var_create_with_body(pattern->argument_names[j],
                                                                      type_arg, extended_context);
                        } else {
                            Expression *param_type = kernel_expr_type(bound_var);
                            pattern_var = kernel_var_create(pattern->argument_names[j], param_type,
                                                            extended_context);
                        }

                        pattern_variables[j] = pattern_var;
                        extended_context = pattern_var;
                        ctor_type = kernel_forall_body(ctor_type);

                        /* Substitute bound_var -> pattern_var in the remaining constructor type.
                         * Essential for parametric constructors where later arg types
                         * reference earlier bound vars (type parameters). */
                        if (pattern_var && ctor_type) {
                            Expression *subst_ctor =
                                kernel_subst(extended_context, ctor_type, bound_var, pattern_var);
                            if (subst_ctor) {
                                ctor_type = subst_ctor;
                            }
                        }
                    }
                    dll_destroy(type_args);

                    Expression *body = _ast_to_expression(branch_ast->value.matchbranch.body,
                                                          extended_context, letbindings, tac_env);
                    if (!body) {
                        fprintf(stderr, ERROR "Failed to create match branch body.\n" CRESET);
                        free(pattern_variables);
                        free(branches);
                        return NULL;
                    }

                    branches[i] = kernel_match_branch_create(constructor, pattern_variables,
                                                             pattern_var_count, body);
                } else {
                    Expression *body = _ast_to_expression(branch_ast->value.matchbranch.body,
                                                          context, letbindings, tac_env);
                    if (!body) {
                        fprintf(stderr, ERROR "Failed to create match branch body.\n" CRESET);
                        free(branches);
                        return NULL;
                    }

                    branches[i] =
                        kernel_match_branch_create(constructor, /*pattern_variables*/ NULL,
                                                   /*pattern_var_count*/ 0, body);
                }

                if (!branches[i]) {
                    fprintf(stderr, ERROR "Failed to create match branch.\n" CRESET);
                    free(pattern_variables);
                    free(branches);
                    return NULL;
                }
            }

            Expression *match_expr =
                kernel_match_create(scrutinee, branches, branch_count, context);
            if (!match_expr) {
                fprintf(stderr, ERROR "Failed to create match expression.\n" CRESET);
                for (int i = 0; i < branch_count; i++) {
                    kernel_match_branch_free(branches[i]);
                }
                free(branches);
                return NULL;
            }

            return match_expr;
        }

        case AST_FIX: {
            int binder_count = ast->value.fix.binder_count;
            Expression *recursive_var = NULL;
            {
                Context **contexts = malloc((binder_count + 1) * sizeof(Context *));
                if (!contexts) {
                    fprintf(stderr, ERROR "Memory allocation failed for contexts.\n" CRESET);
                    return NULL;
                }

                contexts[0] = context;
                for (int i = 0; i < binder_count; i++) {
                    Expression *binder_type = _ast_to_expression(ast->value.fix.binders[i]->type,
                                                                 contexts[i], letbindings, tac_env);
                    if (!binder_type) {
                        fprintf(stderr,
                                ERROR "Failed to convert binder type in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    Expression *binder = kernel_var_create(ast->value.fix.binders[i]->name,
                                                           binder_type, contexts[i]);
                    if (!binder) {
                        fprintf(stderr,
                                ERROR "Failed to create binder in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    contexts[i + 1] = binder;
                }

                Expression *return_type = _ast_to_expression(
                    ast->value.fix.return_type, contexts[binder_count], letbindings, tac_env);
                if (!return_type) {
                    fprintf(stderr,
                            ERROR "Failed to convert return type in fix expression.\n" CRESET);
                    free(contexts);
                    return NULL;
                }

                // recursive var type: forall (x1 : A1) ... (xn : An), return_type
                Expression *rec_var_type = return_type;
                for (int i = binder_count - 1; i >= 0; i--) {
                    rec_var_type = kernel_forall_create(contexts[i + 1], rec_var_type);
                    if (!rec_var_type) {
                        fprintf(stderr, ERROR "Failed to create forall type.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }
                }

                recursive_var = kernel_var_create(ast->value.fix.name, rec_var_type, context);
                if (!recursive_var) {
                    fprintf(stderr,
                            ERROR "Failed to create recursive var in fix expression.\n" CRESET);
                    free(contexts);
                    return NULL;
                }
                free(contexts);
            }
            // Next, make the fix expression

            Expression *fix_expr = NULL;
            {
                Context **contexts = malloc((binder_count + 1) * sizeof(Context *));
                if (!contexts) {
                    fprintf(stderr, ERROR "Memory allocation failed for contexts.\n" CRESET);
                    return NULL;
                }

                contexts[0] = recursive_var;
                for (int i = 0; i < binder_count; i++) {
                    Expression *binder_type = _ast_to_expression(ast->value.fix.binders[i]->type,
                                                                 contexts[i], letbindings, tac_env);
                    if (!binder_type) {
                        fprintf(stderr,
                                ERROR "Failed to convert binder type in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    Expression *binder = kernel_var_create(ast->value.fix.binders[i]->name,
                                                           binder_type, contexts[i]);
                    if (!binder) {
                        fprintf(stderr,
                                ERROR "Failed to create binder in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    contexts[i + 1] = binder;
                }

                Expression *body = _ast_to_expression(ast->value.fix.body, contexts[binder_count],
                                                      letbindings, tac_env);
                if (!body) {
                    fprintf(stderr, ERROR "Failed to convert body in fix expression.\n" CRESET);
                    free(contexts);
                    return NULL;
                }

                Expression *decreasing_arg = kernel_context_lookup(
                    contexts[binder_count], ast->value.fix.decreasing_arg_name);
                if (!decreasing_arg) {
                    fprintf(stderr, ERROR "Decreasing argument '%s' not found in context.\n" CRESET,
                            ast->value.fix.decreasing_arg_name);
                    free(contexts);
                    return NULL;
                }

                int decreasing_arg_index = -1;
                for (int i = 0; i < binder_count; i++) {
                    if (decreasing_arg == contexts[i + 1]) {
                        decreasing_arg_index = i;
                        break;
                    }
                }

                if (decreasing_arg_index == -1) {
                    fprintf(stderr, ERROR "Decreasing argument '%s' not found in binders.\n" CRESET,
                            ast->value.fix.decreasing_arg_name);
                    free(contexts);
                    return NULL;
                }

                fix_expr = kernel_fix_create(recursive_var, contexts + 1, binder_count, body,
                                             decreasing_arg_index, contexts[0]);
                free(contexts);
                if (!fix_expr) {
                    fprintf(stderr, ERROR "Failed to create fix expression.\n" CRESET);
                    return NULL;
                }
            }

            // return the recursive variable here-- the fix expression will be registered as the
            // body of the recursive variable
            return recursive_var;
        }
        default:
            return NULL;
    }
}

Expression *ast_to_expression(AST *ast, Context *context) {
    if (!ast || !context) {
        return NULL;
    }

    DoublyLinkedList *letbindings = dll_create();
    Expression *result = _ast_to_expression(ast, context, letbindings, NULL);

    // Free the letbindings list and its contents
    if (letbindings) {
        DLLNode *current = letbindings->head;
        while (current) {
            LetBinding *binding = (LetBinding *)current->data;
            if (binding) {
                free(binding->name);
                free(binding);
            }
            current = current->next;
        }
        dll_destroy(letbindings);
    }

    return result;
}

Expression *ast_to_expression_env(AST *ast, Context *context, TacticEnvEntry *env) {
    if (!ast || !context) {
        return NULL;
    }

    DoublyLinkedList *letbindings = dll_create();
    Expression *result = _ast_to_expression(ast, context, letbindings, env);

    if (letbindings) {
        DLLNode *current = letbindings->head;
        while (current) {
            LetBinding *binding = (LetBinding *)current->data;
            if (binding) {
                free(binding->name);
                free(binding);
            }
            current = current->next;
        }
        dll_destroy(letbindings);
    }

    return result;
}

Expression *parse_string_to_expression(const char *input, Context *context) {
    if (!input || !context) {
        return NULL;
    }

    // todo: refactor to argument
    MEngineOptions options = {.debug = false};

    Lexer lexer;
    lexer_init(&lexer, input, &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    if (!ast) {
        return NULL;
    }

    Expression *expr = ast_to_expression(ast, context);
    free_ast(ast);

    return expr;
}

void free_ast(AST *ast) {
    if (!ast) {
        return;
    }

    switch (ast->tag) {
        case AST_VAR:
            free(ast->value.var.name);
            break;

        case AST_LAMBDA:
            free(ast->value.lambda.binder.name);
            free_ast(ast->value.lambda.binder.type);
            free_ast(ast->value.lambda.body);
            break;

        case AST_FORALL:
            free(ast->value.forall.binder.name);
            free_ast(ast->value.forall.binder.type);
            free_ast(ast->value.forall.body);
            break;

        case AST_APP:
            free_ast(ast->value.app.func);
            free_ast(ast->value.app.arg);
            break;

        case AST_MATCH:
            free_ast(ast->value.match.scrutinee);
            if (ast->value.match.branches) {
                for (size_t i = 0; i < ast->value.match.branch_count; i++) {
                    free_ast(ast->value.match.branches[i]);
                }
                free(ast->value.match.branches);
            }
            break;

        case AST_MATCHBRANCH:
            if (ast->value.matchbranch.pattern) {
                free(ast->value.matchbranch.pattern->constructor_name);
                for (int i = 0; i < ast->value.matchbranch.pattern->argument_count; i++) {
                    free(ast->value.matchbranch.pattern->argument_names[i]);
                }
                free(ast->value.matchbranch.pattern->argument_names);
                free(ast->value.matchbranch.pattern);
            }
            free_ast(ast->value.matchbranch.body);
            break;

        case AST_LET:
            free(ast->value.let.name);
            free_ast(ast->value.let.type);
            free_ast(ast->value.let.value);
            free_ast(ast->value.let.body);
            break;

        case AST_FIX:
            free(ast->value.fix.name);
            if (ast->value.fix.binders) {
                for (size_t i = 0; i < ast->value.fix.binder_count; i++) {
                    if (ast->value.fix.binders[i]) {
                        free(ast->value.fix.binders[i]->name);
                        free_ast(ast->value.fix.binders[i]->type);
                        free(ast->value.fix.binders[i]);
                    }
                }
                free(ast->value.fix.binders);
            }
            free(ast->value.fix.decreasing_arg_name);
            free_ast(ast->value.fix.return_type);
            free_ast(ast->value.fix.body);
            break;

        case AST_PATVAR:
            free(ast->value.patvar.name);
            break;

        case AST_EXPR_REF:
            engine_tactic_value_free(ast->value.expr_ref.tval);
            break;

        case AST_TYPE:
        case AST_PROP:
        default:
            break;
    }

    free(ast);
}
