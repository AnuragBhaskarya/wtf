#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "hash_table.h"
#include "file_utils.h"
#include "network_sync.h"
#include "version.h"
#include "commands.h"
#include <limits.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_INPUT_LENGTH 256

void print_version() {
    printf("\n%s", COLOR_PRIMARY);
    printf("██╗    ██╗████████╗███████╗\n");
    printf("██║    ██║╚══██╔══╝██╔════╝\n");
    printf("██║ █╗ ██║   ██║   █████╗  \n");
    printf("██║███╗██║   ██║   ██╔══╝  \n");
    printf("╚███╔███╔╝   ██║   ██║     \n");
    printf(" ╚══╝╚══╝    ╚═╝   ╚═╝     \n");
    printf("%s\n", COLOR_RESET);

    printf("%s╭─%s WTF - Command Line Dictionary\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s Version    : %s%s%s\n", COLOR_PRIMARY, COLOR_PRIMARY, COLOR_DIM, WTF_VERSION, COLOR_RESET);
    printf("%s├─%s Author     : %s%s%s\n", COLOR_PRIMARY, COLOR_PRIMARY, COLOR_DIM, WTF_AUTHOR, COLOR_RESET);
    printf("%s├─%s Repository : %s%s%s\n", COLOR_PRIMARY, COLOR_PRIMARY, COLOR_DIM, WTF_REPO, COLOR_RESET);
    printf("%s├─%s Language   : %sC%s\n", COLOR_PRIMARY, COLOR_PRIMARY, COLOR_DIM, COLOR_RESET);
    printf("%s├─%s License    : %sMIT%s\n", COLOR_PRIMARY, COLOR_PRIMARY, COLOR_DIM, COLOR_RESET);
    printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s╰─%s Built by [%sAnurag Bhaskarya%s]\n\n", COLOR_PRIMARY, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
}

