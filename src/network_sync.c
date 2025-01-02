#include "network_sync.h"
#include "file_utils.h"
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    NetworkResponse *resp = (NetworkResponse *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) return 0;
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    // Calculate download speed
    static time_t start_time = 0;
    if (start_time == 0) start_time = time(NULL);
    time_t current_time = time(NULL);
    double elapsed = difftime(current_time, start_time);
    if (elapsed > 0) {
        resp->speed = (double)resp->size / elapsed;
    }
    
    // Display progress
    display_progress(resp->size, resp->total_size, resp->speed);
    
    return realsize;
}

void display_progress(size_t current, size_t total, double speed) {
    if (total == 0) return;
    
    int bar_width = 50;
    float progress = (float)current / total;
    int filled = (int)(bar_width * progress);
    
    printf("\r%s[", COLOR_GRAY);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) printf("=");
        else printf(" ");
    }
    printf("] %.1f%% (%.2f KB/s)%s", progress * 100, speed / 1024, COLOR_RESET);
    fflush(stdout);
    
    if (current == total) printf("\n");
}

int is_network_available(void) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_BASE);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);        // Reduced timeout to 1 second
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L); // Add connect timeout
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Disable SSL verification for speed
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    return (res == CURLE_OK);
}

void load_sync_metadata(const char *config_dir, SyncMetadata *metadata) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", config_dir, SYNC_METADATA_FILE);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        metadata->last_sync = 0;
        metadata->last_sha[0] = '\0';
        return;
    }
    
    if (fscanf(f, "%ld %40s", &metadata->last_sync, metadata->last_sha) != 2) {
        metadata->last_sync = 0;
        metadata->last_sha[0] = '\0';
    }
    
    fclose(f);
}

void save_sync_metadata(const char *config_dir, const SyncMetadata *metadata) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", config_dir, SYNC_METADATA_FILE);
    
    FILE *f = fopen(path, "w");
    if (!f) return;
    
    fprintf(f, "%ld %s", metadata->last_sync, metadata->last_sha);
    fclose(f);
}

SyncStatus check_for_updates(const char *config_dir, char *current_sha) {
    // Load current metadata
    SyncMetadata metadata;
    load_sync_metadata(config_dir, &metadata);
    
    CURL *curl = curl_easy_init();
    if (!curl) return SYNC_ERROR;
    
    char url[512];
    snprintf(url, sizeof(url), "%s/repos/%s/contents/%s", 
             GITHUB_API_BASE, GITHUB_REPO, DEFINITIONS_PATH);
    
    NetworkResponse response = {0};
    response.data = malloc(1);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);         // 2 second timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L);  // 1 second connect timeout
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        free(response.data);
        return SYNC_ERROR;
    }
    
    // Parse JSON response to get SHA
    char *sha_start = strstr(response.data, "\"sha\":\"");
    if (!sha_start) {
        free(response.data);
        return SYNC_ERROR;
    }
    
    sha_start += 7;  // Skip \"sha\":\"
    strncpy(current_sha, sha_start, 40);
    current_sha[40] = '\0';
    
    // Compare with last known SHA
    int needs_update = (metadata.last_sha[0] == '\0' || 
                       strcmp(current_sha, metadata.last_sha) != 0);
    
    free(response.data);
    return needs_update ? SYNC_NEEDED : SYNC_NOT_NEEDED;
}

int sync_dictionary(const char *config_dir, HashTable *dictionary, const char *new_sha) {
    printf("%sUpdate of the dictionary available%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%sDownloading the update...%s\n", COLOR_GRAY, COLOR_RESET);
    
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    char url[512];
    snprintf(url, sizeof(url), "%s/repos/%s/contents/%s", 
             GITHUB_API_BASE, GITHUB_REPO, DEFINITIONS_PATH);
    
    NetworkResponse response = {0};
    response.data = malloc(1);
    response.size = 0;
    response.total_size = 0;  // Will be set by Content-Length
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3.raw");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        printf("%sError occurred while updating%s\n", COLOR_RED, COLOR_RESET);
        free(response.data);
        return 0;
    }
    
    // Ensure res directory exists
    char res_dir[512];
    snprintf(res_dir, sizeof(res_dir), "%s/res", config_dir);
    mkdir(res_dir, 0755);
    
    // Save the downloaded dictionary
    char def_path[512];
    snprintf(def_path, sizeof(def_path), "%s/res/definitions.txt", config_dir);
    
    FILE *f = fopen(def_path, "w");
    if (!f) {
        printf("%sError occurred while updating%s\n", COLOR_RED, COLOR_RESET);
        free(response.data);
        return 0;
    }
    
    fprintf(f, "%s", response.data);
    fclose(f);
    
    // Update metadata
    SyncMetadata metadata;
    metadata.last_sync = time(NULL);
    strncpy(metadata.last_sha, new_sha, sizeof(metadata.last_sha) - 1);
    save_sync_metadata(config_dir, &metadata);
    
    printf("%sUpdate Successful!%s\n", COLOR_GREEN, COLOR_RESET);
    
    // Clear and reload dictionary
    hash_table_clear(dictionary);
    load_definitions(def_path, dictionary);
    
    free(response.data);
    return 1;
}

SyncStatus check_and_sync(const char *config_dir, HashTable *dictionary) {
    if (!is_network_available()) {
        return SYNC_NOT_NEEDED;
    }
    
    SyncMetadata metadata;
    load_sync_metadata(config_dir, &metadata);
    
    time_t current_time = time(NULL);
    
    // Check if 2 minutes have passed since last sync check
    if ((current_time - metadata.last_sync) >= SYNC_INTERVAL) {
        char current_sha[41];
        SyncStatus status = check_for_updates(config_dir, current_sha);
        
        if (status == SYNC_ERROR) {
            return SYNC_ERROR;
        }
        
        // Compare SHA with last known SHA
        if (strcmp(current_sha, metadata.last_sha) != 0) {
            // SHA different - update needed
            if (sync_dictionary(config_dir, dictionary, current_sha)) {
                return SYNC_NEEDED;  // Successfully updated
            } else {
                return SYNC_ERROR;   // Update failed
            }
        } else {
            // No update needed, but update last sync time
            metadata.last_sync = current_time;
            save_sync_metadata(config_dir, &metadata);
            return SYNC_NOT_NEEDED;
        }
    }
    
    return SYNC_NOT_NEEDED;
}