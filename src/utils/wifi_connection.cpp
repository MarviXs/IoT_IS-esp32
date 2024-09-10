#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include <Arduino.h>
#include <WiFi.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_TIMEOUT_MS 20000
#define WIFI_RETRY_INTERVAL_MS 30000

static const char *TAG = "WiFi";

typedef struct
{
    const char *ssid;
    const char *password;
} wifi_credentials_t;

void wifi_connect_task(void *pvParameters)
{
    wifi_credentials_t *credentials = (wifi_credentials_t *)pvParameters;

    while (1)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            ESP_LOGI(TAG, "Attempting to connect to WiFi...");
            WiFi.begin(credentials->ssid, credentials->password);

            unsigned long startAttemptTime = millis();

            while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
            {
                ESP_LOGI(TAG, "Connecting to WiFi...");
                vTaskDelay(pdMS_TO_TICKS(2000));
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                ESP_LOGI(TAG, "Successfully connected to WiFi");
                ESP_LOGI(TAG, "IP address: %s", WiFi.localIP().toString().c_str());
            }
            else
            {
                ESP_LOGE(TAG, "Failed to connect to WiFi");
                ESP_LOGI(TAG, "Retrying in %d ms", WIFI_RETRY_INTERVAL_MS);
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS));
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10000)); // Check connection every 10 seconds
        }
    }
}

void start_wifi_connection(const char *ssid, const char *password)
{
    wifi_credentials_t *credentials = (wifi_credentials_t *)malloc(sizeof(wifi_credentials_t));
    if (credentials == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for wifi credentials");
        return;
    }

    credentials->ssid = ssid;
    credentials->password = password;

    xTaskCreate(wifi_connect_task, "wifi_connect_task", 4096, (void *)credentials, 1, NULL);

    ESP_LOGI(TAG, "WiFi connection task started");
}
