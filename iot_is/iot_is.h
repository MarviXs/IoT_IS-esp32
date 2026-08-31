#pragma once

#include <string>
#include <functional>
#include <atomic>
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

    /** \brief Publish a raw JSON string to devices/{token}/{topic_suffix}. */
    bool publish_json(const char *topic_suffix, const char *json);

    void set_job_received_callback(JobReceivedCallback callback);
    void set_job_control_received_callback(JobControlReceivedCallback callback);

    bool is_connected() const;
    bool is_connecting() const;
    bool can_publish() const;
    int outbox_size() const;

    /**
     * \brief Select QoS for telemetry data (send_data* family).
     *
     * 0 — fire and forget: no PUBACK; message leaves the outbox as soon as
     *     it is written to the socket. Lowest overhead, no delivery guarantee.
     * 1 — at least once: message waits in the outbox until the broker PUBACKs.
     *
     * Job status updates and publish_json always use QoS 1 regardless.
     * Takes effect from the next message; safe to call at any time.
     */
    void set_qos(uint8_t qos);
    uint8_t get_qos() const;

    // Written from the MQTT task's event handler, read from app tasks.
    // Atomic so the handler never has to take _lock (see locking rules below).
    std::atomic<bool> isConnected;

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

    // Locking rules (violating these deadlocks the MQTT task — see mqtt_event_handler):
    //  - _lock guards _mqttClient lifetime, _builder, config strings and stats. It MAY
    //    be held across esp_mqtt_client_* calls, BUT the MQTT task must never block on
    //    it. Therefore the event handler (which runs on the MQTT task, holding esp-mqtt's
    //    internal API lock) must never take _lock.
    //  - _cb_lock guards the job callbacks and _accessToken for reads from the MQTT
    //    task. It must only be held for plain memory access — never across any
    //    esp_mqtt_client_* call or other blocking operation.
    //  - esp_mqtt_client_stop()/destroy() block until the MQTT task exits its loop, so
    //    they must never be called while holding _lock (detach the handle under _lock,
    //    tear it down after releasing).
    SemaphoreHandle_t _lock;
    SemaphoreHandle_t _cb_lock;
    std::atomic<MqttConnState> _state;
    flatbuffers::FlatBufferBuilder _builder;

    JobReceivedCallback _job_received_callback;
    JobControlReceivedCallback _job_control_received_callback;

    // Flow-rate stats. _stat_ack is incremented from the MQTT task (event callback)
    // so it must be atomic — taking _lock there would deadlock with the app task
    // calling esp_mqtt_client_get_outbox_size() while holding _lock.
    uint32_t                _stat_tx;           // enqueued messages (under _lock)
    std::atomic<uint32_t>   _stat_ack;          // PUBACKs received (lock-free, MQTT task)
    int64_t                 _stat_reset_us;     // window start (under _lock)

    // Telemetry QoS (0 or 1). Atomic so setters need not take _lock.
    std::atomic<uint8_t>    _qos;

    bool ensure_client_created_locked();
    static void teardown_client(esp_mqtt_client_handle_t client);
    bool send_data_internal(const std::string &tag, double value, int64_t ts, double latitude, double longitude, int32_t gridX, int32_t gridY);
    void on_connected();
    void on_data_received(esp_mqtt_event_handle_t event);
    void process_received_job(esp_mqtt_event_handle_t event);
    void process_received_job_control(esp_mqtt_event_handle_t event);
    static int64_t get_current_time();
};

extern IoTIs iotIs;