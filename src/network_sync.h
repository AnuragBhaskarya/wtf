#ifndef NETWORK_SYNC_H
#define NETWORK_SYNC_H
#define COMPRESS_RESPONSE 1
#define COMPRESSION_THRESHOLD 1024 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include "hash_table.h"

#define USER_AGENT "WTF-Dictionary/1.0"
#define GITHUB_API_BASE "https://api.github.com"
#define GITHUB_REPO "AnuragBhaskarya/wtf"
#define DEFINITIONS_PATH ".wtf/res/definitions.txt"
#define SYNC_METADATA_FILE "sync.meta"
#define DEFAULT_SYNC_INTERVAL (24 * 60 * 60) // 24 hours

typedef struct {
    char *data;
    size_t size;
} NetworkResponse;

typedef struct {
    char last_commit_sha[41];  // Last synced commit SHA
    time_t last_sync;          // Last sync timestamp
    int auto_sync;            // Auto-sync enabled flag
    time_t sync_interval;     // Sync interval in seconds
} SyncMetadata;

typedef struct {
    char *term;
    char *definition;
    char action;  // 'A' for add, 'M' for modify, 'D' for delete
} DeltaUpdate;

typedef struct {
    DeltaUpdate *updates;
    size_t count;
} UpdateList;

// Function declarations
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
int is_network_available(void);
void load_sync_metadata(const char *config_dir, SyncMetadata *metadata);
void save_sync_metadata(const char *config_dir, const SyncMetadata *metadata);
void parse_patch_changes(const char *patch, UpdateList *updates);
char* get_latest_commit_sha(void);
int sync_full_dictionary(const char *config_dir, HashTable *dictionary);
UpdateList* get_dictionary_updates(const char *last_commit, const char *current_commit);
void apply_delta_updates(HashTable *dictionary, UpdateList *updates);
void free_update_list(UpdateList *list);
int check_and_auto_sync(const char *config_dir, HashTable *dictionary);
int sync_delta_updates(const char *config_dir, HashTable *dictionary);

#endif