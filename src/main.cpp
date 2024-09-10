#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include <WiFi.h>
#include "iot_is.h"
#include "job_manager.h"
#include "esp_log.h"
#include "utils/wifi_connection.h"
#include "utils/sntp.h"
#include <Wire.h>
#include "sensors/scd41.h"
#include "sensors/sht41.h"
#include "sensors/sgp41.h"

#define LED_PIN 4

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const std::string accessToken = "6iZUHMw5ct3rCMw9k7INpivJ";
const std::string mqttHost = "192.168.1.166";
const int mqttPort = 1883;

static const char *TAG = "main";

bool toggleLed(const std::vector<double> &params)
{
    if (params.empty() || (params[0] != 0 && params[0] != 1))
        return false;
    bool state = (params[0] == 1);
    digitalWrite(LED_PIN, state);
    ESP_LOGI(TAG, "LED on pin %d turned %s.", LED_PIN, state ? "ON" : "OFF");
    return true;
}

bool delayCommand(const std::vector<double> &params)
{
    if (params.empty())
        return false;

    int delayMs = static_cast<int>(params[0]);
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    return true;
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        vTaskDelay(pdMS_TO_TICKS(10));

    esp_log_level_set("*", ESP_LOG_INFO);

    pinMode(LED_PIN, OUTPUT);
    Wire.begin(SDA, SCL);

    job_manager.init();
    job_manager.register_command("delay", delayCommand);
    job_manager.register_command("toggle_led", toggleLed);

    start_wifi_connection(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    initSNTP(10 * 60 * 1000);

    initSHT41();
    initSCD41();
    initSGP41();

    iotIs.connect(accessToken, mqttHost, mqttPort);
}

void loop()
{
    vTaskDelay(1000);
}
