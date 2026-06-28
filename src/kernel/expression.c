#include "src/kernel/expression.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/user.h>

#include "src/common/color.h"
#include "src/common/linear_map.h"
#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/conversion.h"
#include "src/kernel/inductive.h"
#include "src/kernel/order.h"
#include "src/kernel/structural.h"
#include "src/kernel/subst.h"
#include "src/kernel/type_compat.h"

#ifndef MENGINE_EVAR_FREE_FILL
#define MENGINE_EVAR_FREE_FILL 1
#endif

// Arena allocator for Expression nodes.
// Each page holds ~64 KiB of node payload (exact count falls out of
// sizeof(Expression), so it stays calibrated if the struct changes).
// Bump-allocating within a page avoids O(n) individual malloc/calloc calls
// and lets expression_gc_shutdown free memory with O(pages) free() calls
// instead of O(nodes).
#define EXPR_PAGE_NODES ((PAGE_SIZE) / (int)sizeof(Expression))

typedef struct ExprArenaPage {
    struct ExprArenaPage *next;
    int used;
    Expression nodes[EXPR_PAGE_NODES];
} ExprArenaPage;

static ExprArenaPage *g_arena_pages = NULL;
static ExprArenaPage *g_arena_current = NULL;

// Arena allocator for Uplink nodes (intrusive list nodes).
// Each page holds ~64 KiB of Uplink payload.
#define UPLINK_PAGE_NODES ((64 * 1024) / (int)sizeof(Uplink))

typedef struct UplinkPage {
    struct UplinkPage *next;
    int used;
    Uplink nodes[UPLINK_PAGE_NODES];
} UplinkPage;

static UplinkPage *g_uplink_pages = NULL;
static UplinkPage *g_uplink_current = NULL;

static Uplink *uplink_arena_alloc(void) {
    if (!g_uplink_current || g_uplink_current->used == UPLINK_PAGE_NODES) {
        UplinkPage *page = calloc(1, sizeof(UplinkPage));
        if (!page) {
            return NULL;
        }
        page->next = g_uplink_pages;
        g_uplink_pages = page;
        g_uplink_current = page;
    }
    return &g_uplink_current->nodes[g_uplink_current->used++];
}

static Expression *arena_alloc_expression(void) {
    if (!g_arena_current || g_arena_current->used == EXPR_PAGE_NODES) {
        ExprArenaPage *page = calloc(1, sizeof(ExprArenaPage));
        if (!page) {
            return NULL;
        }
        page->next = g_arena_pages;
        g_arena_pages = page;
        g_arena_current = page;
    }
    // Nodes are pre-zeroed by calloc; return next available slot.
    return &g_arena_current->nodes[g_arena_current->used++];
}

// Register a body to a recursive variable expression. WARNING: Mutator function, modifies the
// expression in place and returns true if successful, false otherwise.
bool register_fix_body_to_expression(Context *recursive_var, Expression *body) {
    if (recursive_var == NULL || body == NULL) {
        return false;
    }

    if (recursive_var->tag != VAR_EXPRESSION) {
        fprintf(stderr, ERROR "Expression is not a variable expression.\n" CRESET);
        return false;
    }

    if (recursive_var->as.var.body != NULL) {
        fprintf(stderr, ERROR "Variable already has a body.\n" CRESET);
        return false;
    }

    if (!valid_in_context(body, recursive_var)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return false;
    }

    Expression *body_type = get_expression_type(body);
    Expression *expression_type = get_expression_type(recursive_var);
    if (!conversion_holds_in_context(recursive_var, body_type, expression_type)) {
        fprintf(stderr, ERROR "Body type is not congruent to expression type.\n" CRESET);
        return false;
    }

    conversion_cache_clear();
    SET_VAR_BODY(recursive_var, body);

    return true;
}

Uplink *add_to_parents(Expression *expression, void *ptr, Relation r) {
    Uplink *ul = uplink_arena_alloc();
    if (!ul) {
        return NULL;
    }
    ul->ptr = ptr;
    ul->relation = r;
    ul->prev = NULL;
    ul->next = expression->uplinks;
    if (expression->uplinks) {
        expression->uplinks->prev = ul;
    }
    expression->uplinks = ul;
    expression->uplink_count++;
    return ul;
}

void remove_uplink_by_node(Expression *expr, Uplink *uplink_node) {
    if (expr == NULL || uplink_node == NULL) {
        return;
    }
    if (uplink_node->prev) {
        uplink_node->prev->next = uplink_node->next;
    } else {
        expr->uplinks = uplink_node->next;
    }
    if (uplink_node->next) {
        uplink_node->next->prev = uplink_node->prev;
    }
    /* uplink_node memory is arena-managed; not freed here */
    expr->uplink_count--;
}

void propagate_evar_refs(Expression *parent, Expression *child) {
    if (!parent || !child || !child->has_evar) {
        return;
    }

    parent->has_evar = true;
}

void remove_uplink(Expression *expr, void *parent_ptr, Relation rel) {
    if (expr == NULL || expr->uplinks == NULL) {
        return;
    }

    Uplink *current = expr->uplinks;
    while (current != NULL) {
        if (current->ptr == parent_ptr && current->relation == rel) {
            if (current->prev) {
                current->prev->next = current->next;
            } else {
                expr->uplinks = current->next;
            }
            if (current->next) {
                current->next->prev = current->prev;
            }
            /* current is arena-managed; not freed here */
            expr->uplink_count--;
            return;
        }
        current = current->next;
    }
}

// Garbage Collection

void free_expression(Expression *expr) { (void)expr; }

void free_expressions_excluding_context(Expression *a, Expression *b, Expression *shared_ctx) {
    (void)a;
    (void)b;
    (void)shared_ctx;
}

void free_expression_graph(Expression *root) { (void)root; }

void free_filled_hole(Expression *hole) { (void)hole; }

/* Pointer set used at shutdown to free each shared sub-allocation exactly once.
   MATCH branch arrays (and their MatchBranch structs / pattern-variable arrays)
   and FIX argument arrays are shared by pointer across distinct arena nodes —
   e.g. normalize/conversion rebuild a match with a new scrutinee but reuse the
   original branches array.  A naive per-node free would double-free them. */
typedef struct {
    uintptr_t *slots;
    size_t capacity;
    size_t count;
} PtrSet;

static void ptrset_init(PtrSet *set) {
    set->capacity = 1024;
    set->count = 0;
    set->slots = calloc(set->capacity, sizeof(uintptr_t));
}

