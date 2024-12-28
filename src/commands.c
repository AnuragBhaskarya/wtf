#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "hash_table.h"
#include "file_utils.h"

#define MAX_INPUT_LENGTH 256

// Handle "wtf is <term>" command
void handle_is_command(HashTable *dictionary, HashTable *removed_dict, char **args, int argc) {
    if (argc < 3) {
        printf("Error: No term provided. Use `wtf is <term>`.\n");
        return;
    }

    char term[256] = "";
    for (int i = 2; i < argc; i++) {
        strcat(term, args[i]);
        if (i < argc - 1) strcat(term, " ");
    }

    DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
    if (definitions) {
        int match_found = 0;
        
        for (int i = 0; i < definitions->count; i++) {
            // Skip if definition is in removed list
            if (!is_definition_removed(definitions->keys[i], definitions->definitions[i], removed_dict)) {
                printf("%s: %s\n", definitions->keys[i], definitions->definitions[i]);
                match_found = 1;
            }
        }
        
        free_definition_list(definitions);
        
        if (!match_found) {
            printf("Lol I don't know what '%s' means.\n", term);
        }
    } else {
        printf("Lol I don't know what '%s' means.\n", term);
    }
}

// Handle "wtf add <term>:<definition>" command
void handle_add_command(HashTable *dictionary, const char *added_path, const char *term, const char *definition) {
    // First check if this exact definition already exists
    DefinitionList *existing = hash_table_lookup_all(dictionary, term);
    if (existing) {
        for (int i = 0; i < existing->count; i++) {
            if (strcmp(existing->definitions[i], definition) == 0) {
                printf("This definition already exists.\n");
                free_definition_list(existing);
                return;
            }
        }
        free_definition_list(existing);
    }

    if (add_to_added(added_path, term, definition)) {
        hash_table_insert(dictionary, term, definition);
        printf("Definition added successfully.\n");
    } else {
        printf("Error: Could not add definition.\n");
    }
}

// Handle "wtf remove <term>" command
void handle_remove_command(HashTable *dictionary, HashTable *removed_dict, const char *removed_path, char **args, int argc) {
    if (argc < 3) {
        printf("Error: No term provided. Use `wtf remove <term>`.\n");
        return;
    }

    char term[MAX_INPUT_LENGTH] = "";
    for (int i = 2; i < argc; i++) {
        strcat(term, args[i]);
        if (i < argc - 1) strcat(term, " ");
    }

    DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
    if (!definitions) {
        printf("Term '%s' not found in the dictionary.\n", term);
        return;
    }

    // Filter out already removed definitions
    DefinitionList *filtered = create_definition_list();
    for (int i = 0; i < definitions->count; i++) {
        if (!is_definition_removed(definitions->keys[i], definitions->definitions[i], removed_dict)) {
            add_to_definition_list(filtered, definitions->keys[i], definitions->definitions[i]);
        }
    }

    if (filtered->count == 0) {
        printf("No definitions available to remove.\n");
        free_definition_list(definitions);
        free_definition_list(filtered);
        return;
    }

    if (filtered->count == 1) {
        printf("Found definition:\n%s: %s\n", filtered->keys[0], filtered->definitions[0]);
        printf("Are you sure you want to remove this definition? [Y/n]: ");
        
        char response = getchar();
        while (getchar() != '\n');
        
        if (response == 'Y' || response == 'y') {
            if (add_to_removed(removed_path, filtered->keys[0], filtered->definitions[0])) {
                hash_table_insert(removed_dict, filtered->keys[0], filtered->definitions[0]);
                printf("Definition removed successfully.\n");
            }
        } else {
            printf("Operation aborted.\n");
        }
    } else {
        printf("Found definitions:\n");
        for (int i = 0; i < filtered->count; i++) {
            printf("%d. %s: %s\n", i + 1, filtered->keys[i], filtered->definitions[i]);
        }
        
        printf("Enter the numbers of definitions to remove (separated by space or comma): ");
        char input[MAX_INPUT_LENGTH];
        if (fgets(input, sizeof(input), stdin)) {
            printf("Are you sure you want to remove these definitions? [Y/n]: ");
            char response = getchar();
            while (getchar() != '\n');
            
            if (response == 'Y' || response == 'y') {
                char *token = strtok(input, " ,\n");
                int removed = 0;
                
                while (token) {
                    int num = atoi(token);
                    if (num > 0 && num <= filtered->count) {
                        if (add_to_removed(removed_path, filtered->keys[num-1], 
                                         filtered->definitions[num-1])) {
                            hash_table_insert(removed_dict, filtered->keys[num-1], 
                                           filtered->definitions[num-1]);
                            removed++;
                        }
                    }
                    token = strtok(NULL, " ,\n");
                }
                
                if (removed > 0) {
                    printf("%d definition(s) removed successfully.\n", removed);
                } else {
                    printf("No definitions were removed.\n");
                }
            } else {
                printf("Operation aborted.\n");
            }
        }
    }
    
    free_definition_list(definitions);
    free_definition_list(filtered);
}