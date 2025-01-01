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
    } else {
        printf("Error: Unknown command '%s'. Use `wtf -h` for help.\n", argv[1]);
    }

    // Free memory
    free_hash_table(dictionary);
    free_hash_table(removed_dict);
    return 0;
}