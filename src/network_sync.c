#include "network_sync.h"
#include <json-c/json.h>
#include "network_sync.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>


// Implementation of the write_callback function
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    NetworkResponse *resp = (NetworkResponse *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) return 0;
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    return realsize;
}

int is_network_available(void) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_BASE);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

void load_sync_metadata(const char *config_dir, SyncMetadata *metadata) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", config_dir, SYNC_METADATA_FILE);

    FILE *f = fopen(path, "r");
    if (!f) {
        metadata->last_commit_sha[0] = '\0';
        metadata->last_sync = 0;
        metadata->auto_sync = 0;
        metadata->sync_interval = DEFAULT_SYNC_INTERVAL;
        return;
    }

    int items_read = fscanf(f, "%40s\n%ld\n%d\n%ld",
           metadata->last_commit_sha,
           &metadata->last_sync,
           &metadata->auto_sync,
           &metadata->sync_interval);

    if (items_read != 4) {
        // Handle error - reset to defaults
        metadata->last_commit_sha[0] = '\0';
        metadata->last_sync = 0;
        metadata->auto_sync = 0;
        metadata->sync_interval = DEFAULT_SYNC_INTERVAL;
    }

    fclose(f);
}

void save_sync_metadata(const char *config_dir, const SyncMetadata *metadata) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", config_dir, SYNC_METADATA_FILE);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "%s\n%ld\n%d\n%ld",
            metadata->last_commit_sha,
            metadata->last_sync,
            metadata->auto_sync,
            metadata->sync_interval);
    fclose(f);
}

char* get_latest_commit_sha(void) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s/repos/%s/commits/main", GITHUB_API_BASE, GITHUB_REPO);

    NetworkResponse response = {0};
    response.data = malloc(1);
    response.size = 0;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response.data);
        return NULL;
    }

    json_object *root = json_tokener_parse(response.data);
    json_object *sha;
    char *commit_sha = NULL;

    if (json_object_object_get_ex(root, "sha", &sha)) {
        commit_sha = strdup(json_object_get_string(sha));
    }

    json_object_put(root);
    free(response.data);

    return commit_sha;
}

void parse_patch_changes(const char *patch, UpdateList *updates) {
    char *line = strdup(patch);
    char *saveptr;
    char *token = strtok_r(line, "\n", &saveptr);

    while (token) {
        if (token[0] == '+' || token[0] == '-') {
            char *sep = strchr(token + 1, ':');
            if (sep) {
                *sep = '\0';
                DeltaUpdate update;
                update.term = strdup(token + 1);
                update.definition = strdup(sep + 1);
                update.action = (token[0] == '+') ? 'A' : 'D';

                updates->updates[updates->count++] = update;
            }
        }
        token = strtok_r(NULL, "\n", &saveptr);
    }

    free(line);
}

void free_update_list(UpdateList *list) {
    if (!list) return;

    for (size_t i = 0; i < list->count; i++) {
        free(list->updates[i].term);
        free(list->updates[i].definition);
    }
    free(list->updates);
    free(list);
}

int sync_full_dictionary(const char *config_dir, HashTable *dictionary __attribute__((unused))) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    char url[512];
    snprintf(url, sizeof(url), "%s/repos/%s/contents/%s", 
             GITHUB_API_BASE, GITHUB_REPO, DEFINITIONS_PATH);
    
    NetworkResponse response = {0};
    response.data = malloc(1);
    response.size = 0;
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3.raw");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate");  // Add compression
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");  // Enable automatic decompression
    
    printf("Debug: Starting download...\n");
    CURLcode res = curl_easy_perform(curl);
    
    double downloaded_size;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloaded_size);
    printf("Debug: Downloaded %.2f KB\n", downloaded_size/1024.0);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        printf("Debug: Download failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }
    
    // Create the res directory if it doesn't exist
    char res_dir[512];
    snprintf(res_dir, sizeof(res_dir), "%s/res", config_dir);
    mkdir(res_dir, 0755);
    
    char def_path[512];
    snprintf(def_path, sizeof(def_path), "%s/res/definitions.txt", config_dir);
    
    FILE *f = fopen(def_path, "w");
    if (!f) {
        printf("Debug: Failed to open file for writing: %s\n", def_path);
        free(response.data);
        return 0;
    }
    
    fprintf(f, "%s", response.data);
    fclose(f);
    
    printf("Debug: Dictionary saved to %s\n", def_path);
    free(response.data);
    return 1;
}