static void ptrset_free(PtrSet *set) {
    free(set->slots);
    set->slots = NULL;
    set->capacity = 0;
    set->count = 0;
}

static size_t ptrset_slot(uintptr_t key, size_t mask) {
    return (size_t)((key >> 4) ^ (key >> 12)) & mask;
}

static void ptrset_grow(PtrSet *set) {
    uintptr_t *old_slots = set->slots;
    size_t old_capacity = set->capacity;
    set->capacity *= 2;
    set->slots = calloc(set->capacity, sizeof(uintptr_t));
    size_t mask = set->capacity - 1;
    for (size_t i = 0; i < old_capacity; i++) {
        if (!old_slots[i]) {
            continue;
        }
        size_t idx = ptrset_slot(old_slots[i], mask);
        while (set->slots[idx]) {
            idx = (idx + 1) & mask;
        }
        set->slots[idx] = old_slots[i];
    }
    free(old_slots);
}

// Returns true if ptr was newly inserted, false if it was already present.
static bool ptrset_insert(PtrSet *set, const void *ptr) {
    if (!ptr) {
        return false;
    }
    if ((set->count + 1) * 10 >= set->capacity * 7) {
        ptrset_grow(set);
    }
    uintptr_t key = (uintptr_t)ptr;
    size_t mask = set->capacity - 1;
    size_t idx = ptrset_slot(key, mask);
    while (set->slots[idx]) {
        if (set->slots[idx] == key) {
            return false;
        }
        idx = (idx + 1) & mask;
    }
    set->slots[idx] = key;
    set->count++;
    return true;
}

// Flat free of a single expression's non-expression heap allocations.  Shared
// sub-allocations are guarded by `freed` so each is released exactly once.
static void gc_free_node(Expression *expr, PtrSet *freed) {
    /* Uplink nodes are arena-managed; no per-node free needed. */
    expr->uplinks = NULL;

    // Free tag-specific non-expression allocations
    switch (expr->tag) {
        case VAR_EXPRESSION:
            order_on_delete(expr);
            free(expr->as.var.name);
            break;
        case MATCH_EXPRESSION:
            if (ptrset_insert(freed, expr->as.match.branches)) {
                for (int i = 0; i < expr->as.match.branch_count; i++) {
                    MatchBranch *branch = expr->as.match.branches[i];
                    if (ptrset_insert(freed, branch)) {
                        free(branch->pattern_variables);
                        free(branch);
                    }
                }
                free(expr->as.match.branches);
            }
            break;
        case FIX_EXPRESSION:
            if (ptrset_insert(freed, expr->as.fix.args)) {
                free(expr->as.fix.args);
            }
            break;
        case HOLE_EXPRESSION:
            free(expr->as.hole.name);
            break;
        default:
            break;
    }
}

// Walk the arena pages and free every tracked expression.
void expression_gc_shutdown(void) {
    inductive_registry_shutdown();
    conversion_cache_clear();

    PtrSet freed;
    ptrset_init(&freed);
    ExprArenaPage *page = g_arena_pages;
    while (page) {
        ExprArenaPage *next = page->next;
        for (int i = 0; i < page->used; i++) {
            gc_free_node(&page->nodes[i], &freed);
        }
        free(page);
        page = next;
    }
    ptrset_free(&freed);
    g_arena_pages = NULL;
    g_arena_current = NULL;

    UplinkPage *upage = g_uplink_pages;
    while (upage) {
        UplinkPage *unext = upage->next;
        free(upage);
        upage = unext;
    }
    g_uplink_pages = NULL;
    g_uplink_current = NULL;

    TYPE = NULL;
    PROP = NULL;
}

// Helper to construct a lambda type from a bound variable and body.
// Assumes all inputs are valid.
Expression *_construct_lambda_type(Expression *bound_variable, Expression *body) {
    return init_forall_expression_wc(bound_variable, get_expression_type(body));
}

// Helper to construct a app type from a function and argument.
// Assumes all inputs are valid.
Expression *_construct_app_type(Context *context, Expression *func, Expression *arg) {
    Expression *func_type = get_expression_type(func);  // Forall x: A, B
    Expression *weak_func_type = weak_head_normalize(func_type);
    if (weak_func_type->tag != FORALL_EXPRESSION) {
        fprintf(stderr, ERROR "Trying to apply a non-function.\n" CRESET);
        return NULL;
    }
    Expression *variable = get_forall_bound_variable(weak_func_type);  // x
    Expression *expected_arg_type = get_expression_type(variable);     // A
    Expression *actual_arg_type = get_expression_type(arg);            // A?
    Expression *return_type = get_forall_body(weak_func_type);         // B

    Expression *result = NULL;
    if (actual_arg_type == expected_arg_type ||
        (actual_arg_type->tag == PROP_EXPRESSION && expected_arg_type->tag == TYPE_EXPRESSION) ||
        conversion_holds_in_context(context, actual_arg_type, expected_arg_type)) {
        // If variable does not appear free in return_type (i.e. variable is not in
        // return_type's context chain), the substitution return_type[variable -> arg]
        // is trivially return_type itself.  Skip the new_subst call in that case.
        if (context_find(get_expression_context(return_type), variable) == NULL) {
            result = return_type;
        } else {
            result = new_subst(context, return_type, variable, arg);  // B[x -> arg]
        }
    } else {
        fprintf(stderr, ERROR "Application does not type check.\n" CRESET);
    }
    return result;
}

Expression *_init_expression_base(ExpressionType tag, Context *context, int ctx_size,
                                  Expression *type) {
    Expression *expr = arena_alloc_expression();
    if (!expr) {
        return NULL;
    }

    SET_EXPR_TAG(expr, tag);
    // uplinks are lazily allocated in add_to_parents
    SET_EXPR_CONTEXT(expr, context);
    SET_EXPR_CTX_SIZE(expr, ctx_size);
    SET_EXPR_TYPE(expr, type);
    propagate_evar_refs(expr, type);

    return expr;
}

Expression *init_prop_expression() {
    if (PROP == NULL) {
        PROP =
            _init_expression_base(/* tag */ PROP_EXPRESSION, /* context */ context_create_empty(),
                                  /* ctx_size */ 0, /* type */ init_type_expression());
    }
    return PROP;
}

Expression *init_type_expression() {
    if (TYPE == NULL) {
        Context *context = context_create_empty();
        // Special case: Type's type is recursive, so we need to create it manually.
        TYPE = arena_alloc_expression();
        if (!TYPE) {
            return NULL;
        }

        SET_EXPR_TAG(TYPE, TYPE_EXPRESSION);
        SET_EXPR_CONTEXT(TYPE, context);
        SET_EXPR_CTX_SIZE(TYPE, 0);
        SET_EXPR_TYPE(TYPE, init_type_expression());
    }
    return TYPE;
}

