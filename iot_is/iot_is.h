#pragma once

#include <string>
#include <functional>
#include <sys/time.h>

#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "fbs/Job_generated.h"
#include "fbs/JobControl_generated.h"

class IoTIs
{
public:
    using JobReceivedCallback = std::function<void(JobFlatBuffers::JobT &)>;
    using JobControlReceivedCallback = std::function<void(JobFlatBuffers::JobControlT &)>;

    IoTIs();
    ~IoTIs();

    void connect(const std::string &accessToken, const std::string &mqttHost, int mqttPort);
    void disconnect();

    bool send_data(const std::string &tag, double value);
    bool send_data(const std::string &tag, double value, int64_t ts);
    bool send_data_with_location(const std::string &tag, double value, double latitude, double longitude);
    bool send_data_with_location(const std::string &tag, double value, int64_t ts, double latitude, double longitude);
    bool send_data_with_grid(const std::string &tag, double value, int32_t gridX, int32_t gridY);
    bool send_data_with_grid(const std::string &tag, double value, int64_t ts, int32_t gridX, int32_t gridY);

    bool update_job_status(JobFlatBuffers::JobT &job);

    void set_job_received_callback(JobReceivedCallback callback);
    void set_job_control_received_callback(JobControlReceivedCallback callback);

    bool is_connected() const;
    bool is_connecting() const;
    bool can_publish() const;
    int outbox_size() const;

    bool isConnected;

    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

private:
    enum class MqttConnState : uint8_t
    {
        IDLE = 0,
        CONNECTING,
        CONNECTED
    };

    esp_mqtt_client_handle_t _mqttClient;
    std::string _accessToken;
    std::string _mqttHost;
    int _mqttPort;

    SemaphoreHandle_t _lock;
    MqttConnState _state;
    flatbuffers::FlatBufferBuilder _builder;

    JobReceivedCallback _job_received_callback;
    JobControlReceivedCallback _job_control_received_callback;

    bool ensure_client_created_locked();
    bool send_data_internal(const std::string &tag, double value, int64_t ts, double latitude, double longitude, int32_t gridX, int32_t gridY);
    void on_connected();
    void on_data_received(esp_mqtt_event_handle_t event);
    void process_received_job(esp_mqtt_event_handle_t event);
    void process_received_job_control(esp_mqtt_event_handle_t event);
    static int64_t get_current_time();
};

extern IoTIs iotIs;