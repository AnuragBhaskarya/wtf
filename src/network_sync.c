#include "network_sync.h"
#include "file_utils.h"
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>

#define COLOR_BLUE    "\033[0;34m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_YELLOW  "\033[0;33m"

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    NetworkResponse *resp = (NetworkResponse *)userp;
    
    // Get total size on first call if not set
    if (resp->total_size == 0) {
        curl_off_t cl;
        curl_easy_getinfo(resp->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
        if (cl > 0) {
            resp->total_size = cl;
        }
    }
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) return 0;
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    // Calculate speed
    static time_t start_time = 0;
    if (start_time == 0) start_time = time(NULL);
    time_t current_time = time(NULL);
    double elapsed = difftime(current_time, start_time);
    if (elapsed > 0) {
        resp->speed = (double)resp->size / elapsed;
    }
    
    if (resp->show_progress) {
        display_progress(resp->size, resp->total_size, resp->speed);
    }
    
    return realsize;
}

void display_progress(size_t current, size_t total, double speed) {
    if (total == 0) return;
    
    const int bar_width = 40;
    float progress = (float)current / total;
    int filled = (int)(bar_width * progress);
    
    // Clear line and move cursor to start
    printf("\r\033[K");
    
    // Display progress bar
    printf("%s[%s", COLOR_CYAN, COLOR_BLUE);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("█");  // Full block for filled portion
        } else if (i == filled) {
            printf("%s▓%s", COLOR_YELLOW, COLOR_BLUE); // Half block for transition
        } else {
            printf("░");  // Light block for empty portion
        }
    }
    
    // Calculate sizes for display
    char size_str[32], speed_str[32];
    if (total < 1024*1024) {
        snprintf(size_str, sizeof(size_str), "%.1f/%.1f KB", 
                current/1024.0, total/1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%.1f/%.1f MB", 
                current/(1024.0*1024.0), total/(1024.0*1024.0));
    }
    
    if (speed < 1024) {
        snprintf(speed_str, sizeof(speed_str), "%.0f B/s", speed);
    } else if (speed < 1024*1024) {
        snprintf(speed_str, sizeof(speed_str), "%.1f KB/s", speed/1024);
    } else {
        snprintf(speed_str, sizeof(speed_str), "%.1f MB/s", speed/(1024*1024));
    }
    
    // Display statistics
    printf("%s] %s%3.0f%% %s%s %s%s%s", 
           COLOR_CYAN,                    // Close bracket color
           COLOR_YELLOW, progress * 100,  // Percentage
           COLOR_CYAN, size_str,         // Size information
           COLOR_BLUE, speed_str,        // Speed information
           COLOR_RESET);                 // Reset colors
    
    fflush(stdout);
    
    // Print newline when complete
    if (current == total) {
        printf("\n%sDownload complete!%s\n", COLOR_GREEN, COLOR_RESET);
    }
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
    response.show_progress = false; // Don't show progress for update check
    
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

size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t bytes = size * nitems;
    // Check for Content-Encoding header
    if (strncasecmp(buffer, "Content-Encoding:", 16) == 0) {
        // For debugging if needed
        // printf("Content-Encoding: %.*s", (int)bytes - 16, buffer + 16);
    }
    return bytes;
}

int sync_dictionary(const char *config_dir, HashTable *dictionary, const char *new_sha) {
    printf("%sUpdate of the dictionary available%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%sDownloading the update...%s\n", COLOR_GRAY, COLOR_RESET);
    
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    // Use raw.githubusercontent.com instead of the API
    char url[512];
    snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/main/%s", 
             GITHUB_REPO, DEFINITIONS_PATH);
    
    NetworkResponse response = {0};
    response.data = malloc(1);
    response.size = 0;
    response.total_size = 0;
    response.curl = curl;
    response.show_progress = true;
    
    struct curl_slist *headers = NULL;
    // Add compression headers
    headers = curl_slist_append(headers, "Accept-Encoding: gzip");
    headers = curl_slist_append(headers, "User-Agent: gzip");  // Some servers check this
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1L);
    curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Add this to verify we're getting compressed data
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    
    // Debug info to check if compression worked
    double content_length;
    char *content_encoding;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_encoding);
    
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