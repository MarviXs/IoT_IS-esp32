#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include "iot_is.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "command_registry.h"
#include "fbs/DataPoint_generated.h"
#include "fbs/Job_generated.h"
#include "fbs/JobControl_generated.h"

#define TAG "IoTIs"
#define MQTT_OUTBOX_LIMIT_BYTES  (8 * 1024)
#define MQTT_STATS_INTERVAL_US   30000000LL  // print flow report every 30 s

IoTIs::IoTIs()
    : isConnected(false),
      _mqttClient(nullptr),
      _accessToken(""),
      _mqttHost(""),
      _mqttPort(0),
      _lock(xSemaphoreCreateMutex()),
      _state(MqttConnState::IDLE),
      _builder(256),
      _stat_tx(0),
      _stat_ack(0),
      _stat_reset_us(0),
      _qos(1)
{
}

void IoTIs::set_qos(uint8_t qos)
{
    if (qos > 1)
    {
        ESP_LOGW(TAG, "QoS %u not supported, clamping to 1", qos);
        qos = 1;
    }
    _qos.store(qos, std::memory_order_relaxed);
    ESP_LOGI(TAG, "Telemetry QoS set to %u", qos);
}

uint8_t IoTIs::get_qos() const
{
    return _qos.load(std::memory_order_relaxed);
}

IoTIs::~IoTIs()
{
    disconnect();

    if (_lock != nullptr)
    {
        vSemaphoreDelete(_lock);
        _lock = nullptr;
    }
}

bool IoTIs::ensure_client_created_locked()
{
    if (_mqttClient != nullptr)
    {
        return true;
    }

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = _mqttHost.c_str();
    mqtt_cfg.broker.address.port = _mqttPort;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    mqtt_cfg.buffer.size = 2048;
    mqtt_cfg.session.disable_clean_session = true;

    mqtt_cfg.credentials.client_id = _accessToken.c_str();
    mqtt_cfg.credentials.username = _accessToken.c_str();

    mqtt_cfg.outbox.limit = MQTT_OUTBOX_LIMIT_BYTES;

    _mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    if (_mqttClient == nullptr)
    {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return false;
    }

    esp_err_t err = esp_mqtt_client_register_event(
        _mqttClient,
        static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
        mqtt_event_handler,
        this);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(_mqttClient);
        _mqttClient = nullptr;
        return false;
    }

    return true;
}

void IoTIs::connect(const std::string &accessToken, const std::string &mqttHost, int mqttPort)
{
    xSemaphoreTake(_lock, portMAX_DELAY);

    const bool cfg_changed =
        (_accessToken != accessToken) ||
        (_mqttHost != mqttHost) ||
        (_mqttPort != mqttPort);

    _accessToken = accessToken;
    _mqttHost = mqttHost;
    _mqttPort = mqttPort;

    if (cfg_changed && _mqttClient != nullptr)
    {
        ESP_LOGI(TAG, "MQTT config changed, recreating client");
        esp_mqtt_client_unregister_event(_mqttClient,
                                         static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                         mqtt_event_handler);
        esp_mqtt_client_stop(_mqttClient);
        esp_mqtt_client_destroy(_mqttClient);
        _mqttClient = nullptr;
        _state = MqttConnState::IDLE;
        isConnected = false;
    }

    if (_state == MqttConnState::CONNECTED || _state == MqttConnState::CONNECTING)
    {
        xSemaphoreGive(_lock);
        return;
    }

    if (!ensure_client_created_locked())
    {
        xSemaphoreGive(_lock);
        return;
    }

    _state = MqttConnState::CONNECTING;
    isConnected = false;

    esp_err_t err = esp_mqtt_client_start(_mqttClient);
    if (err == ESP_ERR_INVALID_STATE || err == ESP_FAIL)
    {
        ESP_LOGI(TAG, "MQTT client already running or transitional, waiting for internal reconnect");
        err = ESP_OK;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT start failed: %s", esp_err_to_name(err));
        _state = MqttConnState::IDLE;
    }
    else
    {
        ESP_LOGI(TAG, "MQTT connect initiated");
    }

    xSemaphoreGive(_lock);
}

