#include "src/kernel/rocq_export.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/kernel/context.h"
#include "src/kernel/inductive.h"

// Exports reach megabytes, so the output is appended in place rather than re-concatenated.
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_append(StrBuf *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t cap = sb->cap ? sb->cap : 256;
        while (sb->len + n + 1 > cap) {
            cap *= 2;
        }
        sb->data = realloc(sb->data, cap);
        sb->cap = cap;
    }
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

typedef struct {
    Map *context;  // entries of the exported context
    Map *bound;    // variables in scope: context entries and binders printed so far
    Map *renamed;  // constructor parameter binder -> the Inductive's parameter it stands for
    bool failed;
} ExportState;

// Identifiers are derived from node addresses, which are unique, so no binder can capture
// another variable. Variables use the prefix 'v' and let-bound shared subterms use 's'.
static void append_address_name(StrBuf *out, char prefix, Expression *n) {
    char buf[2 * sizeof(void *) + 8];
    snprintf(buf, sizeof(buf), "%c%p", prefix, (void *)n);
    sb_append(out, buf);
}

static void bind_variable(ExportState *st, StrBuf *out, Expression *var) {
    map_set(st->bound, var, var);
    append_address_name(out, 'v', var);
}

// Subterms shared within a term whose free variables are all context entries are printed once,
// as a let at the top of the term. Other shared subterms are printed inline.
typedef struct {
    Map *uses;         // node -> number of parents within the term
    Map *free_locals;  // node -> DoublyLinkedList of its free variables bound inside the term
    DoublyLinkedList *postorder;  // children before parents
} TermLayout;

typedef struct {
    ExportState *st;
    TermLayout *tl;
    Map *visited;
} LayoutVisit;

static void layout_visit(LayoutVisit *v, Expression *n);

static bool is_among(Expression *var, Expression **vars, int count) {
    for (int i = 0; i < count; i++) {
        if (vars[i] == var) {
            return true;
        }
    }
    return false;
}

// Records one parent edge and adds the child's free locals, except those bound by the parent,
// to *acc.
static void layout_child(LayoutVisit *v, Expression *child, DoublyLinkedList **acc,
                         Expression **bound, int bound_count) {
    uintptr_t uses = (uintptr_t)map_get(v->tl->uses, child);
    map_set(v->tl->uses, child, (void *)(uses + 1));
    layout_visit(v, child);
    DoublyLinkedList *child_locals = map_get(v->tl->free_locals, child);
    for (DLLNode *n = child_locals ? child_locals->head : NULL; n; n = n->next) {
        Expression *var = (Expression *)n->data;
        if (is_among(var, bound, bound_count) || (*acc && dll_search(*acc, var))) {
            continue;
        }
        if (!*acc) {
            *acc = dll_create();
        }
        dll_insert_at_tail(*acc, dll_new_node(var));
    }
}

static void layout_visit(LayoutVisit *v, Expression *n) {
    if (map_get(v->visited, n)) {
        return;
    }
    map_set(v->visited, n, n);
    DoublyLinkedList *locals = NULL;
    switch (n->tag) {
        case VAR_EXPRESSION:
            if (!map_get(v->st->context, n)) {
                locals = dll_create();
                dll_insert_at_tail(locals, dll_new_node(n));
            }
            break;
        case APP_EXPRESSION:
            layout_child(v, get_app_func(n), &locals, NULL, 0);
            layout_child(v, get_app_arg(n), &locals, NULL, 0);
            break;
        case LAMBDA_EXPRESSION:
        case FORALL_EXPRESSION: {
            bool lambda = n->tag == LAMBDA_EXPRESSION;
            Expression *var = lambda ? get_lambda_bound_variable(n) : get_forall_bound_variable(n);
            layout_child(v, get_expression_type(var), &locals, NULL, 0);
            layout_child(v, lambda ? get_lambda_body(n) : get_forall_body(n), &locals, &var, 1);
            break;
        }
        case MATCH_EXPRESSION:
            layout_child(v, n->as.match.scrutinee, &locals, NULL, 0);
            for (int i = 0; i < n->as.match.branch_count; i++) {
                MatchBranch *branch = n->as.match.branches[i];
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    Expression *alias_body = get_var_body(branch->pattern_variables[j]);
                    if (alias_body) {
                        layout_child(v, alias_body, &locals, NULL, 0);
                    }
                }
                layout_child(v, branch->body, &locals, branch->pattern_variables,
                             branch->pattern_var_count);
            }
            break;
        case FIX_EXPRESSION: {
            // The recursive variable and the arguments are bound in the annotations and body.
            int bound_count = n->as.fix.arg_count + 1;
            Expression **bound = malloc(bound_count * sizeof(Expression *));
            bound[0] = n->as.fix.recursive_var;
            memcpy(bound + 1, n->as.fix.args, n->as.fix.arg_count * sizeof(Expression *));
            for (int i = 0; i < n->as.fix.arg_count; i++) {
                layout_child(v, get_expression_type(n->as.fix.args[i]), &locals, bound,
                             bound_count);
            }
            layout_child(v, get_expression_type(n->as.fix.body), &locals, bound, bound_count);
            layout_child(v, n->as.fix.body, &locals, bound, bound_count);
            free(bound);
            break;
        }
        case HOLE_EXPRESSION:
            fprintf(stderr, ERROR "Cannot export a term containing the hole ?%s.\n" CRESET,
                    get_hole_name(n));
            v->st->failed = true;
            break;
        default:
            break;
    }
    if (locals) {
        map_set(v->tl->free_locals, n, locals);
    }
    dll_insert_at_tail(v->tl->postorder, dll_new_node(n));
}