Expression *init_hole_expression(char *name, Expression *type, Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ HOLE_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ type);

    SET_HOLE_NAME(expr, strdup(name ? name : "_"));
    expr->has_evar = true;  // A hole always contains itself
    return expr;
}

Expression *init_var_expression_wc(const char *name, Expression *type, Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ VAR_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size + 1,
                                             /* type */ type);

    SET_VAR_NAME(expr, strdup(name));
    order_on_insert(gamma, expr);
    return expr;
}

Expression *init_var_expression_wc_with_body(const char *name, Expression *body, Context *gamma) {
    if (!valid_in_context(body, gamma)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type = get_expression_type(body);
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ VAR_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size + 1,
                                             /* type */ type);

    SET_VAR_NAME(expr, strdup(name));
    SET_VAR_BODY(expr, body);
    propagate_evar_refs(expr, body);
    order_on_insert(gamma, expr);
    return expr;
}

Expression *init_lambda_expression_wc(Expression *bound_variable, Expression *body) {
    Context *gamma = get_expression_context(bound_variable);
    Context *extended_with_bound_variable = bound_variable;

    if (!valid_in_context(body, extended_with_bound_variable)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *expr =
        _init_expression_base(/* tag */ LAMBDA_EXPRESSION, /* context */ gamma,
                              /* ctx_size */ gamma->ctx_size,
                              /* type */ _construct_lambda_type(bound_variable, body));

    SET_LAMBDA_BOUND_VAR(expr, bound_variable);
    SET_LAMBDA_BODY(expr, body);
    propagate_evar_refs(expr, bound_variable);
    propagate_evar_refs(expr, body);

    return expr;
}

Expression *init_app_expression_wc_with_known_type(Expression *func, Expression *arg,
                                                   Context *context, Expression *type) {
    Expression *expr = _init_expression_base(
        /* tag */ APP_EXPRESSION, /* context */ context,
        /* ctx_size */ context->ctx_size,
        /* type */ type);

    SET_APP_FUNC(expr, func);
    SET_APP_ARG(expr, arg);
    propagate_evar_refs(expr, func);
    propagate_evar_refs(expr, arg);

    return expr;
}

Expression *init_app_expression_wc(Expression *func, Expression *arg, Context *context) {
    if (!func || !arg || !context) {
        fprintf(stderr, ERROR "Application input is NULL.\n" CRESET);
        return NULL;
    }

    if (!valid_in_context(func, context)) {
        fprintf(stderr, ERROR "Function is not valid in context.\n" CRESET);
        return NULL;
    }

    // Whether `func`'s type is a Pi is validated by _construct_app_type below, which
    // weak-head-normalizes first. That matters for functions whose type is a Pi only
    // up to reduction (e.g. an induction hypothesis typed by the eliminator's
    // beta-redex `(motive) x`); a premature syntactic FORALL check here would reject
    // them. _construct_app_type returns NULL (and reports) for genuine non-functions.

    if (!valid_in_context(arg, context)) {
        fprintf(stderr, ERROR "Argument is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type = _construct_app_type(context, func, arg);
    if (!type) {
        return NULL;
    }

    Expression *expr = _init_expression_base(
        /* tag */ APP_EXPRESSION, /* context */ context,
        /* ctx_size */ context->ctx_size,
        /* type */ type);

    SET_APP_FUNC(expr, func);
    SET_APP_ARG(expr, arg);
    propagate_evar_refs(expr, func);
    propagate_evar_refs(expr, arg);

    return expr;
}

Expression *init_forall_expression_wc(Expression *bound_variable, Expression *body) {
    Context *gamma = get_expression_context(bound_variable);
    Context *extended_with_bound_variable = bound_variable;

    if (!valid_in_context(body, extended_with_bound_variable)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *body_type = get_expression_type(body);
    if (body_type->tag != PROP_EXPRESSION && body_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Body type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *bound_variable_type = get_expression_type(bound_variable);
    if (body_type->tag == TYPE_EXPRESSION && !valid_in_context(bound_variable_type, gamma)) {
        fprintf(stderr, ERROR "Bound variable type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ FORALL_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ body_type);

    SET_FORALL_BOUND_VAR(expr, bound_variable);
    SET_FORALL_BODY(expr, body);
    propagate_evar_refs(expr, bound_variable);
    propagate_evar_refs(expr, body);

    return expr;
}

Expression *init_arrow_expression_wc(Expression *lhs, Expression *rhs, Context *gamma) {
    // lhs -> rhs <-> Forall _: lhs, rhs
    Expression *unnamed_variable = init_var_expression_wc("_", lhs, gamma);
    return init_forall_expression_wc(unnamed_variable, rhs);
}

Expression *init_match_expression_wc(Expression *scrutinee, MatchBranch **branches,
                                     int branch_count, Context *context) {
    // gamma |- scrutinee : I
    if (!valid_in_context(scrutinee, context)) {
        fprintf(stderr, ERROR "Match scrutinee is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *scrutinee_type = get_expression_type(scrutinee);
    if (!is_inductive(scrutinee_type)) {
        fprintf(stderr, ERROR "Match scrutinee must have an inductive type.\n" CRESET);
        return NULL;
    }

    // For each branch i: gamma, pattern_vars[i] : type(pattern_vars[i]) |- body_i : T
    int expected_ctor_count;
    Expression **expected_ctors = get_constructors(scrutinee_type, &expected_ctor_count);
    if (!expected_ctors || expected_ctor_count == 0) {
        fprintf(stderr, ERROR "Failed to get constructors for inductive type.\n" CRESET);
        return NULL;
    }

    if (branch_count != expected_ctor_count) {
        fprintf(stderr, ERROR "Non-exhaustive match: expected %d branches, got %d.\n" CRESET,
                expected_ctor_count, branch_count);
        return NULL;
    }

    // We combine two check here: 1) use a bool array to track which constructors have been covered,
    // and 2) verify gamma, pattern_vars[i] : type(pattern_vars[i]) |- body_i : T
    bool *constructor_covered = calloc(expected_ctor_count, sizeof(bool));
    Expression *match_type = NULL;

    for (int i = 0; i < branch_count; i++) {
        MatchBranch *branch = branches[i];

        // Find which constructor this branch matches
        int ctor_idx = -1;
        for (int j = 0; j < expected_ctor_count; j++) {
            if (congruence(branch->constructor, expected_ctors[j])) {
                ctor_idx = j;
                break;
            }
        }

        if (ctor_idx == -1) {
            fprintf(stderr, ERROR "Branch constructor not found in inductive type.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }

        if (constructor_covered[ctor_idx]) {
            fprintf(stderr, ERROR "Duplicate pattern for constructor.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }
        constructor_covered[ctor_idx] = true;

        // Verify gamma, pattern_vars[i] : type(pattern_vars[i]) |- body[i] : T
        // TODO: rethink how match expression contexts should be handled
        Expression *ctor_type = get_expression_type(branch->constructor);
        int expected_args = 0;
        Expression *temp_type = ctor_type;
        while (temp_type->tag == FORALL_EXPRESSION) {
            expected_args++;
            temp_type = get_forall_body(temp_type);
        }

        if (branch->pattern_var_count != expected_args) {
            fprintf(stderr, ERROR "Pattern variable count mismatch: expected %d, got %d.\n" CRESET,
                    expected_args, branch->pattern_var_count);
            free(constructor_covered);
            return NULL;
        }

        Expression *branch_body_bound = branch->body;
        if (branch->pattern_var_count == 0 && !valid_in_context(branch_body_bound, context)) {
            fprintf(stderr, ERROR "Branch body is not valid in context.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }

        // All branches must have the same type up to definitional equality.
        Expression *branch_body_type = get_expression_type(branch->body);
        if (match_type == NULL) {
            // Transport the seed type out to the outer match `context` by
            // substituting away this branch's parameter-slot pattern variables
            // (each a delta-alias to one of the scrutinee's type arguments).
            // Otherwise match_type keeps *this* branch's pattern-var context,
            // which is not an ancestor of a sibling branch's context, so the
            // (sound) convertibility check below spuriously rejects a valid
            // non-dependent match whose branch type mentions the inductive's
            // parameter -- e.g. a `nil A` branch, of type `list A`.  (app/length
            // escape only because their first branch is a variable/`O`, whose
            // type already lives in the outer context.)  Substituting a
            // delta-alias for its body is meaning-preserving, so this only
            // relocates the type, never changes it.
            Expression *seed = branch_body_type;
            for (int j = branch->pattern_var_count - 1; j >= 0; j--) {
                Expression *pv = branch->pattern_variables[j];
                Expression *pv_body = get_var_body(pv);
                if (pv_body != NULL) {
                    Expression *lowered =
                        new_subst(get_expression_context(seed), seed, pv, pv_body);
                    if (lowered != NULL) {
                        seed = lowered;
                    }
                }
            }
            match_type = seed;
        } else if (!conversion_holds_in_context(get_expression_context(branch->body),
                                                branch_body_type, match_type)) {
            fprintf(stderr, ERROR "Branch body types do not match.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }
    }

    free(constructor_covered);

    if (!match_type) {
        fprintf(stderr, ERROR "Match expression has no type (no branches).\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(
        /* tag */ MATCH_EXPRESSION, /* context */ context,
        /* ctx_size */ context->ctx_size,
        /* type */ match_type);

    expr->as.match.branch_count = branch_count;  // Set branch_count BEFORE calling macros
    SET_MATCH_SCRUTINEE(expr, scrutinee);
    SET_MATCH_BRANCHES(expr, branches);
    propagate_evar_refs(expr, scrutinee);
    for (int i = 0; i < branch_count; i++) {
        propagate_evar_refs(expr, branches[i]->constructor);
        for (int j = 0; j < branches[i]->pattern_var_count; j++) {
            propagate_evar_refs(expr, branches[i]->pattern_variables[j]);
        }
        propagate_evar_refs(expr, branches[i]->body);
    }

    return expr;
}

// Helper to extract the nth argument from an application chain
Expression *get_nth_app_arg(Expression *app, int n) {
    DoublyLinkedList *args = dll_create();
    Expression *curr = app;

    while (curr->tag == APP_EXPRESSION) {
        dll_insert_at_head(args, dll_new_node(get_app_arg(curr)));
        curr = get_app_func(curr);
    }

    if (n >= dll_len(args)) {
        dll_destroy(args);
        return NULL;
    }

    Expression *result = dll_at(args, n)->data;
    dll_destroy(args);
    return result;
}

// Map from pattern variables to their scrutinee
// Records: "pattern_var is directly structurally smaller than scrutinee"
static bool check_all_recursive_calls_with_context(Expression *body, Expression *rec_var,
                                                   Expression *decreasing_arg, int decreasing_idx,
                                                   LinearMap *pattern_to_scrutinee);

static bool check_all_recursive_calls(Expression *body, Expression *rec_var,
                                      Expression *decreasing_arg, int decreasing_idx) {
    LinearMap *empty_map = linear_map_new();
    bool result = check_all_recursive_calls_with_context(body, rec_var, decreasing_arg,
                                                         decreasing_idx, empty_map);
    linear_map_free(empty_map);
    return result;
}

static bool check_all_recursive_calls_with_context(Expression *body, Expression *rec_var,
                                                   Expression *decreasing_arg, int decreasing_idx,
                                                   LinearMap *pattern_to_scrutinee) {
    switch (body->tag) {
        case VAR_EXPRESSION:
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
        case HOLE_EXPRESSION:
            return true;

        case APP_EXPRESSION: {
            Expression *head = get_head(body);
            if (congruence(head, rec_var)) {
                Expression *actual_arg = get_nth_app_arg(body, decreasing_idx);

                // Check if actual_arg is a pattern variable
                Expression *scrutinee_of_pattern = linear_map_get(pattern_to_scrutinee, actual_arg);
                if (scrutinee_of_pattern) {
                    // actual_arg is directly structurally smaller than scrutinee_of_pattern
                    // Check if scrutinee_of_pattern is the decreasing_arg or smaller
                    if (congruence(scrutinee_of_pattern, decreasing_arg)) {
                        return true;
                    }
                    return term_structurally_smaller_than_arg(scrutinee_of_pattern, decreasing_arg);
                }

                return term_structurally_smaller_than_arg(actual_arg, decreasing_arg);
            }

            // Recursively check subexpressions
            return (check_all_recursive_calls_with_context(get_app_func(body), rec_var,
                                                           decreasing_arg, decreasing_idx,
                                                           pattern_to_scrutinee) &&
                    check_all_recursive_calls_with_context(get_app_arg(body), rec_var,
                                                           decreasing_arg, decreasing_idx,
                                                           pattern_to_scrutinee)) != 0;
        }

        case LAMBDA_EXPRESSION:
            return check_all_recursive_calls_with_context(get_lambda_body(body), rec_var,
                                                          decreasing_arg, decreasing_idx,
                                                          pattern_to_scrutinee);

        case FORALL_EXPRESSION:
            return check_all_recursive_calls_with_context(get_forall_body(body), rec_var,
                                                          decreasing_arg, decreasing_idx,
                                                          pattern_to_scrutinee);

        case MATCH_EXPRESSION: {
            if (!check_all_recursive_calls_with_context(body->as.match.scrutinee, rec_var,
                                                        decreasing_arg, decreasing_idx,
                                                        pattern_to_scrutinee)) {
                return false;
            }

            Expression *scrutinee = body->as.match.scrutinee;

            for (int i = 0; i < body->as.match.branch_count; i++) {
                MatchBranch *branch = body->as.match.branches[i];

                // Create extended map for this branch
                LinearMap *branch_map = linear_map_new();

                // Copy existing mappings
                for (int j = 0; j < pattern_to_scrutinee->size; j++) {
                    LinearMapItem *item = &pattern_to_scrutinee->items[j];
                    if (item->key) {
                        linear_map_set(branch_map, item->key, item->val);
                    }
                }

                // Add new mappings: each pattern variable is directly structurally smaller than
                // scrutinee
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    linear_map_set(branch_map, branch->pattern_variables[j], scrutinee);
                }

                bool branch_ok = check_all_recursive_calls_with_context(
                    branch->body, rec_var, decreasing_arg, decreasing_idx, branch_map);

                linear_map_clear_free(branch_map);

                if (!branch_ok) {
                    return false;
                }
            }
            return true;
        }

        case FIX_EXPRESSION:
            return check_all_recursive_calls_with_context(
                body->as.fix.body, rec_var, decreasing_arg, decreasing_idx, pattern_to_scrutinee);

        default:
            return true;
    }
}

Expression *init_fix_expression_wc(Expression *recursive_var, Expression **args, int arg_count,
                                   int decreasing_arg_index, Expression *body) {
    Context *gamma = get_expression_context(recursive_var);
    int expected_args = 0;
    Expression *temp_type = get_expression_type(recursive_var);
    while (temp_type->tag == FORALL_EXPRESSION) {
        expected_args++;
        temp_type = get_forall_body(temp_type);
    }

    if (arg_count != expected_args) {
        fprintf(stderr, ERROR "Argument count mismatch: expected %d, got %d.\n" CRESET,
                expected_args, arg_count);
        return NULL;
    }

    // Build extended context: gamma -> recursive_var -> args[0] -> ... -> args[n-1]
    Context *extended = recursive_var;
    for (int i = 0; i < arg_count; i++) {
        extended = args[i];
    }

    if (!valid_in_context(body, extended)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    // Check all recursive calls satisfy structural recursion
    if (decreasing_arg_index >= 0 && decreasing_arg_index < arg_count) {
        Expression *decreasing_arg = args[decreasing_arg_index];
        if (!check_all_recursive_calls(body, recursive_var, decreasing_arg, decreasing_arg_index)) {
            fprintf(stderr, ERROR "Structural recursion check failed.\n" CRESET);
            return NULL;
        }
    }

    Expression *expr = _init_expression_base(/* tag */ FIX_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ get_expression_type(recursive_var));

    // Copy args into a fresh heap allocation so gc_free_node can safely free it.
    Expression **args_copy = malloc(arg_count * sizeof(Expression *));
    for (int i = 0; i < arg_count; i++) {
        args_copy[i] = args[i];
    }

    SET_FIX_RECURSIVE_VAR(expr, recursive_var);
    SET_FIX_ARG_COUNT(expr, arg_count);
    SET_FIX_ARGS(expr, args_copy);
    SET_FIX_DECREASING_ARG_INDEX(expr, decreasing_arg_index);
    SET_FIX_BODY(expr, body);
    propagate_evar_refs(expr, recursive_var);
    for (int i = 0; i < arg_count; i++) {
        propagate_evar_refs(expr, args[i]);
    }
    propagate_evar_refs(expr, body);

    // Register the fix node itself as the recursive variable's definition. Unfolding
    // the constant therefore yields a fix node (a value), so reduction is governed by
    // the guarded fix rule (see fix_reduce_app): it fires only once the decreasing
    // argument is constructor-headed. Registering the eta-expanded lambda form instead
    // would make the constant delta-unfold unconditionally and loop on symbolic
    // recursive arguments.
    if (!register_fix_body_to_expression(recursive_var, expr)) {
        fprintf(stderr, ERROR "Failed to register body to recursive variable.\n" CRESET);
        free_expression(expr);
        return NULL;
    }

    return expr;
}

Uplink *get_expression_uplinks(Expression *expression) { return expression->uplinks; }

Expression *get_expression_type(Expression *expression) { return expression->type; }

Expression *get_expression_body(Expression *expression) {
    if (expression->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expression->as.var.body;
}

Context *get_expression_context(Expression *expression) { return expression->context; }

Expression *get_innermost_body(Expression *e) {
    if (e->tag == LAMBDA_EXPRESSION) {
        return get_innermost_body(e->as.lambda.body);
    }
    if (e->tag == FORALL_EXPRESSION) {
        return get_innermost_body(e->as.forall.body);
    }
    return e;
}

Expression *get_head(Expression *e) {
    if (e->tag == APP_EXPRESSION) {
        return get_head(e->as.app.func);
    }
    return e;
}

char *get_var_name(Expression *expr) {
    if (expr->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expr->as.var.name;
}

Expression *get_var_body(Expression *expr) {
    if (expr->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expr->as.var.body;
}

char *get_hole_name(Expression *expr) {
    if (expr->tag != HOLE_EXPRESSION) {
        return NULL;
    }

    return expr->as.hole.name;
}

Expression *get_app_func(Expression *expr) {
    if (expr->tag != APP_EXPRESSION) {
        return NULL;
    }

    return expr->as.app.func;
}

Expression *get_app_arg(Expression *expr) {
    if (expr->tag != APP_EXPRESSION) {
        return NULL;
    }

    return expr->as.app.arg;
}

Expression *get_forall_bound_variable(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return expr->as.forall.bound_variable;
}

Expression *get_lambda_bound_variable(Expression *expr) {
    if (expr->tag != LAMBDA_EXPRESSION) {
        return NULL;
    }

    return expr->as.lambda.bound_variable;
}

Expression *get_match_scrutinee(Expression *expr) {
    if (expr->tag != MATCH_EXPRESSION) {
        return NULL;
    }

    return expr->as.match.scrutinee;
}

Expression *get_match_branch_body(Expression *expr, int index) {
    if (expr->tag != MATCH_EXPRESSION) {
        return NULL;
    }

    return expr->as.match.branches[index]->body;
}

Expression **get_match_branch_pattern_variables(Expression *expr, int index) {
    if (expr->tag != MATCH_EXPRESSION) {
        return NULL;
    }

    return expr->as.match.branches[index]->pattern_variables;
}

int get_match_branch_pattern_var_count(Expression *expr, int index) {
    if (expr->tag != MATCH_EXPRESSION) {
        return -1;
    }

    return expr->as.match.branches[index]->pattern_var_count;
}

Expression *get_fix_recursive_var(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return NULL;
    }

    return expr->as.fix.recursive_var;
}

Expression **get_fix_args(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return NULL;
    }

    return expr->as.fix.args;
}

int get_fix_arg_count(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return -1;
    }

    return expr->as.fix.arg_count;
}

int get_fix_decreasing_arg_index(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return -1;
    }

    return expr->as.fix.decreasing_arg_index;
}

Expression *get_fix_body(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return NULL;
    }

    return expr->as.fix.body;
}

int get_arity(Expression *expr) {
    Expression *type = get_expression_type(expr);
    int arity = 0;

    while (type->tag == FORALL_EXPRESSION) {
        arity++;
        type = get_forall_body(type);
    }

    return arity;
}

Expression *get_forall_body(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return expr->as.forall.body;
}

Expression *get_lambda_body(Expression *expr) {
    if (expr->tag != LAMBDA_EXPRESSION) {
        return NULL;
    }

    return expr->as.lambda.body;
}

Expression *get_arrow_lhs(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return get_expression_type(get_forall_bound_variable(expr));
}

Expression *get_arrow_rhs(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return get_forall_body(expr);
}

// Forward declarations. No need to expose them in expression.h.
bool _congruence(Expression *a, Expression *b, Map *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return true;
    }

    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
        case (TYPE_EXPRESSION):
            return true;
        case (PROP_EXPRESSION):
            return true;
        case (APP_EXPRESSION):
            return (_congruence(a->as.app.func, b->as.app.func, mapping) &&
                    _congruence(a->as.app.arg, b->as.app.arg, mapping)) != 0;
        case (FORALL_EXPRESSION): {
            Expression *bv_a = a->as.forall.bound_variable;
            Expression *bv_b = b->as.forall.bound_variable;
            map_set(mapping, bv_a, bv_b);
            bool result = _congruence(a->as.forall.body, b->as.forall.body, mapping);
            map_del(mapping, bv_a);
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            Expression *bv_a = a->as.lambda.bound_variable;
            Expression *bv_b = b->as.lambda.bound_variable;
            map_set(mapping, bv_a, bv_b);
            bool result = _congruence(a->as.lambda.body, b->as.lambda.body, mapping);
            map_del(mapping, bv_a);
            return result;
        }
        case (VAR_EXPRESSION): {
            return ((a == b) || (map_get(mapping, a) == b)) != 0;
        }
        case (HOLE_EXPRESSION): {
            return ((a == b) || (map_get(mapping, a) == b)) != 0;
        }
        case (MATCH_EXPRESSION): {
            if (!_congruence(a->as.match.scrutinee, b->as.match.scrutinee, mapping)) {
                return false;
            }
            if (a->as.match.branch_count != b->as.match.branch_count) {
                return false;
            }
            for (int i = 0; i < a->as.match.branch_count; i++) {
                MatchBranch *branch_a = a->as.match.branches[i];
                MatchBranch *branch_b = b->as.match.branches[i];

                if (!_congruence(branch_a->constructor, branch_b->constructor, mapping)) {
                    return false;
                }
                if (branch_a->pattern_var_count != branch_b->pattern_var_count) {
                    return false;
                }

                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    map_set(mapping, branch_a->pattern_variables[j],
                            branch_b->pattern_variables[j]);
                }

                bool body_result = _congruence(branch_a->body, branch_b->body, mapping);

                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    map_del(mapping, branch_a->pattern_variables[j]);
                }

                if (!body_result) {
                    return false;
                }
            }
            return true;
        }
        case (FIX_EXPRESSION): {
            Expression *rv_a = a->as.fix.recursive_var;
            Expression *rv_b = b->as.fix.recursive_var;
            map_set(mapping, rv_a, rv_b);

            if (a->as.fix.arg_count != b->as.fix.arg_count) {
                map_del(mapping, rv_a);
                return false;
            }

            if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                map_del(mapping, rv_a);
                return false;
            }

            for (int i = 0; i < a->as.fix.arg_count; i++) {
                map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
            }

            bool result = _congruence(a->as.fix.body, b->as.fix.body, mapping);

            for (int i = 0; i < a->as.fix.arg_count; i++) {
                map_del(mapping, a->as.fix.args[i]);
            }
            map_del(mapping, rv_a);
            return result;
        }
    }
    return false;
}

bool congruence(Expression *a, Expression *b) {
    Map *mapping = map_new_with_capacity(8);
    bool result = _congruence(a, b, mapping);
    map_free(mapping);
    return result;
}

void _match_and_subst(Expression *a, Expression *b, LinearMap *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return;
    }

    switch (a->tag) {
        case (TYPE_EXPRESSION):
            break;
        case (PROP_EXPRESSION):
            break;
        case (APP_EXPRESSION):
            _match_and_subst(a->as.app.func, b->as.app.func, mapping);
            _match_and_subst(a->as.app.arg, b->as.app.arg, mapping);
            break;
        case (FORALL_EXPRESSION): {
            linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
            _match_and_subst(a->as.forall.body, b->as.forall.body, mapping);
            break;
        }
        case (LAMBDA_EXPRESSION): {
            linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            _match_and_subst(a->as.lambda.body, b->as.lambda.body, mapping);
            break;
        }
        case (VAR_EXPRESSION): {
            if (a != b) {
                (linear_map_set(mapping, a, b));
            }
            break;
        }
        case (HOLE_EXPRESSION): {
            if (a != b) {
                (linear_map_set(mapping, a, b));
            }
            break;
        }
        case (MATCH_EXPRESSION): {
            _match_and_subst(a->as.match.scrutinee, b->as.match.scrutinee, mapping);
            for (int i = 0; i < a->as.match.branch_count; i++) {
                MatchBranch *branch_a = a->as.match.branches[i];
                MatchBranch *branch_b = b->as.match.branches[i];
                _match_and_subst(branch_a->constructor, branch_b->constructor, mapping);
                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    linear_map_set(mapping, branch_a->pattern_variables[j],
                                   branch_b->pattern_variables[j]);
                }
                _match_and_subst(branch_a->body, branch_b->body, mapping);
            }
            break;
        }
        case (FIX_EXPRESSION): {
            linear_map_set(mapping, a->as.fix.recursive_var, b->as.fix.recursive_var);
            for (int i = 0; i < a->as.fix.arg_count; i++) {
                linear_map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
            }
            _match_and_subst(a->as.fix.body, b->as.fix.body, mapping);
            break;
        }
        default:
            fprintf(stderr, ERROR "Unsupported expression type in _match_and_subst.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

Expression *match_and_subst(Expression *a, Expression *b, Expression *to_subst) {
    LinearMap *mapping = linear_map_new();
    _match_and_subst(a, b, mapping);

    Map *subst_map = map_new_with_capacity(mapping->size > 0 ? mapping->size : 1);
    for (int i = 0; i < mapping->size; i++) {
        map_set(subst_map, (mapping->items + i)->key, (mapping->items + i)->val);
    }

    Context *to_subst_ctx = get_expression_context(to_subst);
    Expression *result = new_p_subst(to_subst_ctx, to_subst, subst_map);

    map_free(subst_map);
    free(mapping->items);
    free(mapping);
    return result;
}

bool has_holes(Expression *expr) { return expr && expr->has_evar; }

bool is_hole(Expression *expr) { return expr->tag == HOLE_EXPRESSION; }

static bool expr_refs_hole_slow_rec(Expression *expr, Expression *hole, Map *seen) {
    if (!expr) {
        return false;
    }
    if (expr == hole) {
        return true;
    }
#if MENGINE_EVAR_FREE_FILL
    if (!expr->has_evar) {
        return false;
    }
#endif
    if (map_get(seen, expr)) {
        return false;
    }
    map_set(seen, expr, expr);

    if (expr_refs_hole_slow_rec(get_expression_type(expr), hole, seen)) {
        return true;
    }

    switch (expr->tag) {
        case HOLE_EXPRESSION:
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return false;
        case VAR_EXPRESSION:
            return expr_refs_hole_slow_rec(expr->as.var.body, hole, seen);
        case LAMBDA_EXPRESSION:
            return expr_refs_hole_slow_rec(expr->as.lambda.bound_variable, hole, seen) ||
                   expr_refs_hole_slow_rec(expr->as.lambda.body, hole, seen);
        case APP_EXPRESSION:
            return expr_refs_hole_slow_rec(expr->as.app.func, hole, seen) ||
                   expr_refs_hole_slow_rec(expr->as.app.arg, hole, seen);
        case FORALL_EXPRESSION:
            return expr_refs_hole_slow_rec(expr->as.forall.bound_variable, hole, seen) ||
                   expr_refs_hole_slow_rec(expr->as.forall.body, hole, seen);
        case MATCH_EXPRESSION:
            if (expr_refs_hole_slow_rec(expr->as.match.scrutinee, hole, seen)) {
                return true;
            }
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                MatchBranch *branch = expr->as.match.branches[i];
                if (expr_refs_hole_slow_rec(branch->constructor, hole, seen) ||
                    expr_refs_hole_slow_rec(branch->body, hole, seen)) {
                    return true;
                }
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    if (expr_refs_hole_slow_rec(branch->pattern_variables[j], hole, seen)) {
                        return true;
                    }
                }
            }
            return false;
        case FIX_EXPRESSION:
            if (expr_refs_hole_slow_rec(expr->as.fix.recursive_var, hole, seen) ||
                expr_refs_hole_slow_rec(expr->as.fix.body, hole, seen)) {
                return true;
            }
            for (int i = 0; i < expr->as.fix.arg_count; i++) {
                if (expr_refs_hole_slow_rec(expr->as.fix.args[i], hole, seen)) {
                    return true;
                }
            }
            return false;
    }
    return false;
}

static bool expr_refs_hole(Expression *expr, Expression *hole) {
#if MENGINE_EVAR_FREE_FILL
    if (!expr || !expr->has_evar) {
        return false;
    }
#else
    if (!expr) {
        return false;
    }
#endif
    Map *seen = map_new_with_capacity(64);
    bool result = expr_refs_hole_slow_rec(expr, hole, seen);
    map_free(seen);
    return result;
}

// Recompute has_evar for a single expression from its owned children.
// This mirrors construction-time propagate_evar_refs calls, plus the expression's type edge.
static bool recompute_has_evar(Expression *expr) {
    bool old_has_evar = expr->has_evar;
    expr->has_evar = false;

    propagate_evar_refs(expr, get_expression_type(expr));

    switch (expr->tag) {
        case HOLE_EXPRESSION:
            if (!expr->as.hole.is_satisfied) {
                expr->has_evar = true;
            }
            break;
        case VAR_EXPRESSION:
            propagate_evar_refs(expr, expr->as.var.body);
            break;
        case LAMBDA_EXPRESSION:
            propagate_evar_refs(expr, expr->as.lambda.bound_variable);
            propagate_evar_refs(expr, expr->as.lambda.body);
            break;
        case APP_EXPRESSION:
            propagate_evar_refs(expr, expr->as.app.func);
            propagate_evar_refs(expr, expr->as.app.arg);
            break;
        case FORALL_EXPRESSION:
            propagate_evar_refs(expr, expr->as.forall.bound_variable);
            propagate_evar_refs(expr, expr->as.forall.body);
            break;
        case MATCH_EXPRESSION: {
            propagate_evar_refs(expr, expr->as.match.scrutinee);
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                MatchBranch *branch = expr->as.match.branches[i];
                propagate_evar_refs(expr, branch->constructor);
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    propagate_evar_refs(expr, branch->pattern_variables[j]);
                }
                propagate_evar_refs(expr, branch->body);
            }
            break;
        }
        case FIX_EXPRESSION:
            propagate_evar_refs(expr, expr->as.fix.recursive_var);
            for (int i = 0; i < expr->as.fix.arg_count; i++) {
                propagate_evar_refs(expr, expr->as.fix.args[i]);
            }
            propagate_evar_refs(expr, expr->as.fix.body);
            break;
        default:
            break;
    }

    return old_has_evar != expr->has_evar;
}

bool fill_hole(Expression *hole, Expression *term) {
    if (hole->tag != HOLE_EXPRESSION) {
        return false;
    }

    // Check preconditions:
    //   1) type(term) == expected return type of hole
    //   2) term is valid in hole's context
    //   3) term does not contain this hole. Evar-free terms return immediately;
    //      otherwise we walk the term DAG looking for this specific hole.
    LinearMap *hole_assignments = linear_map_new();
    Context *hole_context = get_expression_context(hole);
    if (!valid_in_context(term, hole_context)) {
        linear_map_clear_free(hole_assignments);
        return false;
    }
    if (!open_types_compatible_collecting_in_context(hole_context, get_expression_type(hole),
                                                     get_expression_type(term), hole_assignments)) {
        linear_map_clear_free(hole_assignments);
        return false;
    }
    if (expr_refs_hole(term, hole)) {
        linear_map_clear_free(hole_assignments);
        return false;
    }

    // Cascade: fill sub-holes discovered during the open_types_compatible_collecting check
    for (int i = 0; i < hole_assignments->size; i++) {
        LinearMapItem *item = &hole_assignments->items[i];
        if (item->key) {
            fill_hole((Expression *)item->key, (Expression *)item->val);
        }
    }
    linear_map_clear_free(hole_assignments);

    conversion_cache_clear();

    // Rewrite structural uplinks: replace hole with term in all direct parents.
    Uplink *holepars = hole->uplinks;
    if (holepars) {
        Uplink *ul = holepars;
        while (ul) {
            switch (ul->relation) {
                case (LAMBDA_BODY): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_LAMBDA_BODY(ptr, term);
                    break;
                }
                case (LAMBDA_BOUND_VAR): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_LAMBDA_BOUND_VAR(ptr, term);
                    break;
                }
                case (APP_FUNC): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_APP_FUNC(ptr, term);
                    break;
                }
                case (APP_ARG): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_APP_ARG(ptr, term);
                    break;
                }
                case (FORALL_BODY): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_FORALL_BODY(ptr, term);
                    break;
                }
                case (FORALL_BOUND_VAR): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_FORALL_BOUND_VAR(ptr, term);
                    break;
                }
                case (VAR_BODY): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_VAR_BODY(ptr, term);
                    break;
                }
                case (EXPR_TYPE): {
                    Expression *ptr = (Expression *)ul->ptr;
                    SET_EXPR_TYPE(ptr, term);
                    break;
                }
                default:
                    fprintf(stderr, WARNING "todo: fill_hole for relation %d.\n" CRESET,
                            ul->relation);
                    break;
            }
            ul = ul->next;
        }
    }

    hole->as.hole.is_satisfied = true;
    recompute_has_evar(hole);

    // BFS upward through structural uplinks to recompute has_evar on ancestors.
    if (holepars) {
        DoublyLinkedList *queue = dll_create();

        Uplink *ul = holepars;
        while (ul) {
            Expression *par = (Expression *)ul->ptr;
            dll_insert_at_tail(queue, dll_new_node(par));
            ul = ul->next;
        }

        while (queue->head) {
            DLLNode *n = dll_remove_head(queue);
            Expression *p = (Expression *)n->data;
            free(n);

            bool changed = recompute_has_evar(p);
            if (changed && p->uplinks) {
                Uplink *pul = p->uplinks;
                while (pul) {
                    Expression *pp = (Expression *)pul->ptr;
                    dll_insert_at_tail(queue, dll_new_node(pp));
                    pul = pul->next;
                }
            }
        }

        dll_destroy(queue);
    }

    return true;
}

char c_counter = 'a';
char *get_char() {
    char temp[2] = {c_counter, '\0'};
    c_counter += 1;
    if (c_counter > 'z') {
        c_counter = 'a';
    }
    return strdup(temp);
}

bool _congruence2(Expression *a, Expression *b, LinearMap *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return true;
    }

    if (a->tag == b->tag) {
        switch (a->tag) {
            case (TYPE_EXPRESSION):
                return true;
            case (PROP_EXPRESSION):
                return true;
            case (APP_EXPRESSION):
                return (_congruence2(a->as.app.func, b->as.app.func, mapping) &&
                        _congruence2(a->as.app.arg, b->as.app.arg, mapping)) != 0;
            case (FORALL_EXPRESSION): {
                linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
                return _congruence2(a->as.forall.body, b->as.forall.body, mapping);
            }
            case (LAMBDA_EXPRESSION): {
                linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
                return _congruence2(a->as.lambda.body, b->as.lambda.body, mapping);
            }
            case (VAR_EXPRESSION): {
                return ((a == b) || (linear_map_get(mapping, a) == b)) != 0;
            }
            case (HOLE_EXPRESSION): {
                return ((a == b) || (linear_map_get(mapping, a) == b)) != 0;
            }
            case (MATCH_EXPRESSION): {
                if (!_congruence2(a->as.match.scrutinee, b->as.match.scrutinee, mapping)) {
                    return false;
                }
                if (a->as.match.branch_count != b->as.match.branch_count) {
                    return false;
                }
                for (int i = 0; i < a->as.match.branch_count; i++) {
                    MatchBranch *branch_a = a->as.match.branches[i];
                    MatchBranch *branch_b = b->as.match.branches[i];
                    if (!_congruence2(branch_a->constructor, branch_b->constructor, mapping)) {
                        return false;
                    }
                    if (branch_a->pattern_var_count != branch_b->pattern_var_count) {
                        return false;
                    }
                    for (int j = 0; j < branch_a->pattern_var_count; j++) {
                        linear_map_set(mapping, branch_a->pattern_variables[j],
                                       branch_b->pattern_variables[j]);
                    }
                    if (!_congruence2(branch_a->body, branch_b->body, mapping)) {
                        return false;
                    }
                }
                return true;
            }
            case (FIX_EXPRESSION): {
                linear_map_set(mapping, a->as.fix.recursive_var, b->as.fix.recursive_var);
                if (a->as.fix.arg_count != b->as.fix.arg_count) {
                    return false;
                }
                if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                    return false;
                }
                for (int i = 0; i < a->as.fix.arg_count; i++) {
                    linear_map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
                }
                return _congruence2(a->as.fix.body, b->as.fix.body, mapping);
            }
            default:
                fprintf(stderr, ERROR "Unknown expression type in _congruence2.\n" CRESET);
                return false;
        }
    } else {
        if (a->tag == HOLE_EXPRESSION || b->tag == HOLE_EXPRESSION) {
            linear_map_set(mapping, a, b);
            return true;
        }
    }

    return false;
}

bool congruence2(Expression *a, Expression *b) {
    LinearMap *mapping = linear_map_new();
    bool result = _congruence2(a, b, mapping);
    free(mapping->items);
    free(mapping);
    return result;
}