void IoTIs::disconnect()
{
    xSemaphoreTake(_lock, portMAX_DELAY);

    if (_mqttClient != nullptr)
    {
        esp_mqtt_client_unregister_event(_mqttClient,
                                         static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                         mqtt_event_handler);
        esp_mqtt_client_stop(_mqttClient);
        esp_mqtt_client_destroy(_mqttClient);
        _mqttClient = nullptr;
    }

    _state = MqttConnState::IDLE;
    isConnected = false;

    xSemaphoreGive(_lock);
}

bool IoTIs::can_publish() const
{
    return _mqttClient != nullptr && _state == MqttConnState::CONNECTED && isConnected;
}

bool IoTIs::is_connected() const
{
    return _state == MqttConnState::CONNECTED && isConnected;
}

bool IoTIs::is_connecting() const
{
    return _state == MqttConnState::CONNECTING;
}

int IoTIs::outbox_size() const
{
    if (_mqttClient == nullptr)
    {
        return -1;
    }
    return esp_mqtt_client_get_outbox_size(_mqttClient);
}

bool IoTIs::send_data(const std::string &tag, double value)
{
    return send_data(tag, value, get_current_time());
}

bool IoTIs::send_data(const std::string &tag, double value, int64_t ts)
{
    return send_data_internal(tag, value, ts, -1.0, -1.0, -1, -1);
}

bool IoTIs::send_data_with_location(const std::string &tag, double value, double latitude, double longitude)
{
    return send_data_with_location(tag, value, get_current_time(), latitude, longitude);
}

bool IoTIs::send_data_with_location(const std::string &tag, double value, int64_t ts, double latitude, double longitude)
{
    return send_data_internal(tag, value, ts, latitude, longitude, -1, -1);
}

bool IoTIs::send_data_with_grid(const std::string &tag, double value, int32_t gridX, int32_t gridY)
{
    return send_data_with_grid(tag, value, get_current_time(), gridX, gridY);
}

bool IoTIs::send_data_with_grid(const std::string &tag, double value, int64_t ts, int32_t gridX, int32_t gridY)
{
    return send_data_internal(tag, value, ts, -1.0, -1.0, gridX, gridY);
}

bool IoTIs::send_data_internal(const std::string &tag, double value, int64_t ts,
                               double latitude, double longitude, int32_t gridX, int32_t gridY)
{
    xSemaphoreTake(_lock, portMAX_DELAY);

    if (_mqttClient == nullptr || _state != MqttConnState::CONNECTED || !isConnected)
    {
        xSemaphoreGive(_lock);
        return false;
    }

    const int outbox_before = esp_mqtt_client_get_outbox_size(_mqttClient);
    if (outbox_before >= (int)MQTT_OUTBOX_LIMIT_BYTES)
    {
        ESP_LOGW(TAG, "MQTT outbox full-ish (%d B), dropping sample", outbox_before);
        xSemaphoreGive(_lock);
        return false;
    }

    _builder.Clear();

    auto tag_off = _builder.CreateString(tag);

    auto datapoint = DataPointFlatBuffers::CreateDataPoint(
        _builder,
        tag_off,
        value,
        ts,
        latitude,
        longitude,
        gridX,
        gridY);

    _builder.Finish(datapoint);

    std::string topic = "devices/" + _accessToken + "/data";

    // QoS 0 needs store=true or esp_mqtt_client_enqueue refuses the message;
    // stored QoS 0 messages leave the outbox as soon as they hit the socket.
    const uint8_t qos = _qos.load(std::memory_order_relaxed);

    int msg_id = esp_mqtt_client_enqueue(
        _mqttClient,
        topic.c_str(),
        reinterpret_cast<const char *>(_builder.GetBufferPointer()),
        _builder.GetSize(),
        qos,
        0,
        qos == 0);

    if (msg_id < 0)
    {
        ESP_LOGW(TAG, "MQTT enqueue failed: msg_id=%d heap=%lu outbox=%d",
                 msg_id,
                 esp_get_free_heap_size(),
                 esp_mqtt_client_get_outbox_size(_mqttClient));
        xSemaphoreGive(_lock);
        return false;
    }

    // Flow-rate accounting — report every 30 s; warn when ACKs trail enqueues
    _stat_tx++;
    {
        int64_t now_us = esp_timer_get_time();
        if (_stat_reset_us == 0) _stat_reset_us = now_us;
        int64_t elapsed = now_us - _stat_reset_us;
        if (elapsed >= MQTT_STATS_INTERVAL_US)
        {
            float    s        = elapsed / 1e6f;
            uint32_t ack_snap = _stat_ack.exchange(0, std::memory_order_relaxed);
            float    tx_rate  = _stat_tx  / s;
            float    ack_rate = ack_snap   / s;
            int      ob       = esp_mqtt_client_get_outbox_size(_mqttClient);
            // QoS 0 has no PUBACKs — ACK count is meaningless there,
            // so the lag warning only applies at QoS 1.
            if (qos == 1 && ack_snap < _stat_tx)
                ESP_LOGW(TAG, "MQTT flow [%.0fs]: TX=%.1f ACK=%.1f msg/s outbox=%d B — broker lagging",
                         s, tx_rate, ack_rate, ob);
            else if (qos == 1)
                ESP_LOGI(TAG, "MQTT flow [%.0fs]: TX=%.1f ACK=%.1f msg/s outbox=%d B OK",
                         s, tx_rate, ack_rate, ob);
            else
                ESP_LOGI(TAG, "MQTT flow [%.0fs]: TX=%.1f msg/s outbox=%d B (QoS 0)",
                         s, tx_rate, ob);
            _stat_tx       = 0;
            _stat_reset_us = now_us;
        }
    }

    xSemaphoreGive(_lock);
    return true;
}