static bool is_hoisted(TermLayout *tl, Expression *n) {
    return tl && n->tag != VAR_EXPRESSION && (uintptr_t)map_get(tl->uses, n) >= 2 &&
           !map_get(tl->free_locals, n);
}

static void destroy_list(void *key, void *value, void *ud) {
    (void)key;
    (void)ud;
    dll_destroy((DoublyLinkedList *)value);
}

// With a NULL layout, the node is printed as a tree without let-bindings.
static void print_node(ExportState *st, TermLayout *tl, StrBuf *out, Expression *n, bool defining);

static void print_binder(ExportState *st, TermLayout *tl, StrBuf *out, Expression *var) {
    sb_append(out, "(");
    bind_variable(st, out, var);
    sb_append(out, ": ");
    print_node(st, tl, out, get_expression_type(var), false);
    sb_append(out, ")");
}

static void print_node(ExportState *st, TermLayout *tl, StrBuf *out, Expression *n, bool defining) {
    if (st->failed) {
        return;
    }
    if (!defining && is_hoisted(tl, n)) {
        append_address_name(out, 's', n);
        return;
    }
    switch (n->tag) {
        case VAR_EXPRESSION: {
            Expression *renamed = map_get(st->renamed, n);
            n = renamed ? renamed : n;
            if (!map_get(st->bound, n)) {
                fprintf(stderr,
                        ERROR "Cannot export: %s is not bound in the exported context.\n" CRESET,
                        get_var_name(n));
                st->failed = true;
                return;
            }
            append_address_name(out, 'v', n);
            break;
        }
        case APP_EXPRESSION:
            sb_append(out, "(");
            print_node(st, tl, out, get_app_func(n), false);
            sb_append(out, " ");
            print_node(st, tl, out, get_app_arg(n), false);
            sb_append(out, ")");
            break;
        case LAMBDA_EXPRESSION:
            sb_append(out, "(fun ");
            print_binder(st, tl, out, get_lambda_bound_variable(n));
            sb_append(out, " => ");
            print_node(st, tl, out, get_lambda_body(n), false);
            sb_append(out, ")");
            break;
        case FORALL_EXPRESSION:
            sb_append(out, "(forall ");
            print_binder(st, tl, out, get_forall_bound_variable(n));
            sb_append(out, ", ");
            print_node(st, tl, out, get_forall_body(n), false);
            sb_append(out, ")");
            break;
        case MATCH_EXPRESSION:
            sb_append(out, "(match ");
            print_node(st, tl, out, n->as.match.scrutinee, false);
            sb_append(out, " with");
            for (int i = 0; i < n->as.match.branch_count; i++) {
                MatchBranch *branch = n->as.match.branches[i];
                sb_append(out, " | ");
                print_node(st, tl, out, branch->constructor, false);
                // Parameter slots are aliases of the scrutinee's type arguments (they have a
                // body); Rocq wants _ there, and a let keeps the alias convertible.
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    Expression *pattern_var = branch->pattern_variables[j];
                    sb_append(out, " ");
                    if (get_var_body(pattern_var)) {
                        sb_append(out, "_");
                    } else {
                        bind_variable(st, out, pattern_var);
                    }
                }
                sb_append(out, " => ");
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    Expression *pattern_var = branch->pattern_variables[j];
                    if (get_var_body(pattern_var)) {
                        sb_append(out, "let ");
                        bind_variable(st, out, pattern_var);
                        sb_append(out, " := ");
                        print_node(st, NULL, out, get_var_body(pattern_var), false);
                        sb_append(out, " in ");
                    }
                }
                print_node(st, tl, out, branch->body, false);
            }
            sb_append(out, " end)");
            break;
        case FIX_EXPRESSION:
            sb_append(out, "(fix ");
            bind_variable(st, out, n->as.fix.recursive_var);
            for (int i = 0; i < n->as.fix.arg_count; i++) {
                sb_append(out, " ");
                print_binder(st, NULL, out, n->as.fix.args[i]);
            }
            sb_append(out, " {struct ");
            append_address_name(out, 'v', n->as.fix.args[n->as.fix.decreasing_arg_index]);
            sb_append(out, "} : ");
            print_node(st, NULL, out, get_expression_type(n->as.fix.body), false);
            sb_append(out, " := ");
            print_node(st, tl, out, n->as.fix.body, false);
            sb_append(out, ")");
            break;
        case TYPE_EXPRESSION:
            sb_append(out, "Type");
            break;
        case PROP_EXPRESSION:
            sb_append(out, "Prop");
            break;
        case HOLE_EXPRESSION:
            fprintf(stderr, ERROR "Cannot export a term containing the hole ?%s.\n" CRESET,
                    get_hole_name(n));
            st->failed = true;
            break;
    }
}

