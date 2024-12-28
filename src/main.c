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
        // Remove newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // If we got exactly one character (plus possibly a newline)
        if (strlen(line) == 1) {
            response = line[0];
        } else {
            response = 'n'; // Default to 'no' if input is invalid
        }
    } else {
        response = 'n'; // Default to 'no' if input fails
    }
    
    return response;
}

// For the string input:
int getString(char *buffer, size_t bufferSize) {
    if (fgets(buffer, bufferSize, stdin)) {
        // Remove newline if present
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
            return 1;  // Handle error if HOME directory is not found
        }
    
    // Construct the full path to the definitions file
    char definitions_path[1024];
    snprintf(definitions_path, sizeof(definitions_path), "%s/.wtf/res/definitions.txt", home_dir);
    if (argc < 2) {
        printf("Error: Missing arguments. Use `wtf -h` for help.\n");
        return 1;
    }
    

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }
    // Load the dictionary into memory
    HashTable *dictionary = create_hash_table(100);
    if (!load_definitions(definitions_path, dictionary)) {
        fprintf(stderr, "Error: Could not load definitions from file.\n");
        return 1;
    }

    if (strcmp(argv[1], "is") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf is <term>`.\n");
            free_hash_table(dictionary);
            return 1;
        }

        // Combine arguments to handle multi-word terms
        char term[MAX_INPUT_LENGTH] = "";
        for (int i = 2; i < argc; i++) {
            strcat(term, argv[i]);
            if (i < argc - 1) strcat(term, " "); // Add space between words
        }

        DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
        if (definitions) {
            int match_found = 0;
            
            // Safely print definitions and track whether a match was found
            for (int k = 0; k < definitions->count; k++) {
                // Find the matching original key for each definition
                for (int index = 0; index < dictionary->size; index++) {
                    int inner_match = 0;
                    for (HashNode *current = dictionary->table[index]; current != NULL; current = current->next) {
                        // Case-insensitive comparison
                        char *lower_input = safe_lowercase(term);
                        char *lower_current_key = safe_lowercase(current->key);
                        
                        if (lower_input && lower_current_key) {
                            // Check if the current definition matches the key
                            if (strcmp(lower_input, lower_current_key) == 0 && 
                                strcmp(definitions->definitions[k], current->value) == 0) {
                                printf("%s: %s\n", current->key, definitions->definitions[k]);
                                inner_match = 1;
                                match_found = 1;
                            }
                            
                            // Free temporary lowercase strings
                            free(lower_input);
                            free(lower_current_key);
                            
                            if (inner_match) break;
                        }
                    }
                    
                    if (inner_match) break;
                }
            }
            
            // Always free the definition list
            free_definition_list(definitions);
            
            // If no match was found
            if (!match_found) {
                printf("Lol I don't know what '%s' means.\n", term);
            }
        } else {
            printf("Lol I don't know what '%s' means.\n", term);
        }
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf remove <term>`.\n");
            free_hash_table(dictionary);
            return 1;
        }
    
        // Combine arguments to handle multi-word terms
        char term[MAX_INPUT_LENGTH] = "";
        for (int i = 2; i < argc; i++) {
            strcat(term, argv[i]);
            if (i < argc - 1) strcat(term, " ");
        }
    
        // Look up all definitions for the term
        DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
        if (!definitions) {
            printf("Term '%s' not found in the dictionary.\n", term);
            free_hash_table(dictionary);
            return 1;
        }
    
        if (definitions->count == 1) {
            // Case 1: Single definition
            printf("Found definition:\n%s: %s\n", definitions->keys[0], definitions->definitions[0]);
            printf("Are you sure you want to remove this definition? [Y/n]: ");
            
            char response = getResponse();
            
            if (response == 'Y' || response == 'y') {
                if (hash_table_delete_single(dictionary, definitions->keys[0], definitions->definitions[0])) {
                    // Update the definitions file
                    FILE *file = fopen(definitions_path, "w");
                    if (!file) {
                        fprintf(stderr, "Error: Could not open file for writing.\n");
                        free_definition_list(definitions);
                        free_hash_table(dictionary);
                        return 1;
                    }
    
                    // Write all remaining definitions back to file
                    for (int i = 0; i < dictionary->size; i++) {
                        for (HashNode *node = dictionary->table[i]; node != NULL; node = node->next) {
                            fprintf(file, "%s:%s\n", node->key, node->value);
                        }
                    }
                    fclose(file);
                    printf("Definition removed successfully.\n");
                }
            } else {
                printf("Operation aborted.\n");
            }
        } else {
            // Case 2: Multiple definitions
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
                return 1;
            }
    
            // Parse input numbers
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
                        if (hash_table_delete_single(dictionary, definitions->keys[idx], 
                                                   definitions->definitions[idx])) {
                            removed++;
                        }
                    }
    
                    // Update the definitions file
                    FILE *file = fopen(definitions_path, "w");
                    if (!file) {
                        fprintf(stderr, "Error: Could not open file for writing.\n");
                        free_definition_list(definitions);
                        free_hash_table(dictionary);
                        return 1;
                    }
    
                    // Write all remaining definitions back to file
                    for (int i = 0; i < dictionary->size; i++) {
                        for (HashNode *node = dictionary->table[i]; node != NULL; node = node->next) {
                            fprintf(file, "%s:%s\n", node->key, node->value);
                        }
                    }
                    fclose(file);
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
               return 1;
           }
   
           // Capture the full input string for term and definition
           char input[MAX_INPUT_LENGTH] = "";
           for (int i = 2; i < argc; i++) {
               strcat(input, argv[i]);
               if (i < argc - 1) strcat(input, " "); // Add spaces between words
           }
   
           // Split input into term and definition using ':' as delimiter
           char *term = strtok(input, ":");
           char *definition = strtok(NULL, "");
   
           if (!term || !definition) {
               printf("Error: Invalid format. Use `wtf add <term>:<definition>`.\n");
               free_hash_table(dictionary);
               return 1;
           }
   
           // Add the term and definition to the dictionary
           hash_table_insert(dictionary, term, definition);
   
           // Save the term and definition to the file
           FILE *file = fopen(definitions_path, "a");
           if (!file) {
               fprintf(stderr, "Error: Could not open file %s for appending.\n", definitions_path);
               free_hash_table(dictionary);
               return 1;
           }
           fprintf(file, "%s:%s\n", term, definition);
           fclose(file);
   
           printf("Definition added successfully.\n");
       } else {
           printf("Error: Unknown command '%s'. Use `wtf -h` for help.\n", argv[1]);
       }
   
       // Free memory
       free_hash_table(dictionary);
       return 0;
   }