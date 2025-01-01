// file_utils.h
#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "hash_table.h"

int load_definitions(const char *filename, HashTable *table);
int add_definition(const char *filename, const char *entry);
int load_removed_definitions(const char *filename, HashTable *removed_table);
int is_definition_removed(const char *term, const char *definition, HashTable *removed_table);
int add_to_removed(const char *filename, const char *term, const char *definition);
int add_to_added(const char *filename, const char *term, const char *definition);
int remove_from_removed(const char *filename, const char *term, const char *definition);

#endif