void IoTIs::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)base;
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    IoTIs *instance = static_cast<IoTIs *>(handler_args);

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xSemaphoreTake(instance->_lock, portMAX_DELAY);
        instance->isConnected = true;
        instance->_state = MqttConnState::CONNECTED;
        xSemaphoreGive(instance->_lock);
        instance->on_connected();
        break;

    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        xSemaphoreTake(instance->_lock, portMAX_DELAY);
        instance->isConnected = false;
        instance->_state = MqttConnState::CONNECTING;
        xSemaphoreGive(instance->_lock);
        break;

    case MQTT_EVENT_PUBLISHED:
        instance->_stat_ack.fetch_add(1, std::memory_order_relaxed);
        break;

    case MQTT_EVENT_DATA:
        instance->on_data_received(event);
        break;

    default:
        break;
    }
}

void IoTIs::on_connected()
{
    if (_mqttClient == nullptr)
    {
        return;
    }

    std::string topic_job = "devices/" + _accessToken + "/job";
    std::string topic_job_ctrl = "devices/" + _accessToken + "/job/control";

    esp_mqtt_client_subscribe(_mqttClient, topic_job.c_str(), 1);
    esp_mqtt_client_subscribe(_mqttClient, topic_job_ctrl.c_str(), 2);
}

void IoTIs::on_data_received(esp_mqtt_event_handle_t event)
{
    std::string topic(event->topic, event->topic_len);

    if (topic == "devices/" + _accessToken + "/job")
    {
        process_received_job(event);
    }
    else if (topic == "devices/" + _accessToken + "/job/control")
    {
        process_received_job_control(event);
    }
}

void IoTIs::process_received_job(esp_mqtt_event_handle_t event)
{
    flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t *>(event->data), event->data_len);
    if (!JobFlatBuffers::VerifyJobBuffer(verifier))
    {
        ESP_LOGE(TAG, "Invalid Job buffer");
        return;
    }

    JobFlatBuffers::JobT job;
    JobFlatBuffers::GetJob(event->data)->UnPackTo(&job);

    ESP_LOGI(TAG, "Received job: %s with %ld steps and %ld cycles",
             job.name.c_str(), job.total_steps, job.total_cycles);

    xSemaphoreTake(_lock, portMAX_DELAY);
    auto cb = _job_received_callback;
    xSemaphoreGive(_lock);

    if (cb)
    {
        cb(job);
    }
    else
    {
        ESP_LOGW(TAG, "No job received callback set");
    }
}