UpdateList* get_dictionary_updates(const char *last_commit, const char *current_commit) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    
    char url[512];
    snprintf(url, sizeof(url), "%s/repos/%s/compare/%s...%s",
             GITHUB_API_BASE, GITHUB_REPO, last_commit, current_commit);
    
    NetworkResponse response = {0};
    response.data = malloc(1);
    response.size = 0;
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    
    printf("Debug: Fetching updates...\n");
    CURLcode res = curl_easy_perform(curl);
    
    double downloaded_size;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloaded_size);
    printf("Debug: Downloaded %.2f KB\n", downloaded_size/1024.0);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        free(response.data);
        return NULL;
    }

    UpdateList *updates = malloc(sizeof(UpdateList));
    updates->updates = NULL;
    updates->count = 0;

    json_object *root = json_tokener_parse(response.data);
    json_object *files;

    if (json_object_object_get_ex(root, "files", &files)) {
        int file_count = json_object_array_length(files);
        updates->updates = malloc(sizeof(DeltaUpdate) * file_count);

        for (int i = 0; i < file_count; i++) {
            json_object *file = json_object_array_get_idx(files, i);
            json_object *filename, *status, *patch;

            json_object_object_get_ex(file, "filename", &filename);
            json_object_object_get_ex(file, "status", &status);
            json_object_object_get_ex(file, "patch", &patch);

            if (strcmp(json_object_get_string(filename), DEFINITIONS_PATH) == 0) {
                parse_patch_changes(json_object_get_string(patch), updates);
            }
        }
    }

    json_object_put(root);
    free(response.data);

    return updates;
}

void apply_delta_updates(HashTable *dictionary, UpdateList *updates) {
    for (size_t i = 0; i < updates->count; i++) {
        DeltaUpdate *update = &updates->updates[i];
        
        printf("Debug: Processing update for term '%s' with action '%c'\n", update->term, update->action);
        
        switch (update->action) {
            case 'A':
            case 'M':
                hash_table_insert(dictionary, update->term, update->definition);
                break;
            case 'D':
                // Add debug output before deletion
                printf("Debug: Attempting to delete term '%s'\n", update->term);
                if (hash_table_lookup(dictionary, update->term) != NULL) {
                    hash_table_delete_key(dictionary, update->term);
                    printf("Debug: Successfully deleted term '%s'\n", update->term);
                } else {
                    printf("Debug: Term '%s' not found in dictionary\n", update->term);
                }
                break;
        }
    }
}

int check_and_auto_sync(const char *config_dir, HashTable *dictionary) {
    SyncMetadata metadata;
    load_sync_metadata(config_dir, &metadata);

    if (!metadata.auto_sync ||
        (time(NULL) - metadata.last_sync) < metadata.sync_interval) {
        return 0;
    }

    return sync_delta_updates(config_dir, dictionary);
}

int sync_delta_updates(const char *config_dir, HashTable *dictionary) {
    printf("Debug: Starting sync check...\n");

    if (!is_network_available()) {
        printf("Debug: Network not available\n");
        return 0;
    }

    SyncMetadata metadata;
    load_sync_metadata(config_dir, &metadata);
    printf("Debug: Last commit SHA: %s\n", metadata.last_commit_sha);
    printf("Debug: Last sync time: %ld\n", metadata.last_sync);
    printf("Debug: Auto sync: %d\n", metadata.auto_sync);
    printf("Debug: Sync interval: %ld\n", metadata.sync_interval);

    char *current_commit = get_latest_commit_sha();
    if (!current_commit) {
        printf("Debug: Failed to get current commit SHA\n");
        return 0;
    }
    printf("Debug: Current commit SHA: %s\n", current_commit);

    if (metadata.last_commit_sha[0] == '\0') {
        printf("Debug: No previous sync, performing full sync...\n");
        int result = sync_full_dictionary(config_dir, dictionary);
        if (result) {
            strncpy(metadata.last_commit_sha, current_commit, 40);
            metadata.last_sync = time(NULL);
            save_sync_metadata(config_dir, &metadata);
            printf("Debug: Full sync completed successfully\n");
        } else {
            printf("Debug: Full sync failed\n");
        }
        free(current_commit);
        return result;
    }

    printf("Debug: Getting updates between %s and %s\n",
           metadata.last_commit_sha, current_commit);

    UpdateList *updates = get_dictionary_updates(metadata.last_commit_sha, current_commit);
    if (!updates) {
        printf("Debug: Failed to get updates\n");
        free(current_commit);
        return 0;
    }

    printf("Debug: Found %zu updates\n", updates->count);

    if (updates->count > 0) {
        printf("Debug: Applying updates...\n");
        for (size_t i = 0; i < updates->count; i++) {
            printf("Debug: Update %zu - Action: %c, Term: %s\n",
                   i, updates->updates[i].action, updates->updates[i].term);
        }

        apply_delta_updates(dictionary, updates);
        printf("Applied %zu definition updates.\n", updates->count);

        strncpy(metadata.last_commit_sha, current_commit, 40);
        metadata.last_sync = time(NULL);
        save_sync_metadata(config_dir, &metadata);
        printf("Debug: Updates applied and metadata saved\n");
    } else {
        printf("Dictionary is up to date.\n");
    }

    free_update_list(updates);
    free(current_commit);
    return 1;
}
