#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "hash_table.h"
#include "file_utils.h"
#include "network_sync.h"
#include "commands.h"
#include <limits.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_INPUT_LENGTH 256

void print_help() {
    printf("Usage:\n");
    printf("  wtf is <term>    - Get the definition of a term\n");
    printf("  wtf add <term>:<definition> - Add a new term and definition to the dictionary\n");
    printf("  wtf remove <term> - Remove definition(s) for a term\n");
    printf("  wtf recover <term> - Recover previously removed definition(s) for a term\n");
    printf("  wtf -h | --help  - Show this help menu\n");
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
    
    // Check for auto-sync at startup
    char config_dir[1024];
    snprintf(config_dir, sizeof(config_dir), "%s/.wtf", home_dir);
    check_and_auto_sync(config_dir, dictionary);
    
    // Load removed definitions
    load_definitions(removed_path, removed_dict);

    // Handle commands
    if (strcmp(argv[1], "is") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf is <term>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }
        handle_is_command(dictionary, removed_dict, argv, argc);
    }
    else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf remove <term>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }
        handle_remove_command(dictionary, removed_dict, removed_path, argv, argc);
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
    } else if (strcmp(argv[1], "recover") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf recover <term>`.\n");
            free_hash_table(dictionary);
            free_hash_table(removed_dict);
            return 1;
        }
        handle_recover_command(removed_dict, removed_path, argv, argc);
    } else if (strcmp(argv[1], "sync") == 0) {
            // Get config directory path
            char config_dir[1024];
            snprintf(config_dir, sizeof(config_dir), "%s/.wtf", home_dir);
            
            if (sync_delta_updates(config_dir, dictionary)) {
                // Save synced definitions
                save_definitions(definitions_path, dictionary);
                printf("Dictionary synchronized successfully.\n");
            } else {
                printf("Sync failed or dictionary is already up to date.\n");
            }
        } else if (strcmp(argv[1], "config") == 0) {
            if (argc < 4) {
                printf("Usage:\n");
                printf("  wtf config auto-sync [on|off]\n");
                printf("  wtf config sync-interval [hours]\n");
                free_hash_table(dictionary);
                free_hash_table(removed_dict);
                return 1;
            }
    
            char config_dir[1024];
            snprintf(config_dir, sizeof(config_dir), "%s/.wtf", home_dir);
            
            SyncMetadata metadata;
            load_sync_metadata(config_dir, &metadata);
    
            if (strcmp(argv[2], "auto-sync") == 0) {
                if (strcmp(argv[3], "on") == 0) {
                    metadata.auto_sync = 1;
                    printf("Auto-sync enabled.\n");
                } else if (strcmp(argv[3], "off") == 0) {
                    metadata.auto_sync = 0;
                    printf("Auto-sync disabled.\n");
                } else {
                    printf("Invalid value. Use 'on' or 'off'.\n");
                    free_hash_table(dictionary);
                    free_hash_table(removed_dict);
                    return 1;
                }
            } else if (strcmp(argv[2], "sync-interval") == 0) {
                // Special testing mode for intervals less than 1 hour
                if (strcmp(argv[3], "test") == 0) {
                    metadata.sync_interval = 5; // 5 seconds for testing
                    printf("Test mode: Sync interval set to 5 seconds.\n");
                } else {
                    int hours = atoi(argv[3]);
                    if (hours <= 0) {
                        printf("Invalid interval. Please specify a positive number of hours.\n");
                        free_hash_table(dictionary);
                        free_hash_table(removed_dict);
                        return 1;
                    }
                    metadata.sync_interval = hours * 3600; // Convert hours to seconds
                    printf("Sync interval set to %d hours.\n", hours);
                }
            } else {
                printf("Unknown config option. Use 'auto-sync' or 'sync-interval'.\n");
                free_hash_table(dictionary);
                free_hash_table(removed_dict);
                return 1;
            }
            
            save_sync_metadata(config_dir, &metadata);
        } else {
        printf("Error: Unknown command '%s'. Use `wtf -h` for help.\n", argv[1]);
    }

    // Free memory
    free_hash_table(dictionary);
    free_hash_table(removed_dict);
    return 0;
}