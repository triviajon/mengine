#include "src/engine/new_tactics.h"
#include "src/kernel/doubly_linked_list.h"
#include "src/kernel/expression.h"
#include "src/kernel/subst.h"

TacticResult *intro_tactic(Expression *goal, char *name) {
    TacticResult *result = init_tactic_result(false, NULL, NULL);

    if (goal->type != HOLE_EXPRESSION) {
        result->error_message = "Goal is not a hole";
        return result;
    }

    Expression *goal_ty = get_expression_type(goal);
    if (goal_ty->type != FORALL_EXPRESSION) {
        result->error_message = "Goal is not a forall expression";
        return result;
    }

    Expression *x = goal_ty->value.forall.bound_variable;
    Expression *A = get_expression_type(x);
    Expression *B = goal_ty->value.forall.body;

    // We'll need to create a new bound variable x' with the given name and type of the original bound variable
    // Then, assuming the goal expected type is "forall (x : A), B", we'll need to create a new body B[x -> x']
    // Then, we'll need to create a new goal type with the new body and the new bound variable in the context. 
    Expression *x_prime = init_var_expression_wc(name, A, get_expression_context(goal));
    Expression *new_body = subst(B, x, x_prime);
    Context *new_context = context_insert(get_expression_context(goal), x_prime);
    Expression *new_goal_type = init_lambda_expression_wc(x_prime, new_body, new_context);
    Expression *new_goal = init_hole_expression("Goal", new_goal_type, new_context);

    // Finally, we'll need to create a new proof of the original goal.
    Expression *proof_of_original = init_lambda_expression_wc(x, new_goal, get_expression_context(goal));
    if (can_fill(goal, proof_of_original)) {
        fill_hole(goal, proof_of_original);

        result->success = true;
        result->new_goals = dll_create();

        DLLNode *new_goal_node = dll_new_node(new_goal);
        dll_insert_at_tail(result->new_goals, new_goal_node);
        
        return result;
    }

    result->error_message = "Failed to fill the hole";
    return result;
}
