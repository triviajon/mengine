#ifndef DEFINITION_TABLE_H
#define DEFINITION_TABLE_H

#include "src/kernel/expression.h"

typedef struct {
    char *name;
    Expression *type;
    Expression *body;
} DefinitionEntry;

typedef struct {
    size_t size;
    size_t capacity;
    DefinitionEntry *entries;
} DefinitionTable;

/**
 * Initialize a DefinitionTable.
 *
 * @param tbl Pointer to the DefinitionTable to initialize.
 */
void definition_table_init(DefinitionTable *tbl);

/**
 * Insert a new definition into the table.
 *
 * @param tbl Pointer to the DefinitionTable.
 * @param name Name of the definition.
 * @param type Type of the definition.
 * @param body Body of the definition.
 */
void definition_table_insert(DefinitionTable *tbl, const char *name,
                             Expression *type, Expression *body);

/**
 * Look up a definition by name.
 *
 * @param tbl Pointer to the DefinitionTable.
 * @param name Name of the definition to look up.
 * @return Pointer to the DefinitionEntry if found, NULL otherwise.
 */
DefinitionEntry *definition_table_lookup(DefinitionTable *tbl,
                                         const char *name);

/**
 * Free all resources associated with the DefinitionTable.
 *
 * @param tbl Pointer to the DefinitionTable to free.
 */
void definition_table_free(DefinitionTable *tbl);

#endif  // DEFINITION_TABLE_H