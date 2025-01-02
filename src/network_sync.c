#include "network_sync.h"
#include <sys/stat.h>
#include <time.h>

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
        metadata->last_sync = 0;
        return;
    }
    
    if (fscanf(f, "%ld", &metadata->last_sync) != 1) {
        metadata->last_sync = 0;
    }
    
    fclose(f);
}

void save_sync_metadata(const char *config_dir, const SyncMetadata *metadata) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", config_dir, SYNC_METADATA_FILE);
    
    FILE *f = fopen(path, "w");
    if (!f) return;
    
    fprintf(f, "%ld", metadata->last_sync);
    fclose(f);
}

int sync_dictionary(const char *config_dir, HashTable *dictionary) {
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
        free(response.data);
        return 0;
    }
    
    fprintf(f, "%s", response.data);
    fclose(f);
    
    free(response.data);
    return 1;
}

int check_and_sync(const char *config_dir, HashTable *dictionary) {
    SyncMetadata metadata;
    load_sync_metadata(config_dir, &metadata);
    
    time_t current_time = time(NULL);
    
    // Check if 2 minutes have passed since last sync
    if ((current_time - metadata.last_sync) >= SYNC_INTERVAL) {
        if (is_network_available()) {
            if (sync_dictionary(config_dir, dictionary)) {
                metadata.last_sync = current_time;
                save_sync_metadata(config_dir, &metadata);
                return 1;
            }
        }
    }
    
    return 0;
}