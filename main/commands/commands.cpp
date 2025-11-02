#include "commands.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_PIN GPIO_NUM_26

bool toggleLed(const std::vector<double> &params){
    if (params.empty() || (params[0] != 0 && params[0] != 1))
        return false;
    bool state = (params[0] == 1);
    gpio_set_level(LED_PIN, state ? 1 : 0);
    ESP_LOGI("LED", "LED on pin %d turned %s.", LED_PIN, state ? "ON" : "OFF");
    iotIs.send_data("led", state ? 1 : 0);
    return true;
}

bool delayCommand(const std::vector<double> &params){
    if (params.empty())
        return false;

    int delayMs = static_cast<int>(params[0]);
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    return true;
}