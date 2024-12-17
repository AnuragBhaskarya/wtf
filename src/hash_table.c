#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hash_table.h"

// Hash function
unsigned int hash_function(const char *key, int size) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash * 31) + *key++;
    }
    return hash % size;
}

// Create a hash table
HashTable* create_hash_table(int size) {
    HashTable *table = malloc(sizeof(HashTable));
    if (!table) return NULL;
    
    table->size = size;
    table->table = calloc(size, sizeof(HashNode*));
    if (!table->table) {
        free(table);
        return NULL;
    }
    
    return table;
}

// Insert a key-value pair
void hash_table_insert(HashTable *table, const char *key, const char *value) {
    if (!table || !key || !value) return;

    unsigned int index = hash_function(key, table->size);
    HashNode *new_node = malloc(sizeof(HashNode));
    if (!new_node) return;

    new_node->key = strdup(key);
    new_node->value = strdup(value);
    
    if (!new_node->key || !new_node->value) {
        free(new_node->key);
        free(new_node->value);
        free(new_node);
        return;
    }

    new_node->next = table->table[index];
    table->table[index] = new_node;
}

// Lookup a single key (kept for backward compatibility)
char* hash_table_lookup(HashTable *table, const char *key) {
    if (!table || !key) return NULL;

    unsigned int index = hash_function(key, table->size);
    HashNode *node = table->table[index];
    
    // Create a lowercase version of the input key
    char *lower_key = strdup(key);
    if (!lower_key) return NULL;

    for (int i = 0; lower_key[i]; i++) {
        lower_key[i] = tolower((unsigned char)lower_key[i]);
    }

    while (node) {
        // Create a lowercase version of the node's key
        char *lower_node_key = strdup(node->key);
        if (!lower_node_key) {
            free(lower_key);
            return NULL;
        }

        for (int i = 0; lower_node_key[i]; i++) {
            lower_node_key[i] = tolower((unsigned char)lower_node_key[i]);
        }

        if (strcmp(lower_node_key, lower_key) == 0) {
            free(lower_key);
            free(lower_node_key);
            return node->value;
        }

        free(lower_node_key);
        node = node->next;
    }

    free(lower_key);
    return NULL;
}

// Safer lowercase conversion function
char* safe_lowercase(const char *str) {
    if (!str) return NULL;
    
    char *lower = strdup(str);
    if (!lower) return NULL;
    
    for (int i = 0; lower[i]; i++) {
        lower[i] = tolower((unsigned char)lower[i]);
    }
    return lower;
}

// Updated hash_table_lookup_all function
DefinitionList* hash_table_lookup_all(HashTable *table, const char *key) {
    if (!table || !key) return NULL;

    // Dynamically allocate unique definitions and keys
    char **unique_definitions = calloc(100, sizeof(char*));
    char **unique_keys = calloc(100, sizeof(char*));
    if (!unique_definitions || !unique_keys) {
        free(unique_definitions);
        free(unique_keys);
        return NULL;
    }

    int unique_count = 0;

    // Lowercase version of the search key
    char *lower_search_key = safe_lowercase(key);
    if (!lower_search_key) {
        free(unique_definitions);
        free(unique_keys);
        return NULL;
    }

    // Two-pass approach to handle different case variations
    for (int pass = 0; pass < 2; pass++) {
        for (int index = 0; index < table->size; index++) {
            for (HashNode *current = table->table[index]; current != NULL; current = current->next) {
                // Safely convert current key to lowercase
                char *lower_current_key = safe_lowercase(current->key);
                if (!lower_current_key) continue;

                // Determine match based on pass
                int match = 0;
                if (pass == 0) {
                    // First pass: exact case match
                    match = (strcmp(current->key, key) == 0);
                } else {
                    // Second pass: case-insensitive match, avoiding exact matches from first pass
                    match = (strcmp(lower_current_key, lower_search_key) == 0 && 
                             strcmp(current->key, key) != 0);
                }

                if (match) {
                    // Check for duplicates
                    int is_duplicate = 0;
                    for (int j = 0; j < unique_count; j++) {
                        if (strcmp(unique_definitions[j], current->value) == 0 &&
                            strcmp(unique_keys[j], current->key) == 0) {
                            is_duplicate = 1;
                            break;
                        }
                    }

                    // If not a duplicate and we have space
                    if (!is_duplicate && unique_count < 100) {
                        unique_definitions[unique_count] = strdup(current->value);
                        unique_keys[unique_count] = strdup(current->key);
                        
                        // Ensure successful allocation
                        if (!unique_definitions[unique_count] || !unique_keys[unique_count]) {
                            // Clean up in case of allocation failure
                            for (int k = 0; k < unique_count; k++) {
                                free(unique_definitions[k]);
                                free(unique_keys[k]);
                            }
                            free(unique_definitions);
                            free(unique_keys);
                            free(lower_current_key);
                            free(lower_search_key);
                            return NULL;
                        }
                        
                        unique_count++;
                    }
                }

                free(lower_current_key);
            }
        }
    }

    free(lower_search_key);

    // If no definitions found
    if (unique_count == 0) {
        free(unique_definitions);
        free(unique_keys);
        return NULL;
    }

    // Prepare final list
    DefinitionList *result = malloc(sizeof(DefinitionList));
    if (!result) {
        // Clean up if allocation fails
        for (int i = 0; i < unique_count; i++) {
            free(unique_definitions[i]);
            free(unique_keys[i]);
        }
        free(unique_definitions);
        free(unique_keys);
        return NULL;
    }

    result->definitions = unique_definitions;
    result->keys = unique_keys;
    result->count = unique_count;
    result->capacity = unique_count;

    return result;
}

// Updated free_definition_list to safely free memory
void free_definition_list(DefinitionList *list) {
    if (list) {
        // Free individual definitions and keys
        for (int i = 0; i < list->count; i++) {
            free(list->definitions[i]);
            free(list->keys[i]);
        }
        
        // Free the arrays of definitions and keys
        free(list->definitions);
        free(list->keys);
        
        // Free the list structure itself
        free(list);
    }
}

// Free the hash table
void free_hash_table(HashTable *table) {
    if (!table) return;

    for (int i = 0; i < table->size; i++) {
        HashNode *node = table->table[i];
        while (node) {
            HashNode *temp = node;
            node = node->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }
    free(table->table);
    free(table);
}