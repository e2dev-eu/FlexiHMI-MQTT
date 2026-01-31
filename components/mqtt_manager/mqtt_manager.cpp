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
        m_subscribers.clear();
        m_qos_map.clear();
        ESP_LOGI(TAG, "MQTT client deinitialized");
    }
}

MQTTManager::SubscriptionHandle MQTTManager::subscribe(const std::string& topic, int qos, MessageCallback callback) {
    if (!m_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return 0;  // Invalid handle
    }
    
    // Generate unique handle
    SubscriptionHandle handle = m_next_handle++;
    
    // Store subscription with handle
    Subscription sub{handle, callback};
    m_subscribers[topic].push_back(sub);
    m_handle_to_topic[handle] = topic;
    
    // Only subscribe to broker once per topic
    bool first_subscriber = (m_subscribers[topic].size() == 1);
    if (first_subscriber) {
        m_qos_map[topic] = qos;
        
        if (m_connected) {
            int msg_id = esp_mqtt_client_subscribe(m_client, topic.c_str(), qos);
            if (msg_id == -1) {
                ESP_LOGE(TAG, "Failed to subscribe to %s", topic.c_str());
                return 0;
            }
            ESP_LOGI(TAG, "Subscribed to %s (QoS %d), msg_id=%d", topic.c_str(), qos, msg_id);
        }
    } else {
        ESP_LOGI(TAG, "Added subscriber to existing topic %s (total: %d)", topic.c_str(), m_subscribers[topic].size());
    }
    
    return handle;
}

bool MQTTManager::unsubscribe(SubscriptionHandle handle) {
    if (handle == 0) {
        return false;  // Invalid handle
    }
    
    // Find topic for this handle
    auto topic_it = m_handle_to_topic.find(handle);
    if (topic_it == m_handle_to_topic.end()) {
        ESP_LOGW(TAG, "Unknown subscription handle: %u", handle);
        return false;
    }
    
    std::string topic = topic_it->second;
    m_handle_to_topic.erase(topic_it);
    
    // Remove subscription from topic's callback list
    auto sub_it = m_subscribers.find(topic);
    if (sub_it != m_subscribers.end()) {
        auto& subs = sub_it->second;
        subs.erase(std::remove_if(subs.begin(), subs.end(),
            [handle](const Subscription& s) { return s.handle == handle; }),
            subs.end());
        
        ESP_LOGI(TAG, "Unsubscribed handle %u from %s (%d remaining)", 
                 handle, topic.c_str(), subs.size());
        
        // If no more subscribers for this topic, unsubscribe from broker
        if (subs.empty()) {
            m_subscribers.erase(sub_it);
            m_qos_map.erase(topic);
            
            if (m_connected && m_client) {
                int msg_id = esp_mqtt_client_unsubscribe(m_client, topic.c_str());
                if (msg_id != -1) {
                    ESP_LOGI(TAG, "Unsubscribed from broker topic %s, msg_id=%d", topic.c_str(), msg_id);
                }
            }
        }
    }
    
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
    
    // Handle chunked messages
    if (event->total_data_len > event->data_len) {
        // Multi-chunk message
        ESP_LOGI(TAG, "Chunked message on %s: offset=%d, chunk_len=%d, total=%d",
                 topic.c_str(), event->current_data_offset, event->data_len, event->total_data_len);
        
        // Initialize buffer on first chunk
        if (event->current_data_offset == 0) {
            m_chunk_buffer.clear();
            m_chunk_buffer.reserve(event->total_data_len);
        }
        
        // Accumulate chunk
        m_chunk_buffer.append(event->data, event->data_len);
        
        // Check if we have all chunks
        if (m_chunk_buffer.size() >= static_cast<size_t>(event->total_data_len)) {
            ESP_LOGI(TAG, "Complete message received on %s: %d bytes", topic.c_str(), m_chunk_buffer.size());
            
            // Find matching subscribers and deliver complete message to all
            auto it = m_subscribers.find(topic);
            if (it != m_subscribers.end()) {
                for (auto& sub : it->second) {
                    sub.callback(topic, m_chunk_buffer);
                }
            }
            
            m_chunk_buffer.clear();
        }
    } else {
        // Single chunk message
        std::string payload(event->data, event->data_len);
        ESP_LOGD(TAG, "Received on %s: %d bytes", topic.c_str(), payload.size());
        
        // Find matching subscribers and invoke all callbacks
        auto it = m_subscribers.find(topic);
        if (it != m_subscribers.end()) {
            ESP_LOGD(TAG, "Delivering to %d subscriber(s)", it->second.size());
            for (auto& sub : it->second) {
                sub.callback(topic, payload);
            }
        } else {
            ESP_LOGW(TAG, "No subscriber for topic: %s", topic.c_str());
        }
    }
}