void IoTIs::process_received_job_control(esp_mqtt_event_handle_t event)
{
    flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t *>(event->data), event->data_len);
    if (!JobFlatBuffers::VerifyJobControlBuffer(verifier))
    {
        ESP_LOGE(TAG, "Invalid JobControl buffer");
        return;
    }

    JobFlatBuffers::JobControlT job_control;
    JobFlatBuffers::GetJobControl(event->data)->UnPackTo(&job_control);

    xSemaphoreTake(_lock, portMAX_DELAY);
    auto cb = _job_control_received_callback;
    xSemaphoreGive(_lock);

    if (cb)
    {
        cb(job_control);
    }
    else
    {
        ESP_LOGW(TAG, "No job control received callback set");
    }
}

void IoTIs::set_job_received_callback(JobReceivedCallback callback)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    _job_received_callback = std::move(callback);
    xSemaphoreGive(_lock);
}

void IoTIs::set_job_control_received_callback(JobControlReceivedCallback callback)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    _job_control_received_callback = std::move(callback);
    xSemaphoreGive(_lock);
}

bool IoTIs::update_job_status(JobFlatBuffers::JobT &job)
{
    xSemaphoreTake(_lock, portMAX_DELAY);

    if (_mqttClient == nullptr || _state != MqttConnState::CONNECTED || !isConnected)
    {
        xSemaphoreGive(_lock);
        return false;
    }

    const int outbox_before = esp_mqtt_client_get_outbox_size(_mqttClient);
    if (outbox_before >= (int)MQTT_OUTBOX_LIMIT_BYTES)
    {
        ESP_LOGW(TAG, "MQTT outbox full-ish (%d B), dropping job update", outbox_before);
        xSemaphoreGive(_lock);
        return false;
    }

    _builder.Clear();

    auto name = _builder.CreateString(job.name);
    auto job_id = _builder.CreateString(job.job_id);

    JobFlatBuffers::JobBuilder jobBuilder(_builder);
    jobBuilder.add_name(name);
    jobBuilder.add_job_id(job_id);
    jobBuilder.add_status(static_cast<JobFlatBuffers::JobStatusEnum>(job.status));
    jobBuilder.add_current_step(job.current_step);
    jobBuilder.add_total_steps(job.total_steps);
    jobBuilder.add_current_cycle(job.current_cycle);
    jobBuilder.add_total_cycles(job.total_cycles);
    jobBuilder.add_paused(job.paused);
    jobBuilder.add_started_at(job.started_at);
    jobBuilder.add_finished_at(job.finished_at);

    auto jobOffset = jobBuilder.Finish();
    _builder.Finish(jobOffset);

    std::string topic = "devices/" + _accessToken + "/job_from_device";

    int msg_id = esp_mqtt_client_enqueue(
        _mqttClient,
        topic.c_str(),
        reinterpret_cast<const char *>(_builder.GetBufferPointer()),
        _builder.GetSize(),
        1,
        0,
        true);

    if (msg_id < 0)
    {
        ESP_LOGW(TAG, "MQTT job update enqueue failed: msg_id=%d heap=%lu outbox=%d",
                 msg_id,
                 esp_get_free_heap_size(),
                 esp_mqtt_client_get_outbox_size(_mqttClient));
        xSemaphoreGive(_lock);
        return false;
    }

    xSemaphoreGive(_lock);
    return true;
}

bool IoTIs::publish_json(const char *topic_suffix, const char *json)
{
    if (!topic_suffix || !json) return false;

    xSemaphoreTake(_lock, portMAX_DELAY);

    if (_mqttClient == nullptr || _state != MqttConnState::CONNECTED || !isConnected)
    {
        xSemaphoreGive(_lock);
        return false;
    }

    std::string topic = "devices/" + _accessToken + "/" + topic_suffix;
    int len = (int)strlen(json);

    int msg_id = esp_mqtt_client_enqueue(
        _mqttClient,
        topic.c_str(),
        json,
        len,
        1,
        0,
        true);

    xSemaphoreGive(_lock);

    if (msg_id < 0)
    {
        ESP_LOGW(TAG, "publish_json enqueue failed: msg_id=%d", msg_id);
        return false;
    }
    return true;
}

int64_t IoTIs::get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (int64_t)tv.tv_sec * 1000LL + (int64_t)tv.tv_usec / 1000LL;
}

IoTIs iotIs;