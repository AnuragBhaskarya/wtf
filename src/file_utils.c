#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_utils.h"
#include "hash_table.h"

// Load definitions from file into hash table
int load_definitions(const char *filename, HashTable *table) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *term = strtok(line, ":");
        char *definition = strtok(NULL, "\n");
        if (term && definition) {
            // Check if this is a new term or additional definition
            hash_table_insert(table, term, definition);
        }
    }

    fclose(file);
    return 1;
}

// Load removed definitions into a separate hash table
int load_removed_definitions(const char *filename, HashTable *removed_table) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *term = strtok(line, ":");
        char *definition = strtok(NULL, "\n");
        if (term && definition) {
            hash_table_insert(removed_table, term, definition);
        }
    }

    fclose(file);
    return 1;
}

// Check if a specific term:definition pair is in the removed list
int is_definition_removed(const char *term, const char *definition, HashTable *removed_table) {
    DefinitionList *removed_defs = hash_table_lookup_all(removed_table, term);
    if (!removed_defs) return 0;

    for (int i = 0; i < removed_defs->count; i++) {
        if (strcmp(removed_defs->definitions[i], definition) == 0) {
            free_definition_list(removed_defs);
            return 1;
        }
    }

    free_definition_list(removed_defs);
    return 0;
}

// Add a definition to the removed.txt file
int add_to_removed(const char *filename, const char *term, const char *definition) {
    FILE *file = fopen(filename, "a");
    if (!file) return 0;

    fprintf(file, "%s:%s\n", term, definition);
    fclose(file);
    return 1;
}

// Add a definition to the added.txt file
int add_to_added(const char *filename, const char *term, const char *definition) {
    FILE *file = fopen(filename, "a");
    if (!file) return 0;

    fprintf(file, "%s:%s\n", term, definition);
    fclose(file);
    return 1;
}

int remove_from_removed(const char *filename, const char *term, const char *definition) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;
    
    // Create a temporary file
    FILE *temp = fopen("/tmp/wtf_temp", "w");
    if (!temp) {
        fclose(file);
        return 0;
    }
    
    char line[256];
    int removed = 0;
    
    // Copy all lines except the one to be removed
    while (fgets(line, sizeof(line), file)) {
        char *curr_term = strdup(line);
        char *curr_def = strchr(curr_term, ':');
        if (curr_def) {
            *curr_def = '\0';
            curr_def++;
            char *newline = strchr(curr_def, '\n');
            if (newline) *newline = '\0';
            
            if (strcmp(curr_term, term) != 0 || strcmp(curr_def, definition) != 0) {
                fprintf(temp, "%s", line);
            } else {
                removed = 1;
            }
        }
        free(curr_term);
    }
    
    fclose(file);
    fclose(temp);
    
    // Replace original file with temporary file
    if (removed) {
        remove(filename);
        rename("/tmp/wtf_temp", filename);
        return 1;
    }
    
    remove("/tmp/wtf_temp");
    return 0;
}