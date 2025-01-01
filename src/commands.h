#ifndef COMMANDS_H
#define COMMANDS_H

#include "hash_table.h"

void handle_is_command(HashTable *dictionary, HashTable *removed_dict, char **args, int argc);
void handle_add_command(HashTable *dictionary, const char *added_path, const char *term, const char *definition);
void handle_remove_command(HashTable *dictionary, HashTable *removed_dict, const char *removed_path, char **args, int argc);
void handle_recover_command(HashTable *removed_dict, const char *removed_path, char **args, int argc);
#endif