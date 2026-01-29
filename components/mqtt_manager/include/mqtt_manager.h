#pragma once

#include <string>
#include <map>
#include <functional>
#include "mqtt_client.h"

class MQTTManager {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
    
    static MQTTManager& getInstance();
    
    // Initialize and connect to MQTT broker
    bool init(const std::string& broker_uri, const std::string& client_id = "");
    
    // Connect with authentication
    bool init(const std::string& broker_uri, const std::string& username, 
              const std::string& password, const std::string& client_id = "");
    
    // Disconnect and cleanup
    void deinit();
    
    // Subscribe to a topic with callback
    bool subscribe(const std::string& topic, int qos, MessageCallback callback);
    
    // Unsubscribe from a topic
    bool unsubscribe(const std::string& topic);
    
    // Publish a message
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false);
    
    // Check connection status
    bool isConnected() const { return m_connected; }
    
private:
    MQTTManager();
    ~MQTTManager();
    MQTTManager(const MQTTManager&) = delete;
    MQTTManager& operator=(const MQTTManager&) = delete;
    
    static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                                   int32_t event_id, void* event_data);
    
    void handleConnected();
    void handleDisconnected();
    void handleData(esp_mqtt_event_handle_t event);
    
    esp_mqtt_client_handle_t m_client;
    bool m_connected;
    std::map<std::string, MessageCallback> m_subscribers;
    std::map<std::string, int> m_qos_map; // Store QoS for each topic
};
