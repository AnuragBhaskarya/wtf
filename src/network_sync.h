#ifndef NETWORK_SYNC_H
#define NETWORK_SYNC_H

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
#define SYNC_INTERVAL 120  // 2 minutes in seconds

typedef struct {
    char *data;
    size_t size;
} NetworkResponse;

typedef struct {
    time_t last_sync;          // Last sync timestamp
} SyncMetadata;

// Function declarations
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
int is_network_available(void);
void load_sync_metadata(const char *config_dir, SyncMetadata *metadata);
void save_sync_metadata(const char *config_dir, const SyncMetadata *metadata);
int sync_dictionary(const char *config_dir, HashTable *dictionary);
int check_and_sync(const char *config_dir, HashTable *dictionary);

#endif