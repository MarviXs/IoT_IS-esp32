#include <Arduino.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include <iot_is.h>
#include "sht41.h"

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

SensirionI2cSht4x sht41;
static char errorMessage[64];
static int16_t error;

static const char *TAG = "SHT41";

void readSHT41(void *parameter)
{
    while (true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        error = sht41.measureHighPrecision(sht_41_temperature, sht_41_humidity);
        if (error != NO_ERROR)
        {
            errorToString(error, errorMessage, sizeof errorMessage);
            ESP_LOGE(TAG, "Error trying to measure sht41: %s", errorMessage);
        }
        else
        {
            ESP_LOGI(TAG, "Temperature: %.2f °C, Humidity: %.2f %%", sht_41_temperature, sht_41_humidity);

            iotIs.send_data("temp", sht_41_temperature);
            iotIs.send_data("hum", sht_41_humidity);
        }
    }

    vTaskDelete(NULL);
}

void initSHT41()
{
    sht41.begin(Wire, SHT41_I2C_ADDR_44);
    sht41.softReset();
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t serialNumber = 0;
    error = sht41.serialNumber(serialNumber);

    if (error != NO_ERROR)
    {
        errorToString(error, errorMessage, sizeof errorMessage);
        ESP_LOGE(TAG, "Error trying to get sht41 serial number: %s", errorMessage);
        return;
    }

    xTaskCreate(readSHT41, "readSHT4x", 4096, NULL, 5, NULL);
}