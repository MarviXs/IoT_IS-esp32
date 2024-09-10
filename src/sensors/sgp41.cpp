#include <Arduino.h>
#include <SensirionI2CSgp41.h>
#include <Wire.h>
#include <iot_is.h>
#include "sht41.h"
#include "sensirion_gas_index_algorithm/sensirion_gas_index_algorithm.h"

SensirionI2CSgp41 sgp41;
static char errorMessage[64];
static int16_t error;
uint16_t conditioning_s = 10;

static const char *TAG = "SGP41";

void readSGP41(void *parameter)
{
    GasIndexAlgorithmParams paramsVoc;
    GasIndexAlgorithm_init(&paramsVoc, GasIndexAlgorithm_ALGORITHM_TYPE_VOC);

    GasIndexAlgorithmParams paramsNox;
    GasIndexAlgorithm_init(&paramsNox, GasIndexAlgorithm_ALGORITHM_TYPE_NOX);

    uint16_t defaultRh = 0x8000;
    uint16_t defaultT = 0x6666;

    uint16_t srawVoc = 0;
    uint16_t srawNox = 0;

    int32_t IndexVoc = 0;
    int32_t IndexNox = 0;
    while (true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        float humidityTicks = (sht_41_humidity == 0) ? defaultRh : sht_41_humidity * 65535 / 100;
        float temperatureTicks = (sht_41_temperature == 0) ? defaultT : (sht_41_temperature + 45) * 65535 / 175;

        if (conditioning_s > 0)
        {
            // During NOx conditioning (10s) SRAW NOx will remain 0
            error = sgp41.executeConditioning(humidityTicks, temperatureTicks, srawVoc);
            conditioning_s--;
        }
        else
        {
            error = sgp41.measureRawSignals(humidityTicks, temperatureTicks, srawVoc, srawNox);
        }

        if (error)
        {
            errorToString(error, errorMessage, sizeof errorMessage);
            ESP_LOGE(TAG, "Error trying to read measurement: %s", errorMessage);
        }
        else
        {
            GasIndexAlgorithm_process(&paramsVoc, srawVoc, &IndexVoc);
            iotIs.send_data("voc", IndexVoc);
            if (srawNox != 0)
            {
                GasIndexAlgorithm_process(&paramsNox, srawNox, &IndexNox);
                iotIs.send_data("nox", IndexNox);
            }

            ESP_LOGI(TAG, "Index VOC: %d, Index NOx: %d", IndexVoc, IndexNox);
        }
    }

    vTaskDelete(NULL);
}

void initSGP41()
{
    sgp41.begin(Wire);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t serialNumberSize = 3;
    uint16_t serialNumber[serialNumberSize];

    error = sgp41.getSerialNumber(serialNumber);
    if (error)
    {
        errorToString(error, errorMessage, sizeof errorMessage);
        ESP_LOGE(TAG, "Error trying to get scd41 serial number: %s", errorMessage);
        return;
    }

    uint16_t testResult;
    error = sgp41.executeSelfTest(testResult);
    if (error)
    {
        errorToString(error, errorMessage, sizeof errorMessage);
        ESP_LOGE(TAG, "Error trying to execute self test: %s", errorMessage);
        return;
    }
    else if (testResult != 0xD400)
    {
        ESP_LOGE(TAG, "executeSelfTest failed with error: %d", testResult);
    }

    xTaskCreate(readSGP41, "readSGP41", 4096, NULL, 5, NULL);
}