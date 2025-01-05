#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "hash_table.h"
#include "file_utils.h"
#include "network_sync.h"
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 256

// Helper function to wrap text with proper indentation
void print_wrapped_definition(const char* text, int indent_size, int term_width, int is_last_item) {
    int line_pos = indent_size;
    int text_len = strlen(text);
    
    // Print first part (after the term:)
    for (int i = 0; i < text_len; i++) {
        if (line_pos >= term_width - 1) {  // -1 for safety margin
            // Use different prefix based on whether it's the last item
            if (is_last_item) {
                printf("\n%s %s%*s", COLOR_PRIMARY, COLOR_RESET, indent_size - 2, "");
            } else {
                printf("\n%s│%s%*s", COLOR_PRIMARY, COLOR_RESET, indent_size - 2, "");
            }
            line_pos = indent_size;
        }
        putchar(text[i]);
        line_pos++;
    }
}


// Handle "wtf is <term>" command
void handle_is_command(HashTable *dictionary, HashTable *removed_dict, char **args, int argc) {
    
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col;
    char term[256] = "";
    for (int i = 2; i < argc; i++) {
        strcat(term, args[i]);
        if (i < argc - 1) strcat(term, " ");
    }
    

    DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
    if (definitions) {
        int def_count = 0;
        
        // Count valid definitions
        for (int i = 0; i < definitions->count; i++) {
            if (!is_definition_removed(definitions->keys[i], definitions->definitions[i], removed_dict)) {
                def_count++;
            }
        }
        
        if (def_count > 0) {
            printf("\n%s╭─ Found %d definition%s for '%s%s%s'%s\n", 
                COLOR_PRIMARY, def_count, 
                (def_count > 1 ? "s" : ""),
                COLOR_YELLOW, term, COLOR_PRIMARY,
                COLOR_RESET);
            printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
            
            for (int i = 0; i < definitions->count; i++) {
                if (!is_definition_removed(definitions->keys[i], definitions->definitions[i], removed_dict)) {
                    
                    // Calculate indent size (tree symbol + term + ": ")
                    int indent_size = 4 + strlen(definitions->keys[i]) + 2;
                    
                    if (i == definitions->count - 1) {
                        printf("%s╰─ %s%s%s: ", 
                            COLOR_PRIMARY,
                            COLOR_YELLOW,
                            definitions->keys[i],
                            COLOR_RESET);
                    } else {
                        printf("%s├─ %s%s%s: ", 
                            COLOR_PRIMARY,
                            COLOR_YELLOW,
                            definitions->keys[i],
                            COLOR_RESET);
                    }
                    print_wrapped_definition(definitions->definitions[i], 
                                           indent_size, 
                                           term_width, 
                                           i == definitions->count - 1);
                    if (i < definitions->count - 1) {
                        printf("\n%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
                    }
                }
            }
            printf("\n");
            printf("\n");
        } else {
            printf("%s│%s\n",COLOR_PRIMARY, COLOR_RESET);
            printf("%s╰─%sLol.. I don't know what `%s%s%s` means\n\n", COLOR_PRIMARY, COLOR_RESET, COLOR_YELLOW, term, COLOR_RESET);
        }
        
        free_definition_list(definitions);
    } else {
        printf("%s│%s\n",COLOR_PRIMARY, COLOR_RESET);
        printf("%s╰─%sLol.. I don't know what `%s%s%s` means\n\n", COLOR_PRIMARY, COLOR_RESET, COLOR_YELLOW, term, COLOR_RESET);
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
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col;

    char term[MAX_INPUT_LENGTH] = "";
    for (int i = 2; i < argc; i++) {
        strcat(term, args[i]);
        if (i < argc - 1) strcat(term, " ");
    }

    DefinitionList *definitions = hash_table_lookup_all(dictionary, term);
    if (!definitions) {
        printf("\n%s╰─ Term '%s%s%s' not found in the dictionary%s\n\n", 
            COLOR_RED, COLOR_YELLOW, term, COLOR_RED, COLOR_RESET);
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
        printf("\n%s╰─ No definitions available to remove%s\n\n", COLOR_RED, COLOR_RESET);
        free_definition_list(definitions);
        free_definition_list(filtered);
        return;
    }

    if (filtered->count == 1) {
        // Single definition case
        printf("\n%s╭─ Found definition for '%s%s%s'%s\n",
            COLOR_PRIMARY, COLOR_YELLOW, term, COLOR_PRIMARY, COLOR_RESET);
        printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
        
        int indent_size = 4 + strlen(filtered->keys[0]) + 2;
        printf("%s╰─ %s%s%s: ", 
            COLOR_PRIMARY,
            COLOR_YELLOW,
            filtered->keys[0],
            COLOR_RESET);
        
        print_wrapped_definition(filtered->definitions[0], indent_size, term_width, 1);
        printf("\n\n");
        
        printf("\nAre you sure you want to remove this definition? [Y/n]: ");
        
        char response = getchar();
        while (getchar() != '\n');
        
        if (response == 'Y' || response == 'y') {
            if (add_to_removed(removed_path, filtered->keys[0], filtered->definitions[0])) {
                hash_table_insert(removed_dict, filtered->keys[0], filtered->definitions[0]);
                printf("\n%s╰─ Definition removed successfully%s\n\n", COLOR_SUCCESS, COLOR_RESET);
            }
        } else {
            printf("\n%s╰─ Operation aborted%s\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        // Multiple definitions case
        printf("\n%s╭─ Found %d definitions for '%s%s%s'%s\n",
            COLOR_PRIMARY, filtered->count,
            COLOR_YELLOW, term, COLOR_PRIMARY, COLOR_RESET);
        printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
        
        for (int i = 0; i < filtered->count; i++) {
            // Calculate indent size (number + ". " + term + ": ")
            int number_width = snprintf(NULL, 0, "%d", i + 1);
            int indent_size = 4 + number_width + 2 + strlen(filtered->keys[i]) + 2;
            
            if (i == filtered->count - 1) {
                printf("%s╰─ %s%d. %s%s: ",
                    COLOR_PRIMARY,
                    COLOR_YELLOW,
                    i + 1,
                    filtered->keys[i],
                    COLOR_RESET);
            } else {
                printf("%s├─ %s%d. %s%s: ",
                    COLOR_PRIMARY,
                    COLOR_YELLOW,
                    i + 1,
                    filtered->keys[i],
                    COLOR_RESET);
            }
            
            print_wrapped_definition(filtered->definitions[i], indent_size, term_width, i == filtered->count - 1);
            
            if (i < filtered->count - 1) {
                printf("\n%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
            }
        }
        
        printf("\n\nEnter the numbers of definitions to remove %s(separated by space or comma)%s: ", COLOR_YELLOW, COLOR_RESET);
            
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
                    printf("\n%s╰─ %s(%d)%s definition(s) removed successfully%s\n\n", 
                        COLOR_SUCCESS, COLOR_YELLOW, removed, COLOR_SUCCESS, COLOR_RESET);
                } else {
                    printf("\n%s╰─ No definitions were removed%s\n\n", 
                        COLOR_RED, COLOR_RESET);
                }
            } else {
                printf("\n%s╰─ Operation aborted%s\n\n", COLOR_RED, COLOR_RESET);
            }
        }
    }
    
    free_definition_list(definitions);
    free_definition_list(filtered);
}

void handle_recover_command(HashTable *removed_dict, const char *removed_path, char **args, int argc) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col;

    char term[MAX_INPUT_LENGTH] = "";
    for (int i = 2; i < argc; i++) {
        strcat(term, args[i]);
        if (i < argc - 1) strcat(term, " ");
    }

    DefinitionList *removed_defs = hash_table_lookup_all(removed_dict, term);
    if (!removed_defs) {
        printf("\n%s╰─ Term '%s%s%s' not found in removed definitions%s\n\n", 
            COLOR_RED, COLOR_YELLOW, term, COLOR_RED, COLOR_RESET);
        return;
    }

    if (removed_defs->count == 1) {
        // Single definition case
        printf("\n%s╭─ Found removed definition for '%s%s%s'%s\n",
            COLOR_PRIMARY, COLOR_YELLOW, term, COLOR_PRIMARY, COLOR_RESET);
        printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
        
        int indent_size = 4 + strlen(removed_defs->keys[0]) + 2;
        printf("%s╰─ %s%s%s: ", 
            COLOR_PRIMARY,
            COLOR_YELLOW,
            removed_defs->keys[0],
            COLOR_RESET);
        
        print_wrapped_definition(removed_defs->definitions[0], indent_size, term_width, 1);
        printf("\n\n");
        
        printf("\nAre you sure you want to recover this definition? [Y/n]: ");
        
        char response = getchar();
        while (getchar() != '\n');
        
        if (response == 'Y' || response == 'y') {
            if (remove_from_removed(removed_path, removed_defs->keys[0], removed_defs->definitions[0])) {
                hash_table_delete_single(removed_dict, removed_defs->keys[0], removed_defs->definitions[0]);
                printf("\n%s╰─ Definition recovered successfully%s\n\n", COLOR_SUCCESS, COLOR_RESET);
            } else {
                printf("\n%s╰─ Error: Could not recover definition%s\n\n", COLOR_RED, COLOR_RESET);
            }
        } else {
            printf("\n%s╰─ Operation aborted%s\n\n", COLOR_RED, COLOR_RESET);
        }
    } else {
        // Multiple definitions case
        printf("\n%s╭─ Found %d removed definitions for '%s%s%s'%s\n",
            COLOR_PRIMARY, removed_defs->count,
            COLOR_YELLOW, term, COLOR_PRIMARY, COLOR_RESET);
        printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
        
        for (int i = 0; i < removed_defs->count; i++) {
            int number_width = snprintf(NULL, 0, "%d", i + 1);
            int indent_size = 4 + number_width + 2 + strlen(removed_defs->keys[i]) + 2;
            
            if (i == removed_defs->count - 1) {
                printf("%s╰─ %s%d. %s%s: ",
                    COLOR_PRIMARY,
                    COLOR_YELLOW,
                    i + 1,
                    removed_defs->keys[i],
                    COLOR_RESET);
            } else {
                printf("%s├─ %s%d. %s%s: ",
                    COLOR_PRIMARY,
                    COLOR_YELLOW,
                    i + 1,
                    removed_defs->keys[i],
                    COLOR_RESET);
            }
            
            print_wrapped_definition(removed_defs->definitions[i], indent_size, term_width, i == removed_defs->count - 1);
            
            if (i < removed_defs->count - 1) {
                printf("\n%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
            }
        }
        
        printf("\n\nEnter the numbers of definitions to recover %s(separated by space or comma)%s: ", 
            COLOR_YELLOW, COLOR_RESET);
            
        char input[MAX_INPUT_LENGTH];
        if (fgets(input, sizeof(input), stdin)) {
            printf("Are you sure you want to recover these definitions? [Y/n]: ");
            char response = getchar();
            while (getchar() != '\n');
            
            if (response == 'Y' || response == 'y') {
                char *token = strtok(input, " ,\n");
                int recovered = 0;
                
                while (token) {
                    int num = atoi(token);
                    if (num > 0 && num <= removed_defs->count) {
                        if (remove_from_removed(removed_path, removed_defs->keys[num-1], 
                                             removed_defs->definitions[num-1])) {
                            hash_table_delete_single(removed_dict, removed_defs->keys[num-1], 
                                                   removed_defs->definitions[num-1]);
                            recovered++;
                        }
                    }
                    token = strtok(NULL, " ,\n");
                }
                
                if (recovered > 0) {
                    printf("\n%s╰─ %s(%d)%s definition(s) recovered successfully%s\n\n", 
                        COLOR_SUCCESS, COLOR_YELLOW, recovered, COLOR_SUCCESS, COLOR_RESET);
                } else {
                    printf("\n%s╰─ No definitions were recovered%s\n\n", COLOR_RED, COLOR_RESET);
                }
            } else {
                printf("\n%s╰─ Operation aborted%s\n\n", COLOR_RED, COLOR_RESET);
            }
        }
    }
    
    free_definition_list(removed_defs);
}