#include "src/runtime/definition_table.h"

void definition_table_init(DefinitionTable *tbl) {
    tbl->size = 0;
    tbl->capacity = 1;
    tbl->entries =
        (DefinitionEntry *)malloc(tbl->capacity * sizeof(DefinitionEntry));
}

void definition_table_insert(DefinitionTable *tbl, const char *name,
                             Expression *type, Expression *body) {
    if (tbl->size >= tbl->capacity - 1) {
        tbl->capacity *= 2;
        tbl->entries = (DefinitionEntry *)realloc(
            tbl->entries, tbl->capacity * sizeof(DefinitionEntry));
    }

    DefinitionEntry *entry = &tbl->entries[tbl->size++];
    entry->name = strdup(name);
    entry->type = type;
    entry->body = body;
}

DefinitionEntry *definition_table_lookup(DefinitionTable *tbl,
                                         const char *name) {
    for (size_t i = 0; i < tbl->size; i++) {
        if (strcmp(tbl->entries[i].name, name) == 0) {
            return &tbl->entries[i];
        }
    }
    return NULL;
}

void definition_table_free(DefinitionTable *tbl) {
    if (!tbl) {
        return;
    }

    for (size_t i = 0; i < tbl->size; i++) {
        free(tbl->entries[i].name);
    }
    free(tbl->entries);
}