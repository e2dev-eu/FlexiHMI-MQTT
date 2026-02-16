#include "mqtt_manager.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "MQTTManager";

MQTTManager& MQTTManager::getInstance() {
    static MQTTManager instance;
    return instance;
}

MQTTManager::MQTTManager() : m_client(nullptr), m_connected(false) {
    m_mutex = xSemaphoreCreateMutex();
}

MQTTManager::~MQTTManager() {
    deinit();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

bool MQTTManager::init(const std::string& broker_uri, const std::string& client_id) {
    if (m_client) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return true;
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = broker_uri.c_str();
    mqtt_cfg.buffer.size = 512*1024;  // 512KB buffer for large JSON configs with chunking support
    mqtt_cfg.buffer.out_size = 8192;
    
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
    mqtt_cfg.buffer.size = 1024*1024;  // 1MB buffer for large JSON configs with chunking support
    mqtt_cfg.buffer.out_size = 8192;
    
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
        if (m_mutex) {
            xSemaphoreTake(m_mutex, portMAX_DELAY);
        }
        m_subscribers.clear();
        m_handle_to_topic.clear();
        m_qos_map.clear();
        if (m_mutex) {
            xSemaphoreGive(m_mutex);
        }
        ESP_LOGI(TAG, "MQTT client deinitialized");
    }
}

MQTTManager::SubscriptionHandle MQTTManager::subscribe(const std::string& topic, int qos, MessageCallback callback) {
    if (!m_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return 0;  // Invalid handle
    }

    bool first_subscriber = false;
    SubscriptionHandle handle = 0;
    if (m_mutex) {
        xSemaphoreTake(m_mutex, portMAX_DELAY);
    }

    handle = m_next_handle++;
    Subscription sub{handle, callback};
    m_subscribers[topic].push_back(sub);
    m_handle_to_topic[handle] = topic;
    first_subscriber = (m_subscribers[topic].size() == 1);
    if (first_subscriber) {
        m_qos_map[topic] = qos;
    }

    size_t topic_count = m_subscribers.size();
    size_t handle_count = m_handle_to_topic.size();
    if (m_mutex) {
        xSemaphoreGive(m_mutex);
    }

    if (first_subscriber && m_connected) {
        int msg_id = esp_mqtt_client_subscribe(m_client, topic.c_str(), qos);
        if (msg_id == -1) {
            ESP_LOGE(TAG, "Failed to subscribe to %s", topic.c_str());
            return 0;
        }
        ESP_LOGD(TAG, "Subscribed to %s (QoS %d), msg_id=%d", topic.c_str(), qos, msg_id);
    } else if (!first_subscriber) {
        ESP_LOGD(TAG, "Added subscriber to existing topic %s", topic.c_str());
    }

    ESP_LOGD(TAG, "MQTT subs: topics=%u handles=%u", (unsigned)topic_count, (unsigned)handle_count);
    return handle;
}

bool MQTTManager::unsubscribe(SubscriptionHandle handle) {
    if (handle == 0) {
        return false;  // Invalid handle
    }

    std::string topic;
    bool unsubscribe_broker = false;
    size_t topic_count = 0;
    size_t handle_count = 0;

    if (m_mutex) {
        xSemaphoreTake(m_mutex, portMAX_DELAY);
    }

    auto topic_it = m_handle_to_topic.find(handle);
    if (topic_it == m_handle_to_topic.end()) {
        if (m_mutex) {
            xSemaphoreGive(m_mutex);
        }
        ESP_LOGW(TAG, "Unknown subscription handle: %u", handle);
        return false;
    }

    topic = topic_it->second;
    m_handle_to_topic.erase(topic_it);

    auto sub_it = m_subscribers.find(topic);
    if (sub_it != m_subscribers.end()) {
        auto& subs = sub_it->second;
        subs.erase(std::remove_if(subs.begin(), subs.end(),
            [handle](const Subscription& s) { return s.handle == handle; }),
            subs.end());

        ESP_LOGD(TAG, "Unsubscribed handle %u from %s (%d remaining)", 
                 handle, topic.c_str(), subs.size());

        if (subs.empty()) {
            m_subscribers.erase(sub_it);
            m_qos_map.erase(topic);
            unsubscribe_broker = true;
        }
    }

    topic_count = m_subscribers.size();
    handle_count = m_handle_to_topic.size();
    if (m_mutex) {
        xSemaphoreGive(m_mutex);
    }

    if (unsubscribe_broker && m_connected && m_client) {
        int msg_id = esp_mqtt_client_unsubscribe(m_client, topic.c_str());
        if (msg_id != -1) {
            ESP_LOGD(TAG, "Unsubscribed from broker topic %s, msg_id=%d", topic.c_str(), msg_id);
        }
    }

    ESP_LOGD(TAG, "MQTT subs: topics=%u handles=%u", (unsigned)topic_count, (unsigned)handle_count);
    return true;
}