static void print_term(ExportState *st, StrBuf *out, Expression *root) {
    TermLayout tl = {map_new(), map_new(), dll_create()};
    LayoutVisit visit = {st, &tl, map_new()};
    layout_visit(&visit, root);
    for (DLLNode *n = tl.postorder->head; n && !st->failed; n = n->next) {
        Expression *node = (Expression *)n->data;
        if (is_hoisted(&tl, node)) {
            sb_append(out, "let ");
            append_address_name(out, 's', node);
            sb_append(out, " := ");
            print_node(st, &tl, out, node, true);
            sb_append(out, " in ");
        }
    }
    print_node(st, &tl, out, root, false);
    map_free(visit.visited);
    map_free(tl.uses);
    map_for_each(tl.free_locals, destroy_list, NULL);
    map_free(tl.free_locals);
    dll_destroy(tl.postorder);
}

// Proofs are exported opaquely, like a Qed; only computational bodies matter for conversion.
static bool is_proof(Expression *var) {
    return get_expression_type(get_expression_type(var))->tag == PROP_EXPRESSION;
}

static void print_entry(ExportState *st, StrBuf *out, Expression *var, bool with_body) {
    sb_append(out, with_body ? "Definition " : "Axiom ");
    append_address_name(out, 'v', var);
    sb_append(out, " : ");
    print_term(st, out, get_expression_type(var));
    if (with_body) {
        sb_append(out, " := ");
        print_term(st, out, get_var_body(var));
    }
    sb_append(out, ". (* ");
    sb_append(out, get_var_name(var));
    sb_append(out, " *)\n");
}

// Registered inductives become real Inductives so that match patterns name constructors.
static void print_inductive(ExportState *st, StrBuf *out, InductiveDefinition *def, Map *emitted) {
    sb_append(out, "Inductive ");
    append_address_name(out, 'v', def->inductive_var);
    Expression *arity = get_expression_type(def->inductive_var);
    for (int p = 0; p < def->param_count; p++, arity = get_forall_body(arity)) {
        sb_append(out, " ");
        print_binder(st, NULL, out, get_forall_bound_variable(arity));
    }
    sb_append(out, " : ");
    print_term(st, out, arity);
    sb_append(out, " :=");
    for (int i = 0; i < def->constructor_count; i++) {
        // Each constructor binds its own copy of the parameters; print them as the Inductive's.
        Expression *ctor_type = get_expression_type(def->constructors[i]);
        Expression *params = get_expression_type(def->inductive_var);
        for (int p = 0; p < def->param_count; p++) {
            map_set(st->renamed, get_forall_bound_variable(ctor_type),
                    get_forall_bound_variable(params));
            ctor_type = get_forall_body(ctor_type);
            params = get_forall_body(params);
        }
        sb_append(out, "\n  | ");
        append_address_name(out, 'v', def->constructors[i]);
        sb_append(out, " : ");
        print_term(st, out, ctor_type);
        map_set(emitted, def->constructors[i], def->constructors[i]);
    }
    sb_append(out, ". (* ");
    sb_append(out, get_var_name(def->inductive_var));
    sb_append(out, " *)\n");
}

char *rocq_export(Expression *target) {
    if (!target || target->tag != VAR_EXPRESSION || !get_var_body(target)) {
        fprintf(stderr, ERROR "Cannot export: the symbol has no body.\n" CRESET);
        return NULL;
    }

    ExportState st = {map_new(), map_new(), map_new(), false};

    // Context entries outermost first; all are in scope for every printed term.
    DoublyLinkedList *entries = dll_create();
    for (Context *c = get_expression_context(target); !context_is_empty(c);
         c = get_expression_context(c)) {
        dll_insert_at_head(entries, dll_new_node(c));
        map_set(st.context, c, c);
        map_set(st.bound, c, c);
    }

    Map *emitted = map_new();  // constructors already declared by their Inductive
    StrBuf out = {NULL, 0, 0};
    sb_append(&out, "Section MEngineExport.\n");
    for (DLLNode *n = entries->head; n && !st.failed; n = n->next) {
        Expression *var = (Expression *)n->data;
        if (map_get(emitted, var)) {
            continue;
        }
        if (is_inductive(var)) {
            print_inductive(&st, &out, get_inductive_definition(var), emitted);
        } else {
            print_entry(&st, &out, var, get_var_body(var) && !is_proof(var));
        }
    }
    print_entry(&st, &out, target, true);
    sb_append(&out, "End MEngineExport.\n");

    dll_destroy(entries);
    map_free(emitted);
    map_free(st.context);
    map_free(st.bound);
    map_free(st.renamed);

    if (st.failed) {
        free(out.data);
        return NULL;
    }
    return out.data;
}
