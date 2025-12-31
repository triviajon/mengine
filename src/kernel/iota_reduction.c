#include "src/kernel/iota_reduction.h"

#include <stdlib.h>

#include "src/kernel/doubly_linked_list.h"
#include "src/kernel/inductive.h"
#include "src/kernel/new_subst.h"

bool is_iota_reducible(Expression *expression) {
    if (expression->tag != MATCH_EXPRESSION) {
        return false;
    }

    Expression *scrutinee = get_match_scrutinee(expression);
    Expression *inductive_type = get_expression_type(scrutinee);
    if (!is_inductive(inductive_type)) {
        return false;
    }

    Expression *expected_constructor = get_head(scrutinee);
    if (!is_constructor_of(expected_constructor, inductive_type)) {
        return false;
    }

    return true;
}

// Helper: Extract arguments from an application chain (c u1 u2 ... un) -> [u1, u2, ..., un]
static Expression **get_application_args(Expression *expr, int *arg_count) {
    DoublyLinkedList *args = dll_create();
    Expression *curr = expr;

    // Traverse the application spine, collecting arguments
    while (curr->tag == APP_EXPRESSION) {
        dll_insert_at_head(args, dll_new_node(get_app_arg(curr)));
        curr = get_app_func(curr);
    }

    *arg_count = dll_len(args);
    Expression **result = malloc(*arg_count * sizeof(Expression *));
    for (int i = 0; i < *arg_count; i++) {
        result[i] = dll_at(args, i)->data;
    }
    dll_destroy(args);
    return result;
}

Expression *iota_reduce(Context *gamma, Expression *expression) {
    if (!is_iota_reducible(expression)) {
        return NULL;
    }

    Expression *scrutinee = get_match_scrutinee(expression);
    Expression *scrutinee_constructor = get_head(scrutinee);

    int scrutinee_arg_count;
    Expression **scrutinee_args = get_application_args(scrutinee, &scrutinee_arg_count);

    for (int i = 0; i < expression->as.match.branch_count; i++) {
        MatchBranch *branch = expression->as.match.branches[i];
        Expression *branch_constructor = branch->constructor;

        // Check if this branch's constructor matches the scrutinee's constructor
        if (congruence(branch_constructor, scrutinee_constructor)) {
            Expression **pattern_variables = branch->pattern_variables;
            int pattern_var_count = branch->pattern_var_count;

            if (pattern_var_count != scrutinee_arg_count) {
                free(scrutinee_args);
                return NULL;
            }

            DoublyLinkedList *old_exprs = dll_create();
            DoublyLinkedList *new_exprs = dll_create();

            for (int j = 0; j < pattern_var_count; j++) {
                dll_insert_at_tail(old_exprs, dll_new_node(pattern_variables[j]));
                dll_insert_at_tail(new_exprs, dll_new_node(scrutinee_args[j]));
            }

            Expression *reduced_body = new_p_subst(gamma, branch->body, old_exprs, new_exprs);

            dll_destroy(old_exprs);
            dll_destroy(new_exprs);
            free(scrutinee_args);

            return reduced_body;
        }
    }

    free(scrutinee_args);
    return NULL;
}
