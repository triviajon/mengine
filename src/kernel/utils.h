#ifndef UTILS_H
#define UTILS_H

#include "expression.h"
#include "dyn_array_map.h"
#include "doubly_linked_list.h"

typedef struct Context Context;

char *stringify_expression(Expression *expression);
char *stringify_context(Context *context);
char *stringify_expression_with_let(Expression *expression);

char *stringify_expression2(Expression *expression);
char *stringify_context2(Context *context);
char *stringify_type(Expression *expression);

char *se(Expression *expression);
char *sc(Context *context);

typedef enum {
	UNMARKED,
	TEMPORARY,
	PERMANENT
} MarkType;

typedef struct {
	void *expr;
	int count;
} ExpressionCount;

// Two expressions are equal wrt this function if their pointers are equal
bool expression_equal(Expression *a, Expression *b);

#endif  // UTILS_H