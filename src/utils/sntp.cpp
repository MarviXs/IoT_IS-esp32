#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include "esp_sntp.h"
#include "esp_timer.h"
#include <esp_log.h>

static const char *TAG = "time";

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized");
}

void initSNTP(uint32_t syncIntervalMs)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_interval(300000); // Set sync interval to 5 minutes (300000 ms)
    sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2023 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (timeinfo.tm_year < (2023 - 1900))
    {
        ESP_LOGE(TAG, "System time NOT set.");
    }
    else
    {
        ESP_LOGI(TAG, "System time is set.");
    }
}