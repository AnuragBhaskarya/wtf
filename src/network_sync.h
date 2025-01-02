#ifndef NETWORK_SYNC_H
#define NETWORK_SYNC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <time.h>
#include "hash_table.h"

#define USER_AGENT "WTF-Dictionary/1.0"
#define GITHUB_API_BASE "https://api.github.com"
#define GITHUB_REPO "AnuragBhaskarya/wtf"
#define DEFINITIONS_PATH ".wtf/res/definitions.txt"
#define SYNC_METADATA_FILE "sync.meta"
#define SYNC_INTERVAL 10

// ANSI color codes
#define COLOR_GREEN "\033[0;32m"
#define COLOR_GRAY "\033[0;37m"
#define COLOR_RED "\033[0;31m"
#define COLOR_RESET "\033[0m"

typedef struct {
    char *data;
    size_t size;
    size_t total_size;
    double speed;
    CURL *curl;
    bool show_progress;
} NetworkResponse;

typedef struct {
    time_t last_sync;
    char last_sha[41];  // SHA-1 hash is 40 chars + null terminator
} SyncMetadata;

typedef enum {
    SYNC_NOT_NEEDED,
    SYNC_NEEDED,
    SYNC_ERROR
} SyncStatus;

// Function declarations
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
int is_network_available(void);
void load_sync_metadata(const char *config_dir, SyncMetadata *metadata);
void save_sync_metadata(const char *config_dir, const SyncMetadata *metadata);
SyncStatus check_for_updates(const char *config_dir, char *current_sha);
int sync_dictionary(const char *config_dir, HashTable *dictionary, const char *new_sha);
SyncStatus check_and_sync(const char *config_dir, HashTable *dictionary);
void display_progress(size_t current, size_t total, double speed);

#endif