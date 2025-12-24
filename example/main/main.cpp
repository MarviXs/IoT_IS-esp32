#include <stdio.h>

#include "wifi_provisioning/manager.h"
#include "iot_is.h"
#include "job_manager.h"
#include "scd41.h"
#include "sgp4x.h"
#include "sht4x.h"
#include "ota.h"
#include "wifi.h"
#include "sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_is.h"
#include "job_manager.h"
#include "config.h"
#include "esp_log.h"

#define TAG "MAIN"

extern "C" void app_main(void)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS); //wait for system tasks to settle
    wifi_init();

    //job manager init
    job_manager.init();

    mark_app_valid_cancel_rollback();

    //wait for wifi connection
    while(wifi_get_connected_status() != true){
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    //perform ota process
    updateFirmwareVersion(CFG_HTTP_BACKEND_URL,CFG_ACCESS_TOKEN);
    
    perform_ota_update(CFG_HTTP_BACKEND_URL,CFG_ACCESS_TOKEN);
    
    init_sntp(300000); //sync interval 5 minutes

    iotIs.connect(CFG_ACCESS_TOKEN, CFG_MQTT_BROKER_URL, CFG_MQTT_BROKER_PORT);

    while(!iotIs.isConnected){
        ESP_LOGI(TAG, "Waiting for MQTT connection...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    iotIs.send_data("device_started", 1);
    

    while(1){
        //main loop
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}