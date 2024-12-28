#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "hash_table.h"
#include "file_utils.h"
#include "commands.h"
#include <limits.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_INPUT_LENGTH 256

void print_help() {
    printf("Usage:\n");
    printf("  wtf is <term>    - Get the definition of a term\n");
    printf("  wtf add <term>:<definition> - Add a new term and definition to the dictionary\n");
    printf("  wtf -h | --help  - Show this help menu\n");
    printf("  wtf remove <term> - Remove definition(s) for a term\n");
}

// For the single character response:
char getResponse(void) {
    char response;
    char line[256];
    
    if (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        if (strlen(line) == 1) {
            response = line[0];
        } else {
            response = 'n';
        }
    } else {
        response = 'n';
    }
    
    return response;
}

// For the string input:
int getString(char *buffer, size_t bufferSize) {
    if (fgets(buffer, bufferSize, stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: Unable to get home directory.\n");
        return 1;
    }
    
    // Construct paths for all dictionary files
    char definitions_path[1024];
    char added_path[1024];
    char removed_path[1024];
    
    snprintf(definitions_path, sizeof(definitions_path), "%s/.wtf/res/definitions.txt", home_dir);
    snprintf(added_path, sizeof(added_path), "%s/.wtf/res/added.txt", home_dir);
    snprintf(removed_path, sizeof(removed_path), "%s/.wtf/res/removed.txt", home_dir);
    
    if (argc < 2) {
        printf("Error: Missing arguments. Use `wtf -h` for help.\n");
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    // Load the main dictionary and user additions into memory
    HashTable *dictionary = create_hash_table(100);
    HashTable *removed_dict = create_hash_table(100);
    
    if (!load_definitions(definitions_path, dictionary)) {
        fprintf(stderr, "Error: Could not load main definitions.\n");
        return 1;
    }
    
    // Load user-added definitions
    load_definitions(added_path, dictionary);
    
    // Load removed definitions
    load_definitions(removed_path, removed_dict);

    if (strcmp(argv[1], "is") == 0) {
        handle_is_command(dictionary, removed_dict, argv, argc);
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf is <term>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }

        char term[MAX_INPUT_LENGTH] = "";
        for (int i = 2; i < argc; i++) {
            strcat(term, argv[i]);
            if (i < argc - 1) strcat(term, " ");
        }

        DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
        if (definitions) {
            int match_found = 0;
            
            for (int k = 0; k < definitions->count; k++) {
                // Check if this definition is in removed list
                if (!is_definition_removed(term, definitions->definitions[k], removed_dict)) {
                    for (int index = 0; index < dictionary->size; index++) {
                        int inner_match = 0;
                        for (HashNode *current = dictionary->table[index]; current != NULL; current = current->next) {
                            char *lower_input = safe_lowercase(term);
                            char *lower_current_key = safe_lowercase(current->key);
                            
                            if (lower_input && lower_current_key) {
                                if (strcmp(lower_input, lower_current_key) == 0 && 
                                    strcmp(definitions->definitions[k], current->value) == 0) {
                                    printf("%s: %s\n", current->key, definitions->definitions[k]);
                                    inner_match = 1;
                                    match_found = 1;
                                }
                                
                                free(lower_input);
                                free(lower_current_key);
                                
                                if (inner_match) break;
                            }
                        }
                        if (inner_match) break;
                    }
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
    else if (strcmp(argv[1], "remove") == 0) {
        handle_remove_command(dictionary, removed_dict, removed_path, argv, argc);
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf remove <term>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }

        char term[MAX_INPUT_LENGTH] = "";
        for (int i = 2; i < argc; i++) {
            strcat(term, argv[i]);
            if (i < argc - 1) strcat(term, " ");
        }

        DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
        if (!definitions) {
            printf("Term '%s' not found in the dictionary.\n", term);
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }

        if (definitions->count == 1) {
            printf("Found definition:\n%s: %s\n", definitions->keys[0], definitions->definitions[0]);
            printf("Are you sure you want to remove this definition? [Y/n]: ");
            
            char response = getResponse();
            
            if (response == 'Y' || response == 'y') {
                // Add to removed.txt instead of actually deleting
                if (add_to_removed(removed_path, definitions->keys[0], definitions->definitions[0])) {
                    printf("Definition removed successfully.\n");
                }
            } else {
                printf("Operation aborted.\n");
            }
        } else {
            printf("Found definitions:\n");
            for (int i = 0; i < definitions->count; i++) {
                printf("%d. %s: %s\n", i + 1, definitions->keys[i], definitions->definitions[i]);
            }
            
            printf("Enter the numbers of definitions to remove (separated by space or comma): ");
            char input[MAX_INPUT_LENGTH];
            if (!getString(input, sizeof(input))) {
                printf("Error reading input.\n");
                free_definition_list(definitions);
                free_hash_table(dictionary);
                free_hash_table(removed_dict);
                return 1;
            }

            int to_remove[100] = {0};
            int remove_count = 0;
            char *token = strtok(input, " ,");
            
            while (token && remove_count < 100) {
                int num = atoi(token);
                if (num > 0 && num <= definitions->count) {
                    to_remove[remove_count++] = num - 1;
                }
                token = strtok(NULL, " ,");
            }

            if (remove_count > 0) {
                printf("Are you sure you want to remove these definitions? [Y/n]: ");
                char response = getResponse();

                if (response == 'Y' || response == 'y') {
                    int removed = 0;
                    for (int i = 0; i < remove_count; i++) {
                        int idx = to_remove[i];
                        if (add_to_removed(removed_path, definitions->keys[idx], 
                                         definitions->definitions[idx])) {
                            removed++;
                        }
                    }
                    printf("%d definition(s) removed successfully.\n", removed);
                } else {
                    printf("Operation aborted.\n");
                }
            } else {
                printf("No valid numbers selected.\n");
            }
        }
        free_definition_list(definitions);
    }
    else if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf add <term>:<definition>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }

        char input[MAX_INPUT_LENGTH] = "";
        for (int i = 2; i < argc; i++) {
            strcat(input, argv[i]);
            if (i < argc - 1) strcat(input, " ");
        }

        char *term = strtok(input, ":");
        char *definition = strtok(NULL, "");

        if (!term || !definition) {
            printf("Error: Invalid format. Use `wtf add <term>:<definition>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }
        
        handle_add_command(dictionary, added_path, term, definition);

        // Add to added.txt instead of definitions.txt
        if (add_to_added(added_path, term, definition)) {
            hash_table_insert(dictionary, term, definition);
            printf("Definition added successfully.\n");
        } else {
            printf("Error: Could not add definition.\n");
        }
    } else {
        printf("Error: Unknown command '%s'. Use `wtf -h` for help.\n", argv[1]);
    }

    // Free memory
    free_hash_table(dictionary);
    free_hash_table(removed_dict);
    return 0;
}