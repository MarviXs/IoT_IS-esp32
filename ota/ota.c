#include "ota.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "semver_utils.h"
#include "esp_crt_bundle.h"

#define OTA_TAG "OTA"
#define HTTP_READ_CHUNK 1024
#define HTTP_TIMEOUT_MS 15000

typedef struct {
    int major, minor, patch;
    bool has_prerelease;
    char prerelease[32]; // enough for "rc.1", "dirty", "gabcdef", etc.
    bool is_dirty;       // true if prerelease contains "dirty"
} semver_t;

static esp_err_t http_get_all(esp_http_client_handle_t client, char **out_buf, int *out_len)
{
    *out_buf = NULL;
    *out_len = 0;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "open failed: %s", esp_err_to_name(err));
        return err;
    }

    int64_t cl = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(OTA_TAG, "HTTP status %d", status);
        esp_http_client_close(client);
        return ESP_FAIL;
    }

    // If content-length known, allocate once; otherwise grow as we read.
    int alloc = (cl > 0 && cl < 1024*1024) ? (int)cl : HTTP_READ_CHUNK; // cap initial alloc
    char *buf = (char *)malloc(alloc + 1);
    if (!buf) {
        ESP_LOGE(OTA_TAG, "malloc failed");
        esp_http_client_close(client);
        return ESP_ERR_NO_MEM;
    }

    int total = 0;
    while (1) {
        // Ensure capacity
        if (total + HTTP_READ_CHUNK + 1 > alloc) {
            int new_alloc = alloc + HTTP_READ_CHUNK;
            char *nb = (char *)realloc(buf, new_alloc + 1);
            if (!nb) {
                ESP_LOGE(OTA_TAG, "realloc failed");
                free(buf);
                esp_http_client_close(client);
                return ESP_ERR_NO_MEM;
            }
            buf = nb;
            alloc = new_alloc;
        }

        int r = esp_http_client_read(client, buf + total, HTTP_READ_CHUNK);
        if (r < 0) {
            ESP_LOGE(OTA_TAG, "read error");
            free(buf);
            esp_http_client_close(client);
            return ESP_FAIL;
        }
        if (r == 0) break;  // EOF
        total += r;
    }

    buf[total] = '\0';
    esp_http_client_close(client);

    *out_buf = buf;
    *out_len = total;
    return ESP_OK;
}

esp_err_t perform_ota_update(const char* base_url, const char* accessToken) {
    esp_http_client_config_t config = {
        .url = base_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .disable_auto_redirect = false,  // allow redirects
        .keep_alive_enable = true,
    };

    //1st we need to GET /devices/{accessToken}/firmwares/active to get the URL of the firmware
    char url[256];
    snprintf(url, sizeof(url), "%sdevices/%s/firmwares/active", base_url, accessToken);
    config.url = url;

    ESP_LOGI(OTA_TAG, "GET %s", url);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(OTA_TAG, "client init failed");
        return ESP_FAIL;
    }

    char *resp = NULL;
    int resp_len = 0;
    esp_err_t err = http_get_all(client, &resp, &resp_len);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) return err;

    if (resp_len <= 0) {
        ESP_LOGE(OTA_TAG, "empty body");
        free(resp);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp);
    if (root == NULL) {
        ESP_LOGE(OTA_TAG, "Failed to parse JSON");
        free(resp);
        return ESP_FAIL;
    }

    cJSON *firmwareId = cJSON_GetObjectItem(root, "firmwareId");
    cJSON *versionNumber = cJSON_GetObjectItem(root, "versionNumber");
    cJSON *originalFilename = cJSON_GetObjectItem(root, "originalFilename");
    cJSON *downloadUrl = cJSON_GetObjectItem(root, "downloadUrl");

    if (downloadUrl == NULL || downloadUrl->valuestring == NULL) {
        ESP_LOGE(OTA_TAG, "Failed to get download URL from JSON");
        cJSON_Delete(root);
        free(resp);
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "Firmware ID: %s", firmwareId->valuestring);
    ESP_LOGI(OTA_TAG, "Version Number: %s", versionNumber->valuestring);
    ESP_LOGI(OTA_TAG, "Original Filename: %s", originalFilename->valuestring);
    ESP_LOGI(OTA_TAG, "Download URL: %s", downloadUrl->valuestring);

    //check the version number here against current image number
    const char *curVer = esp_app_get_description()->version;
    const char *avail  = versionNumber->valuestring;

    int cmp = semver_compare(curVer, avail);
    ESP_LOGI(OTA_TAG, "Current: %s, Available: %s", curVer, versionNumber->valuestring);
    /* cmp > 0  -> current is newer
    cmp == 0 -> equal (same base + same prerelease)
    cmp < 0  -> available is newer (do OTA) */
    if (cmp >= 0) {
        ESP_LOGI(OTA_TAG, "No update needed.");
        cJSON_Delete(root);
        free(resp);
        return ESP_OK;
    }
    //hard copy the download URL to config for the ota process
    char downloadUrlStr[512];
    snprintf(downloadUrlStr, sizeof(downloadUrlStr), "%s%s", base_url, downloadUrl->valuestring+1);
    cJSON_Delete(root);
    free(resp);

    //2nd perform the OTA update from the obtained URL
    config.url = downloadUrlStr;
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA OK, restarting");
        esp_restart();
    } else {
        ESP_LOGE(OTA_TAG, "OTA update failed with error: %d", ret);
    }
    return ret;
}


esp_err_t updateFirmwareVersion(const char* base_url, const char* accessToken){
    //make a POST request to /devices/{accessToken}/firmwares/current with JSON body {"versionNumber": "x.y.z"}
    esp_http_client_config_t config = {
        .url = base_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST,
    };
    char url[256];
    snprintf(url, sizeof(url), "%sdevices/%s/firmwares/current", base_url, accessToken);
    config.url = url;

    ESP_LOGI(OTA_TAG, "Updating current firmware version at URL: %s", url);

    const char* versionNumber = esp_app_get_description()->version;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if(client == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    
    char postData[128];
    snprintf(postData, sizeof(postData), "{\"versionNumber\": \"%s\"}", versionNumber);
    esp_http_client_set_post_field(client, postData, strlen(postData));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(OTA_TAG, "Firmware version updated successfully to %s", versionNumber);
    } else {
        ESP_LOGE(OTA_TAG, "Failed to update firmware version, error: %d", err);
    }
    esp_http_client_cleanup(client);
    return err;
}


esp_err_t mark_app_valid_cancel_rollback(){
    return  esp_ota_mark_app_valid_cancel_rollback();
}