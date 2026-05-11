#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include <WiFi.h>
#include "iot_is.h"
#include "job_manager.h"
#include "esp_log.h"
#include "ota.h"
#include "utils/wifi_connection.h"
#include "utils/sntp.h"
#include "esp_timer.h"
#include <Wire.h>
#include <map>
#include <cstring>
#include "freertos/semphr.h"

#define LED_PIN 26
#define MQTT_BENCHMARK_DURATION_US 30000000LL
#define MQTT_LATENCY_HISTOGRAM_MS_MAX 10000

const char *ssid = "";
const char *password = "";

const std::string accessToken = "6iZUHMw5ct3rCMw9k7INpivJ";
const std::string mqttHost = "";
const int mqttPort = 1883;

static const char *TAG = "main";
static SemaphoreHandle_t benchmarkMutex = nullptr;
static std::map<int, int64_t> pendingPublishes;
static uint32_t latencyHistogram[MQTT_LATENCY_HISTOGRAM_MS_MAX + 1] = {};
static uint64_t acknowledgedPublishes = 0;
static uint64_t failedPublishes = 0;
static uint64_t latencySumUs = 0;
static bool benchmarkActive = false;

static bool lockBenchmarkStats(TickType_t ticksToWait)
{
    if (benchmarkMutex == nullptr)
    {
        benchmarkMutex = xSemaphoreCreateMutex();
    }

    return benchmarkMutex != nullptr && xSemaphoreTake(benchmarkMutex, ticksToWait) == pdTRUE;
}

static void unlockBenchmarkStats()
{
    xSemaphoreGive(benchmarkMutex);
}

static void resetBenchmarkStats()
{
    if (!lockBenchmarkStats(portMAX_DELAY))
    {
        return;
    }

    pendingPublishes.clear();
    memset(latencyHistogram, 0, sizeof(latencyHistogram));
    acknowledgedPublishes = 0;
    failedPublishes = 0;
    latencySumUs = 0;
    benchmarkActive = true;
    unlockBenchmarkStats();
}

static void onMqttDataPublished(int msgId)
{
    const int64_t nowUs = esp_timer_get_time();

    if (!lockBenchmarkStats(portMAX_DELAY))
    {
        return;
    }

    if (benchmarkActive)
    {
        const auto pending = pendingPublishes.find(msgId);
        if (pending != pendingPublishes.end())
        {
            const uint64_t latencyUs = nowUs - pending->second;
            uint32_t latencyMs = latencyUs / 1000;
            if (latencyMs > MQTT_LATENCY_HISTOGRAM_MS_MAX)
            {
                latencyMs = MQTT_LATENCY_HISTOGRAM_MS_MAX;
            }

            latencyHistogram[latencyMs]++;
            latencySumUs += latencyUs;
            acknowledgedPublishes++;
            pendingPublishes.erase(pending);
        }
    }
    unlockBenchmarkStats();
}

static double calculateP95LatencyMs(uint64_t acknowledged)
{
    if (acknowledged == 0)
    {
        return 0.0;
    }

    const uint64_t target = (acknowledged * 95 + 99) / 100;
    uint64_t seen = 0;

    for (uint32_t latencyMs = 0; latencyMs <= MQTT_LATENCY_HISTOGRAM_MS_MAX; latencyMs++)
    {
        seen += latencyHistogram[latencyMs];
        if (seen >= target)
        {
            return static_cast<double>(latencyMs);
        }
    }

    return static_cast<double>(MQTT_LATENCY_HISTOGRAM_MS_MAX);
}

static void benchmarkMqttBroker()
{
    ESP_LOGI(TAG, "Starting MQTT broker benchmark for 30 seconds...");
    resetBenchmarkStats();
    iotIs.set_data_published_callback(onMqttDataPublished);

    const int64_t startUs = esp_timer_get_time();
    const int64_t endUs = startUs + MQTT_BENCHMARK_DURATION_US;
    uint64_t sent = 0;

    while (iotIs.isConnected && esp_timer_get_time() < endUs)
    {
        const double value = (sent & 1) ? 1.0 : 0.0;
        if (!lockBenchmarkStats(portMAX_DELAY))
        {
            yield();
            continue;
        }

        const int64_t publishStartUs = esp_timer_get_time();
        const int msgId = iotIs.enqueue_data("led", value);
        if (msgId == -1)
        {
            failedPublishes++;
        }
        else
        {
            pendingPublishes[msgId] = publishStartUs;
            sent++;
        }
        unlockBenchmarkStats();

        if ((sent & 0x3FF) == 0)
        {
            yield();
        }
    }

    const int64_t elapsedUs = esp_timer_get_time() - startUs;
    const double elapsedSeconds = elapsedUs / 1000000.0;
    const double messagesPerSecond = elapsedSeconds > 0.0 ? sent / elapsedSeconds : 0.0;

    if (!lockBenchmarkStats(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "MQTT benchmark failed: could not lock benchmark stats");
        return;
    }

    benchmarkActive = false;
    const uint64_t acknowledged = acknowledgedPublishes;
    const uint64_t failed = failedPublishes;
    const uint64_t pending = pendingPublishes.size();
    const uint64_t latencyTotalUs = latencySumUs;
    const double p95LatencyMs = calculateP95LatencyMs(acknowledged);
    unlockBenchmarkStats();

    const double averageLatencyMs = acknowledged > 0 ? (latencyTotalUs / 1000.0) / acknowledged : 0.0;

    ESP_LOGI(TAG, "MQTT benchmark finished: sent=%llu acked=%llu pending=%llu failed=%llu elapsed=%.3fs speed=%.2f/s avg_latency=%.2fms p95_latency=%.2fms",
             static_cast<unsigned long long>(sent),
             static_cast<unsigned long long>(acknowledged),
             static_cast<unsigned long long>(pending),
             static_cast<unsigned long long>(failed),
             elapsedSeconds,
             messagesPerSecond,
             averageLatencyMs,
             p95LatencyMs);
}

bool toggleLed(const std::vector<double> &params)
{
    if (params.empty() || (params[0] != 0 && params[0] != 1))
        return false;
    bool state = (params[0] == 1);
    digitalWrite(LED_PIN, state);
    ESP_LOGI(TAG, "LED on pin %d turned %s.", LED_PIN, state ? "ON" : "OFF");
    iotIs.send_data("led", state ? 1 : 0);
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

    // mark firmware valid - if any fatal error occurs after update, rollback to previous firmware will be done up to this point
    esp_ota_mark_app_valid_cancel_rollback();

    start_wifi_connection(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        ESP_LOGI(TAG, "Connecting to WiFi...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    initSNTP(10 * 60 * 1000);

    iotIs.connect(accessToken, mqttHost, mqttPort);
    while (!iotIs.isConnected)
    {
        ESP_LOGI(TAG, "Waiting for MQTT connection...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    iotIs.send_data("led", 0);

    benchmarkMqttBroker();
}

void loop()
{
    vTaskDelay(1000);
}

