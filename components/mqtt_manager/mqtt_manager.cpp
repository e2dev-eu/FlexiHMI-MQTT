#include "mqtt_manager.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "MQTTManager";

MQTTManager& MQTTManager::getInstance() {
    static MQTTManager instance;
    return instance;
}

MQTTManager::MQTTManager() : m_client(nullptr), m_connected(false) {
}

MQTTManager::~MQTTManager() {
    deinit();
}

bool MQTTManager::init(const std::string& broker_uri, const std::string& client_id) {
    if (m_client) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return true;
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = broker_uri.c_str();
    mqtt_cfg.buffer.size = 8192;  // Increase buffer size for large JSON configs
    mqtt_cfg.buffer.out_size = 2048;
    
    if (!client_id.empty()) {
        mqtt_cfg.credentials.client_id = client_id.c_str();
    }
    
    m_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!m_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return false;
    }
    
    esp_mqtt_client_register_event(m_client, MQTT_EVENT_ANY, mqtt_event_handler, this);
    
    esp_err_t err = esp_mqtt_client_start(m_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "MQTT client started, connecting to %s", broker_uri.c_str());
    return true;
}

bool MQTTManager::init(const std::string& broker_uri, const std::string& username, 
                       const std::string& password, const std::string& client_id) {
    if (m_client) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return true;
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = broker_uri.c_str();
    mqtt_cfg.credentials.username = username.c_str();
    mqtt_cfg.credentials.authentication.password = password.c_str();
    mqtt_cfg.buffer.size = 8192;  // Increase buffer size for large JSON configs
    mqtt_cfg.buffer.out_size = 2048;
    
    if (!client_id.empty()) {
        mqtt_cfg.credentials.client_id = client_id.c_str();
    }
    
    m_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!m_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return false;
    }
    
    esp_mqtt_client_register_event(m_client, MQTT_EVENT_ANY, mqtt_event_handler, this);
    
    esp_err_t err = esp_mqtt_client_start(m_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "MQTT client started with auth, connecting to %s", broker_uri.c_str());
    return true;
}

void MQTTManager::deinit() {
    if (m_client) {
        esp_mqtt_client_stop(m_client);
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
        m_connected = false;
        m_subscribers.clear();
        m_qos_map.clear();
        ESP_LOGI(TAG, "MQTT client deinitialized");
    }
}

bool MQTTManager::subscribe(const std::string& topic, int qos, MessageCallback callback) {
    if (!m_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return false;
    }
    
    m_subscribers[topic] = callback;
    m_qos_map[topic] = qos;
    
    if (m_connected) {
        int msg_id = esp_mqtt_client_subscribe(m_client, topic.c_str(), qos);
        if (msg_id == -1) {
            ESP_LOGE(TAG, "Failed to subscribe to %s", topic.c_str());
            return false;
        }
        ESP_LOGI(TAG, "Subscribed to %s (QoS %d), msg_id=%d", topic.c_str(), qos, msg_id);
    }
    
    return true;
}

bool MQTTManager::unsubscribe(const std::string& topic) {
    if (!m_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return false;
    }
    
    m_subscribers.erase(topic);
    m_qos_map.erase(topic);
    
    if (m_connected) {
        int msg_id = esp_mqtt_client_unsubscribe(m_client, topic.c_str());
        if (msg_id == -1) {
            ESP_LOGE(TAG, "Failed to unsubscribe from %s", topic.c_str());
            return false;
        }
        ESP_LOGI(TAG, "Unsubscribed from %s, msg_id=%d", topic.c_str(), msg_id);
    }
    
    return true;
}

bool MQTTManager::publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
    if (!m_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return false;
    }
    
    if (!m_connected) {
        ESP_LOGW(TAG, "MQTT client not connected, cannot publish");
        return false;
    }
    
    int msg_id = esp_mqtt_client_publish(m_client, topic.c_str(), payload.c_str(), 
                                         payload.length(), qos, retain ? 1 : 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to publish to %s", topic.c_str());
        return false;
    }
    
    ESP_LOGD(TAG, "Published to %s: %s (msg_id=%d)", topic.c_str(), payload.c_str(), msg_id);
    return true;
}

void MQTTManager::mqtt_event_handler(void* handler_args, esp_event_base_t base,
                                     int32_t event_id, void* event_data) {
    MQTTManager* manager = static_cast<MQTTManager*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            manager->handleConnected();
            break;
        case MQTT_EVENT_DISCONNECTED:
            manager->handleDisconnected();
            break;
        case MQTT_EVENT_DATA:
            manager->handleData(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

void MQTTManager::handleConnected() {
    ESP_LOGI(TAG, "MQTT connected");
    m_connected = true;
    
    // Resubscribe to all topics
    for (const auto& sub : m_subscribers) {
        const std::string& topic = sub.first;
        int qos = m_qos_map[topic];
        int msg_id = esp_mqtt_client_subscribe(m_client, topic.c_str(), qos);
        ESP_LOGI(TAG, "Resubscribed to %s (QoS %d), msg_id=%d", topic.c_str(), qos, msg_id);
    }
}

void MQTTManager::handleDisconnected() {
    ESP_LOGW(TAG, "MQTT disconnected");
    m_connected = false;
}

void MQTTManager::handleData(esp_mqtt_event_handle_t event) {
    std::string topic(event->topic, event->topic_len);
    std::string payload(event->data, event->data_len);
    
    ESP_LOGD(TAG, "Received on %s: %s", topic.c_str(), payload.c_str());
    
    // Find matching subscriber
    auto it = m_subscribers.find(topic);
    if (it != m_subscribers.end()) {
        it->second(topic, payload);
    } else {
        ESP_LOGW(TAG, "No subscriber for topic: %s", topic.c_str());
    }
}
