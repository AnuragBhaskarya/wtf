#include <stdio.h>
#include <string.h>
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
                for (int i = 0; i < definitions->count; i++) {
                    printf("%s: %s\n",term, definitions->definitions[i]);
                }
                free_definition_list(definitions);
            } else {
                printf("Lol I don't know what '%s' means.\n", term);
            }
    } else if (strcmp(argv[1], "add") == 0) {
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