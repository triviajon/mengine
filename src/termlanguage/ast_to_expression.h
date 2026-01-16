#ifndef AST_TO_EXPRESSION_H
#define AST_TO_EXPRESSION_H

#include "src/common/options.h"
#include "src/kernel/kernel_api.h"
#include "src/termlanguage/parser.h"

/**
 * Convert an AST node to a kernel Expression.
 *
 * The conversion process:
 * - For variable references (AST_VAR): Look up the variable name in the
 *   provided context and return the corresponding Expression*.
 * - For binders in lambdas/foralls: Create new VAR_EXPRESSION nodes with the
 *   specified names and types.
 * - For variable references in bodies: Look them up from a working context that
 *   includes any newly bound variables.
 *
 * @param ast Pointer to the AST node to convert.
 * @param context The context providing access to existing variables and types.
 * @return Pointer to the converted Expression, or NULL if conversion fails.
 */
Expression *ast_to_expression(AST *ast, Context *context);

/**
 * Parse a string and convert it directly to a kernel Expression.
 *
 * @param input The input string to parse.
 * @param context The context providing access to existing variables and types.
 * @return Pointer to the converted Expression, or NULL if parsing/conversion
 * fails.
 */
Expression *parse_string_to_expression(const char *input, Context *context);

/**
 * Free an AST node and all its children recursively.
 *
 * @param ast Pointer to the AST node to free.
 */
void free_ast(AST *ast);

#endif  // AST_TO_EXPRESSION_H
