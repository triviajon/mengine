#include "src/kernel/fix_reduction.h"

#include <stdlib.h>

#include "src/kernel/inductive.h"

bool is_fix_reducible(Expression *expression) { return expression->tag == FIX_EXPRESSION; }

Expression *fix_reduce(Expression *expression) {
    if (!is_fix_reducible(expression)) {
        return NULL;
    }

    Expression **args = get_fix_args(expression);
    int arg_count = get_fix_arg_count(expression);
    Expression *body = get_fix_body(expression);

    // Recursive occurrences in the body refer to the fix's recursive variable, whose
    // definitional body is this very fix node (registered in init_fix_expression_wc).
    // So unfolding is just stripping the fix down to its lambda abstraction over the
    // arguments; the recursive calls keep resolving through the recursive variable
    // (via delta). Substituting the fix node inline instead would be wrong here: the
    // fix shares its argument binders with the body, so re-binding them with the
    // wrapping lambdas would capture the copies living inside the substituted fix.
    Expression *result = body;

    for (int i = arg_count - 1; i >= 0; i--) {
        result = init_lambda_expression_wc(args[i], result);
        if (!result) {
            return NULL;
        }
    }

    return result;
}

Expression *fix_reduce_app(Expression *app, Expression *(*whnf)(Expression *)) {
    // The fix at the head of the application spine fires only when its decreasing
    // argument is in constructor head normal form. This is the standard guard that
    // keeps reduction terminating on symbolic (variable) recursive arguments:
    // `add n O` with `n` a variable stays stuck instead of unfolding forever.
    Expression *head = get_head(app);
    if (head->tag != FIX_EXPRESSION) {
        return NULL;
    }

    int decreasing_index = get_fix_decreasing_arg_index(head);
    if (decreasing_index < 0) {
        return NULL;
    }

    // Count the spine arguments (app = (((fix) a0) a1) ... a_{count-1}).
    int count = 0;
    for (Expression *cursor = app; cursor->tag == APP_EXPRESSION; cursor = get_app_func(cursor)) {
        count++;
    }
    if (decreasing_index >= count) {
        // The decreasing argument has not been supplied yet; cannot reduce.
        return NULL;
    }

    Expression **args = malloc(count * sizeof(Expression *));
    if (!args) {
        return NULL;
    }
    Expression *cursor = app;
    for (int i = count - 1; i >= 0; i--) {
        args[i] = get_app_arg(cursor);
        cursor = get_app_func(cursor);
    }

    Expression *decreasing_arg = whnf ? whnf(args[decreasing_index]) : args[decreasing_index];
    if (!is_constructor(get_head(decreasing_arg))) {
        free(args);
        return NULL;
    }

    Expression *unfolded = fix_reduce(head);
    if (!unfolded) {
        free(args);
        return NULL;
    }

    Context *context = get_expression_context(app);
    Expression *result = unfolded;
    for (int i = 0; i < count; i++) {
        result = init_app_expression_wc(result, args[i], context);
        if (!result) {
            free(args);
            return NULL;
        }
    }

    free(args);
    return result;
}