void print_help() {
    printf("\n%s╭─ Usage:%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s wtf is <term>\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│  └─ Get the definition of a term%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s wtf add <term>:<definition>\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│  └─ Add a new term and definition to the dictionary%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s wtf remove <term>\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│  └─ Remove definition(s) for a term%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s wtf recover <term>\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│  └─ Recover previously removed definition(s) for a term%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s wtf sync\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│  └─ Sync dictionary with latest updates%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s├─%s wtf sync --force\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s│  └─ Force sync dictionary with latest updates%s\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s╰─%s wtf -h | --help\n", COLOR_PRIMARY, COLOR_RESET);
    printf("%s   └─ Show this help menu%s\n\n", COLOR_PRIMARY, COLOR_RESET);
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
    
    char config_dir[PATH_MAX];
    char definitions_path[PATH_MAX];
    char added_path[PATH_MAX];
    char removed_path[PATH_MAX];
    
    // Initialize pointers to NULL at declaration
    HashTable *dictionary = NULL;
    HashTable *removed_dict = NULL;
    
    // Fix sign comparison warnings by storing snprintf result in size_t
    size_t written;
    
    written = (size_t)snprintf(config_dir, sizeof(config_dir), "%s/.wtf", home_dir);
    if (written >= sizeof(config_dir)) {
        fprintf(stderr, "Error: Path too long for config directory.\n");
        return 1;
    }
    
    written = (size_t)snprintf(definitions_path, sizeof(definitions_path), 
                                "%s/res/definitions.txt", config_dir);
    if (written >= sizeof(definitions_path)) {
        fprintf(stderr, "Error: Path too long for definitions file.\n");
        return 1;
    }
    
    written = (size_t)snprintf(added_path, sizeof(added_path), 
                                "%s/res/added.txt", config_dir);
    if (written >= sizeof(added_path)) {
        fprintf(stderr, "Error: Path too long for added definitions file.\n");
        return 1;
    }
    
    written = (size_t)snprintf(removed_path, sizeof(removed_path), 
                                "%s/res/removed.txt", config_dir);
    if (written >= sizeof(removed_path)) {
        fprintf(stderr, "Error: Path too long for removed definitions file.\n");
        return 1;
    }
    
    // Check for update only once at startup and only if:
    // 1. It's been more than interval since last check
    // 2. This is the first command of the day
    time_t current_time = time(NULL);
    SyncMetadata metadata;
    load_sync_metadata(config_dir, &metadata);
    
    // Get current date parts
    struct tm *tm_now = localtime(&current_time);
    int current_day = tm_now->tm_mday;
    int current_month = tm_now->tm_mon;
    int current_year = tm_now->tm_year;
    
    // Get last sync date parts
    struct tm *tm_last = localtime(&metadata.last_sync);
    int last_day = tm_last->tm_mday;
    int last_month = tm_last->tm_mon;
    int last_year = tm_last->tm_year;
    
    // Check if it's a new day
    int is_new_day = (current_day != last_day || 
                        current_month != last_month || 
                        current_year != last_year);
    
    if (argc < 2) {
        printf("Error: Missing arguments. Use `wtf -h` for help.\n");
        goto cleanup;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        if (argc > 2) {
            printf("\n%s╭─ Error%s: Invalid parameter %s'%s'%s\n", COLOR_RED, COLOR_RESET, COLOR_YELLOW, argv[2], COLOR_RESET);
            printf("%s│%s\n",COLOR_RED, COLOR_RESET);
            printf("%s├─%s Did you mean: %swtf -h%s or %swtf --help%s\n",COLOR_RED, COLOR_RESET, COLOR_PRIMARY, COLOR_RESET, COLOR_PRIMARY, COLOR_RESET);
            printf("%s│%s\n",COLOR_RED, COLOR_RESET);
            printf("%s╰─%s Get help using this command.\n\n",COLOR_RED, COLOR_RESET);
            goto cleanup;
        }
        print_help();
        goto cleanup;
    }

    // Load the main dictionary and user additions into memory
    dictionary = create_hash_table(100);
    if (!dictionary) {
        fprintf(stderr, "Error: Could not create main dictionary.\n");
        goto cleanup;
    }

    removed_dict = create_hash_table(100);
    if (!removed_dict) {
        fprintf(stderr, "Error: Could not create removed dictionary.\n");
        goto cleanup;
    }
    
    if (!dictionary || !removed_dict) {
        fprintf(stderr, "Error: Could not create hash tables.\n");
        goto cleanup;
    }
    
    bool is_force_sync = (argc > 2 && strcmp(argv[1], "sync") == 0 && strcmp(argv[2], "--force") == 0);
    
    // Only try to load definitions if we're not doing a force sync
    if (!is_force_sync) {
        if (!load_definitions(definitions_path, dictionary)) {
            fprintf(stderr, "Error: Could not load main definitions. try running \"wtf sync --force\"\n");
            goto cleanup;
        }
    }
    
    // Load user-added definitions
    load_definitions(added_path, dictionary);   
    
    // Load removed definitions
    load_definitions(removed_path, removed_dict);

    // Handle commands
    if (strcmp(argv[1], "is") == 0) {
        if (argc < 3) {
            printf("\n%s╰─ Error%s: No term provided. Use `wtf is <term>`\n\n", COLOR_RED, COLOR_RESET);
            goto cleanup;
        }
        handle_is_command(dictionary, removed_dict, argv, argc);
        
        // Show definition immediately without checking for updates
        // After showing the definition, check for updates in background
        if (is_new_day || (current_time - metadata.last_sync) >= SYNC_INTERVAL) {
            check_and_sync(config_dir, dictionary, false);
        }
            
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("\n%s╰─ Error%s: No term provided. Use `wtf remove <term>`\n\n", COLOR_RED, COLOR_RESET);
            goto cleanup;
        }
        handle_remove_command(dictionary, removed_dict, removed_path, argv, argc);
    }
    else if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            printf("Error: No term provided. Use `wtf add <term>:<definition>`.\n");
            goto cleanup;
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
            goto cleanup;
        }
        
        handle_add_command(dictionary, added_path, term, definition);
        // Check for updates after adding
        if (is_new_day || (current_time - metadata.last_sync) >= SYNC_INTERVAL) {
            check_and_sync(config_dir, dictionary, false);
        }
    } else if (strcmp(argv[1], "recover") == 0) {
        if (argc < 3) {
            printf("\n%s╰─ Error%s: No term provided. Use `wtf recover <term>`\n\n", COLOR_RED, COLOR_RESET);
            goto cleanup;
        }
        handle_recover_command(removed_dict, removed_path, argv, argc);
    } // Only check for updates if:
    // 1. It's a new day and this is the first command
    // 2. Explicit sync command is used
    else if (strcmp(argv[1], "sync") == 0) {
        // Force sync when explicit command is used
        bool force_sync = false;
        // Check if there are additional parameters
        if (argc > 2) {
            if (strcmp(argv[2], "--force") == 0) {
                force_sync = true;
            } else {
                // Invalid parameter
                printf("\n%s╭─ Error%s: Invalid parameter %s'%s'%s\n", COLOR_RED, COLOR_RESET, COLOR_YELLOW, argv[2], COLOR_RESET);
                printf("%s│%s\n",COLOR_RED, COLOR_RESET);
                printf("%s├─%s Did you mean: %swtf sync --force%s\n",COLOR_RED, COLOR_RESET, COLOR_PRIMARY, COLOR_RESET);
                printf("%s│%s\n",COLOR_RED, COLOR_RESET);
                printf("%s╰─%s It force syncs dictionary with remote repository \n\n",COLOR_RED, COLOR_RESET);
                goto cleanup;
            }
        }
        char current_sha[41] = {0};
        SyncStatus status = check_and_sync(config_dir, dictionary, force_sync);
    
        switch(status) {
            case SYNC_NOT_NEEDED:
                printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
                printf("%s├─ %s✓%s Checking...%s\n", COLOR_PRIMARY, COLOR_SUCCESS, COLOR_PRIMARY, COLOR_RESET);
                printf("%s╰─ %s✓%s Dictionary is up-to-date!%s\n\n", COLOR_PRIMARY, COLOR_SUCCESS, COLOR_PRIMARY, COLOR_RESET);
                break;
            case SYNC_NO_INTERNET:
                printf("%s│%s\n", COLOR_PRIMARY, COLOR_RESET);
                printf("%s╰─ %s! Check Your Internet Connection!%s\n\n", COLOR_PRIMARY, COLOR_RED, COLOR_RESET);
                break;
            case SYNC_NEEDED:
                break;
            case SYNC_ERROR:
                printf("%sError: Could not sync dictionary%s\n", COLOR_RED, COLOR_RESET);
                break;
        }
        
        if (status == SYNC_NEEDED) {
            SyncMetadata metadata;
            metadata.last_sync = time(NULL);
            check_for_updates(config_dir, current_sha);
            strncpy(metadata.last_sha, current_sha, sizeof(metadata.last_sha) - 1);
            metadata.last_sha[sizeof(metadata.last_sha) - 1] = '\0';
            save_sync_metadata(config_dir, &metadata);
        }
    } else if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        if (argc > 2) {
            printf("\n%s╭─ Error%s: Invalid parameter %s'%s'%s\n", COLOR_RED, COLOR_RESET, COLOR_YELLOW, argv[2], COLOR_RESET);
            printf("%s│%s\n",COLOR_RED, COLOR_RESET);
            printf("%s├─%s Did you mean: %swtf -v%s or %swtf --version%s\n",COLOR_RED, COLOR_RESET, COLOR_PRIMARY, COLOR_RESET, COLOR_PRIMARY, COLOR_RESET);
            printf("%s│%s\n",COLOR_RED, COLOR_RESET);
            printf("%s╰─%s Get the current version info\n\n",COLOR_RED, COLOR_RESET);
            goto cleanup;
        }
        print_version();
        goto cleanup;
    } else {
        printf("Error: Unknown command '%s'. Use `wtf -h` for help.\n", argv[1]);
    }
    
    cleanup:
        if (dictionary) {
            free_hash_table(dictionary);
            dictionary = NULL;
        }
        if (removed_dict) {
            free_hash_table(removed_dict);
            removed_dict = NULL;
        }
        return 0;
    }