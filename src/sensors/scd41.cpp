#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <iot_is.h>

SensirionI2CScd4x scd41;
static char errorMessage[64];
static int16_t error;

static const char *TAG = "SCD41";

void readSCD41(void *parameter)
{
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    bool isDataReady = false;
    while (true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        error = scd41.getDataReadyFlag(isDataReady);
        if (error)
        {
            errorToString(error, errorMessage, sizeof errorMessage);
            ESP_LOGE(TAG, "Error trying to get data ready flag: %s", errorMessage);
        }
        else if (isDataReady)
        {
            error = scd41.readMeasurement(co2, temperature, humidity);
            if (error)
            {
                errorToString(error, errorMessage, sizeof errorMessage);
                ESP_LOGE(TAG, "Error trying to read measurement: %s", errorMessage);
            }
            else if (co2 == 0)
            {
                ESP_LOGE(TAG, "CO2 level is zero, indicating a potential sensor issue.");
            }
            else
            {
                ESP_LOGI(TAG, "CO2 level: %d ppm", co2);

                iotIs.send_data("co2", co2);
                iotIs.send_data("temp2", temperature);
                iotIs.send_data("hum2", humidity);
            }
        }
    }

    vTaskDelete(NULL);
}

void initSCD41()
{
    scd41.begin(Wire);
    vTaskDelay(pdMS_TO_TICKS(100));

    error = scd41.stopPeriodicMeasurement();
    if (error)
    {
        errorToString(error, errorMessage, 256);
        ESP_LOGE(TAG, "Error trying to execute stopPeriodicMeasurement(): %s", errorMessage);
        return;
    }

    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    error = scd41.getSerialNumber(serial0, serial1, serial2);
    if (error)
    {
        errorToString(error, errorMessage, sizeof errorMessage);
        ESP_LOGE(TAG, "Error trying to get scd41 serial number: %s", errorMessage);
        return;
    }

    scd41.setSensorAltitude(148);
    error = scd41.startPeriodicMeasurement();
    if (error)
    {
        errorToString(error, errorMessage, 256);
        ESP_LOGE(TAG, "Error trying to execute startPeriodicMeasurement(): %s", errorMessage);
    }

    xTaskCreate(readSCD41, "readSCD41", 4096, NULL, 5, NULL);
}