#include "wifi.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "provisioning.h"

#define TAG "WIFI"

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Recv creds ssid:%s", (const char *)sta_cfg->ssid);
            break;
        }
        case WIFI_PROV_CRED_FAIL:
            ESP_LOGW(TAG, "Provisioning failed (bad creds or unable to connect)");
            break;
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;
        case WIFI_PROV_END:
            ESP_LOGI(TAG, "Provisioning end");
            break;
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

esp_err_t wifi_init(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("WIFI", "NVS flash init failed, erasing and retrying...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();

    // Create default netifs for both STA and AP (AP used by SoftAP scheme)
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    // Register common handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI("WIFI", "WiFi initialized");

    //initialize provisioning if not done already
    start_provisioning();

    return ESP_OK;
}

esp_err_t wifi_set_creds(const char* ssid, const char* password){
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_LOGI("WIFI", "Setting WiFi credentials: SSID=%s", ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    return ESP_OK;
}

bool wifi_get_connected_status(){
    // Check WiFi connection status
    wifi_ap_record_t ap_info;
    esp_err_t status = esp_wifi_sta_get_ap_info(&ap_info);
    if (status == ESP_OK) {
        return true;
    } else {
        return false;
    }
}