bool MQTTManager::unsubscribeTopic(const std::string& topic) {
    auto sub_it = m_subscribers.find(topic);
    if (sub_it == m_subscribers.end()) {
        return false;
    }
    
    // Remove all handle mappings for this topic
    for (const auto& sub : sub_it->second) {
        m_handle_to_topic.erase(sub.handle);
    }
    
    m_subscribers.erase(sub_it);
    m_qos_map.erase(topic);
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
        ESP_LOGD(TAG, "Unsubscribed from %s, msg_id=%d", topic.c_str(), msg_id);
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
        m_messages_sent++;
    if (m_status_callback) {
        m_status_callback(m_connected, m_messages_received, m_messages_sent);
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
    
    // Notify status callback
    if (m_status_callback) {
        m_status_callback(m_connected, m_messages_received, m_messages_sent);
    }
    
    // Resubscribe to all topics
    for (const auto& sub : m_subscribers) {
        const std::string& topic = sub.first;
        int qos = m_qos_map[topic];
        int msg_id = esp_mqtt_client_subscribe(m_client, topic.c_str(), qos);
        ESP_LOGD(TAG, "Resubscribed to %s (QoS %d), msg_id=%d", topic.c_str(), qos, msg_id);
    }
}

void MQTTManager::handleDisconnected() {
    ESP_LOGW(TAG, "MQTT disconnected");
    m_connected = false;
    
    // Notify status callback
    if (m_status_callback) {
        m_status_callback(m_connected, m_messages_received, m_messages_sent);
    }
}

void MQTTManager::handleData(esp_mqtt_event_handle_t event) {
    std::string topic(event->topic, event->topic_len);
    
    // Handle chunked messages
    if (event->total_data_len > event->data_len) {
        // Multi-chunk message
        ESP_LOGI(TAG, "Chunked message on %s: offset=%d, chunk_len=%d, total=%d",
                 topic.c_str(), event->current_data_offset, event->data_len, event->total_data_len);
        
        // Initialize buffer on first chunk
        if (event->current_data_offset == 0) {
            m_chunk_buffer.clear();
            m_chunk_buffer.reserve(event->total_data_len);
            m_chunk_topic = topic;
        }

        if (!topic.empty()) {
            m_chunk_topic = topic;
        }
        
        std::string payload;
        std::vector<Subscription> subs_copy;

        if (m_mutex) {
            xSemaphoreTake(m_mutex, portMAX_DELAY);
        }

        // Accumulate chunk
        m_chunk_buffer.append(event->data, event->data_len);

        std::string full_topic;
        if (m_chunk_buffer.size() >= static_cast<size_t>(event->total_data_len)) {
            full_topic = m_chunk_topic.empty() ? topic : m_chunk_topic;
            ESP_LOGI(TAG, "Complete message received on %s: %d bytes", full_topic.c_str(), m_chunk_buffer.size());
            payload = m_chunk_buffer;
            m_chunk_buffer.clear();
            m_chunk_topic.clear();

            auto it = m_subscribers.find(full_topic);
            if (it != m_subscribers.end()) {
                subs_copy = it->second;
            }
        }

        if (m_mutex) {
            xSemaphoreGive(m_mutex);
        }

        if (!payload.empty()) {
            m_messages_received++;
            if (m_status_callback) {
                m_status_callback(m_connected, m_messages_received, m_messages_sent);
            }

            for (auto& sub : subs_copy) {
                sub.callback(full_topic, payload);
            }
        }
    } else {
        // Single chunk message
        std::string payload(event->data, event->data_len);
        ESP_LOGD(TAG, "Received on %s: %d bytes", topic.c_str(), payload.size());
        
        m_messages_received++;
        if (m_status_callback) {
            m_status_callback(m_connected, m_messages_received, m_messages_sent);
        }
        
        std::vector<Subscription> subs_copy;
        if (m_mutex) {
            xSemaphoreTake(m_mutex, portMAX_DELAY);
        }
        auto it = m_subscribers.find(topic);
        if (it != m_subscribers.end()) {
            subs_copy = it->second;
        }
        if (m_mutex) {
            xSemaphoreGive(m_mutex);
        }

        if (!subs_copy.empty()) {
            ESP_LOGD(TAG, "Delivering to %d subscriber(s)", subs_copy.size());
            for (auto& sub : subs_copy) {
                sub.callback(topic, payload);
            }
        } else {
            ESP_LOGW(TAG, "No subscriber for topic: %s", topic.c_str());
        }
    }
}
