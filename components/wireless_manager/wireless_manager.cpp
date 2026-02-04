#include "wireless_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <cstring>

static const char* TAG = "WirelessManager";

// NVS keys
static const char* NVS_NAMESPACE = "wifi_config";
static const char* NVS_KEY_IP_MODE = "ip_mode";
static const char* NVS_KEY_IP = "ip";
static const char* NVS_KEY_NETMASK = "netmask";
static const char* NVS_KEY_GATEWAY = "gateway";

// Event group bits
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1
#define WIFI_SCAN_DONE_BIT   BIT2

static EventGroupHandle_t s_wifi_event_group = nullptr;

WirelessManager& WirelessManager::getInstance() {
    static WirelessManager instance;
    return instance;
}

WirelessManager::WirelessManager()
    : m_initialized(false)
    , m_status(WifiConnectionStatus::DISCONNECTED)
    , m_current_rssi(0)
    , m_ip_mode(IpConfigMode::DHCP)
    , m_sta_netif(nullptr)
    , m_wifi_event_handler(nullptr)
    , m_ip_event_handler(nullptr)
{
    m_mutex = xSemaphoreCreateMutex();
}

WirelessManager::~WirelessManager() {
    deinit();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

esp_err_t WirelessManager::init() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (m_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        xSemaphoreGive(m_mutex);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Wireless Manager for ESP32-C6...");

    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == nullptr) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP network interface (if not already done)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create default event loop (if not already created)
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create default Wi-Fi STA netif
    m_sta_netif = esp_netif_create_default_wifi_sta();
    if (m_sta_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi STA netif");
        return ESP_FAIL;
    }

    // Initialize Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_handler,
                                              this,
                                              &m_wifi_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Wi-Fi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &ip_event_handler,
                                              this,
                                              &m_ip_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set Wi-Fi mode to station
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start Wi-Fi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "Wireless Manager initialized successfully");
    
    xSemaphoreGive(m_mutex);
    
    // Load and apply saved configuration
    loadConfig();
    
    return ESP_OK;
}

esp_err_t WirelessManager::deinit() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        xSemaphoreGive(m_mutex);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing Wireless Manager...");

    // Stop Wi-Fi
    esp_wifi_stop();

    // Unregister event handlers
    if (m_wifi_event_handler) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, m_wifi_event_handler);
        m_wifi_event_handler = nullptr;
    }
    if (m_ip_event_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, m_ip_event_handler);
        m_ip_event_handler = nullptr;
    }

    // Deinit Wi-Fi
    esp_wifi_deinit();

    // Destroy netif
    if (m_sta_netif) {
        esp_netif_destroy(m_sta_netif);
        m_sta_netif = nullptr;
    }

    // Delete event group
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
    }

    m_initialized = false;
    m_status = WifiConnectionStatus::DISCONNECTED;
    
    ESP_LOGI(TAG, "Wireless Manager deinitialized");
    xSemaphoreGive(m_mutex);
    return ESP_OK;
}

esp_err_t WirelessManager::scan(std::vector<WifiNetworkInfo>& networks, uint16_t max_results, uint32_t scan_time_ms) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreGive(m_mutex);
    
    networks.clear();

    ESP_LOGI(TAG, "Starting Wi-Fi scan...");

    // Configure scan
    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = true;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 120;  // Increased for better discovery
    scan_config.scan_time.active.max = 300;  // Per channel timeout

    // Clear previous scan done bit
    xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);

    // Start scan
    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for scan to complete
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_SCAN_DONE_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(scan_time_ms + 5000));

    if (!(bits & WIFI_SCAN_DONE_BIT)) {
        ESP_LOGE(TAG, "Scan timeout");
        return ESP_ERR_TIMEOUT;
    }

    // Get scan results
    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ap_count == 0) {
        ESP_LOGI(TAG, "No networks found");
        return ESP_OK;
    }

    // Limit results if requested
    if (max_results > 0 && ap_count > max_results) {
        ap_count = max_results;
    }

    // Allocate memory for AP records
    wifi_ap_record_t* ap_records = new wifi_ap_record_t[ap_count];
    if (ap_records == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        return ESP_ERR_NO_MEM;
    }

    // Get AP records
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(ret));
        delete[] ap_records;
        return ret;
    }

    // Convert to our format
    for (uint16_t i = 0; i < ap_count; i++) {
        WifiNetworkInfo info;
        info.ssid = std::string(reinterpret_cast<char*>(ap_records[i].ssid));
        info.rssi = ap_records[i].rssi;
        info.auth_mode = ap_records[i].authmode;
        info.channel = ap_records[i].primary;
        networks.push_back(info);
    }

    delete[] ap_records;

    ESP_LOGI(TAG, "Found %d networks", networks.size());
    return ESP_OK;
}

esp_err_t WirelessManager::scanAsync(WifiScanCallback callback, uint16_t max_results) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreGive(m_mutex);
    
    ESP_LOGI(TAG, "Starting async Wi-Fi scan...");
    
    // Create task parameters (will be freed by task)
    ScanTaskParams* params = new ScanTaskParams();
    if (!params) {
        ESP_LOGE(TAG, "Failed to allocate scan task params");
        return ESP_ERR_NO_MEM;
    }
    
    params->callback = callback;
    params->max_results = max_results;
    
    // Create scan task with high priority to avoid blocking
    BaseType_t ret = xTaskCreate(scanTask, "wifi_scan", 4096, params, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scan task");
        delete params;
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

void WirelessManager::scanTask(void* arg) {
    ScanTaskParams* params = static_cast<ScanTaskParams*>(arg);
    if (!params) {
        vTaskDelete(NULL);
        return;
    }
    
    WirelessManager& mgr = WirelessManager::getInstance();
    std::vector<WifiNetworkInfo> networks;
    
    // Perform blocking scan in this dedicated task
    esp_err_t err = mgr.scan(networks, params->max_results, 5000);
    
    // Call callback with results
    if (params->callback) {
        params->callback(networks, err);
    }
    
    // Clean up and delete task
    delete params;
    vTaskDelete(NULL);
}

esp_err_t WirelessManager::connect(const std::string& ssid, const std::string& password, uint32_t timeout_ms) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (ssid.empty()) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to Wi-Fi network: %s", ssid.c_str());

    // Update status
    m_status = WifiConnectionStatus::CONNECTING;
    m_current_ssid = ssid;
    
    StatusCallback status_cb = m_status_callback;
    xSemaphoreGive(m_mutex);
    
    if (status_cb) {
        status_cb(WifiConnectionStatus::CONNECTING, ssid);
    }

    // Configure Wi-Fi
    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    if (!password.empty()) {
        strncpy(reinterpret_cast<char*>(wifi_config.sta.password), password.c_str(), sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        m_status = WifiConnectionStatus::FAILED;
        status_cb = m_status_callback;
        xSemaphoreGive(m_mutex);
        if (status_cb) {
            status_cb(WifiConnectionStatus::FAILED, "Configuration failed");
        }
        return ret;
    }

    // Clear event bits
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // Connect
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        m_status = WifiConnectionStatus::FAILED;
        status_cb = m_status_callback;
        xSemaphoreGive(m_mutex);
        if (status_cb) {
            status_cb(WifiConnectionStatus::FAILED, "Connection failed");
        }
        return ret;
    }

    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to Wi-Fi network: %s", ssid.c_str());
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network: %s", ssid.c_str());
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        m_status = WifiConnectionStatus::FAILED;
        status_cb = m_status_callback;
        xSemaphoreGive(m_mutex);
        if (status_cb) {
            status_cb(WifiConnectionStatus::FAILED, "Authentication failed");
        }
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Connection timeout");
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        m_status = WifiConnectionStatus::FAILED;
        status_cb = m_status_callback;
        xSemaphoreGive(m_mutex);
        if (status_cb) {
            status_cb(WifiConnectionStatus::FAILED, "Connection timeout");
        }
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t WirelessManager::disconnect() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Disconnecting from Wi-Fi...");

    xSemaphoreGive(m_mutex);
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
        return ret;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_status = WifiConnectionStatus::DISCONNECTED;
    m_current_ssid.clear();
    m_current_ip.clear();
    m_current_rssi = 0;

    StatusCallback status_cb = m_status_callback;
    xSemaphoreGive(m_mutex);
    
    if (status_cb) {
        status_cb(WifiConnectionStatus::DISCONNECTED, "Disconnected");
    }

    return ESP_OK;
}

esp_err_t WirelessManager::setIpConfig(IpConfigMode mode, const StaticIpConfig* config) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    m_ip_mode = mode;

    if (mode == IpConfigMode::STATIC) {
        if (config == nullptr) {
            ESP_LOGE(TAG, "Static IP config cannot be null");
            xSemaphoreGive(m_mutex);
            return ESP_ERR_INVALID_ARG;
        }

        m_static_config = *config;
        esp_netif_t* netif = m_sta_netif;
        xSemaphoreGive(m_mutex);

        // Stop DHCP client
        esp_err_t ret = esp_netif_dhcpc_stop(netif);
        if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            ESP_LOGE(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(ret));
            return ret;
        }

        // Set static IP
        esp_netif_ip_info_t ip_info = {};
        esp_netif_str_to_ip4(config->ip.c_str(), &ip_info.ip);
        esp_netif_str_to_ip4(config->gateway.c_str(), &ip_info.gw);
        esp_netif_str_to_ip4(config->netmask.c_str(), &ip_info.netmask);

        ret = esp_netif_set_ip_info(netif, &ip_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set IP info: %s", esp_err_to_name(ret));
            return ret;
        }

        // Set DNS servers
        if (!config->dns1.empty()) {
            esp_netif_dns_info_t dns_info = {};
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_str_to_ip4(config->dns1.c_str(), &dns_info.ip.u_addr.ip4);
            ret = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set DNS1: %s", esp_err_to_name(ret));
            }
        }

        if (!config->dns2.empty()) {
            esp_netif_dns_info_t dns_info = {};
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_str_to_ip4(config->dns2.c_str(), &dns_info.ip.u_addr.ip4);
            ret = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set DNS2: %s", esp_err_to_name(ret));
            }
        }

        ESP_LOGI(TAG, "Static IP configured: %s", config->ip.c_str());
    } else {
        esp_netif_t* netif = m_sta_netif;
        xSemaphoreGive(m_mutex);
        
        // Start DHCP client
        esp_err_t ret = esp_netif_dhcpc_start(netif);
        if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGE(TAG, "Failed to start DHCP client: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "DHCP enabled");
    }

    return ESP_OK;
}

WifiConnectionStatus WirelessManager::getStatus() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    WifiConnectionStatus status = m_status;
    xSemaphoreGive(m_mutex);
    return status;
}

bool WirelessManager::isConnected() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    bool connected = (m_status == WifiConnectionStatus::CONNECTED);
    xSemaphoreGive(m_mutex);
    return connected;
}

std::string WirelessManager::getCurrentSsid() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string ssid = m_current_ssid;
    xSemaphoreGive(m_mutex);
    return ssid;
}

std::string WirelessManager::getIpAddress() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string ip = m_current_ip;
    xSemaphoreGive(m_mutex);
    return ip;
}

std::string WirelessManager::getNetmask() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string netmask = m_current_netmask;
    xSemaphoreGive(m_mutex);
    return netmask;
}

std::string WirelessManager::getGateway() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string gateway = m_current_gateway;
    xSemaphoreGive(m_mutex);
    return gateway;
}

int8_t WirelessManager::getRssi() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    int8_t rssi = m_current_rssi;
    xSemaphoreGive(m_mutex);
    return rssi;
}

void WirelessManager::setStatusCallback(StatusCallback callback) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_status_callback = callback;
    xSemaphoreGive(m_mutex);
}

void WirelessManager::setIpCallback(IpCallback callback) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_ip_callback = callback;
    xSemaphoreGive(m_mutex);
}

void WirelessManager::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                        int32_t event_id, void* event_data) {
    WirelessManager* manager = static_cast<WirelessManager*>(arg);

    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi started");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi connected");
        wifi_event_sta_connected_t* event = static_cast<wifi_event_sta_connected_t*>(event_data);
        xSemaphoreTake(manager->m_mutex, portMAX_DELAY);
        manager->m_current_ssid = std::string(reinterpret_cast<char*>(event->ssid), event->ssid_len);
        xSemaphoreGive(manager->m_mutex);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected");
        xSemaphoreTake(manager->m_mutex, portMAX_DELAY);
        manager->m_status = WifiConnectionStatus::DISCONNECTED;
        manager->m_current_ip.clear();
        manager->m_current_rssi = 0;
        
        StatusCallback status_cb = manager->m_status_callback;
        xSemaphoreGive(manager->m_mutex);
        
        if (status_cb) {
            status_cb(WifiConnectionStatus::DISCONNECTED, "Disconnected");
        }
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Wi-Fi scan done");
        xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
    }
}

void WirelessManager::ip_event_handler(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data) {
    WirelessManager* manager = static_cast<WirelessManager*>(arg);

    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
        
        char ip_str[16], mask_str[16], gw_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(mask_str, sizeof(mask_str), IPSTR, IP2STR(&event->ip_info.netmask));
        snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&event->ip_info.gw));
        
        xSemaphoreTake(manager->m_mutex, portMAX_DELAY);
        manager->m_current_ip = ip_str;
        manager->m_current_netmask = mask_str;
        manager->m_current_gateway = gw_str;
        manager->m_status = WifiConnectionStatus::CONNECTED;
        
        // Get RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            manager->m_current_rssi = ap_info.rssi;
        }
        
        StatusCallback status_cb = manager->m_status_callback;
        IpCallback ip_cb = manager->m_ip_callback;
        xSemaphoreGive(manager->m_mutex);
        
        ESP_LOGI(TAG, "Got IP: %s", ip_str);
        ESP_LOGI(TAG, "Netmask: %s", mask_str);
        ESP_LOGI(TAG, "Gateway: %s", gw_str);
        
        if (status_cb) {
            status_cb(WifiConnectionStatus::CONNECTED, "Connected");
        }
        
        if (ip_cb) {
            ip_cb(ip_str, mask_str, gw_str);
        }
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t WirelessManager::saveConfig() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    // Save IP mode
    uint8_t mode = (m_ip_mode == IpConfigMode::DHCP) ? 1 : 0;
    err = nvs_set_u8(nvs_handle, NVS_KEY_IP_MODE, mode);
    
    // Save static config
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, NVS_KEY_IP, m_static_config.ip.c_str());
    }
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, NVS_KEY_NETMASK, m_static_config.netmask.c_str());
    }
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, NVS_KEY_GATEWAY, m_static_config.gateway.c_str());
    }
    
    xSemaphoreGive(m_mutex);
    
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi config saved to NVS (Mode: %s)", 
                 mode ? "DHCP" : "Static");
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi config: %s", esp_err_to_name(err));
    }
    
    return err;
}

esp_err_t WirelessManager::loadConfig() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved WiFi config in NVS, using DHCP");
        // Apply default DHCP configuration
        setIpConfig(IpConfigMode::DHCP, nullptr);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read IP mode
    uint8_t mode = 1; // Default to DHCP
    nvs_get_u8(nvs_handle, NVS_KEY_IP_MODE, &mode);
    
    // Helper to read strings
    auto read_str = [&](const char* key, std::string& value) {
        size_t required_size = 0;
        esp_err_t err = nvs_get_str(nvs_handle, key, nullptr, &required_size);
        if (err == ESP_OK && required_size > 0) {
            char* buffer = (char*)malloc(required_size);
            if (buffer) {
                err = nvs_get_str(nvs_handle, key, buffer, &required_size);
                if (err == ESP_OK) {
                    value = buffer;
                }
                free(buffer);
            }
        }
    };
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    m_ip_mode = (mode == 1) ? IpConfigMode::DHCP : IpConfigMode::STATIC;
    read_str(NVS_KEY_IP, m_static_config.ip);
    read_str(NVS_KEY_NETMASK, m_static_config.netmask);
    read_str(NVS_KEY_GATEWAY, m_static_config.gateway);
    
    IpConfigMode loaded_mode = m_ip_mode;
    StaticIpConfig loaded_config = m_static_config;
    
    xSemaphoreGive(m_mutex);
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Loaded WiFi config from NVS (Mode: %s)", 
             (loaded_mode == IpConfigMode::DHCP) ? "DHCP" : "Static");
    if (loaded_mode == IpConfigMode::STATIC) {
        ESP_LOGI(TAG, "  IP: %s", loaded_config.ip.c_str());
        ESP_LOGI(TAG, "  Netmask: %s", loaded_config.netmask.c_str());
        ESP_LOGI(TAG, "  Gateway: %s", loaded_config.gateway.c_str());
    }
    
    // Apply the loaded configuration
    if (loaded_mode == IpConfigMode::DHCP) {
        setIpConfig(IpConfigMode::DHCP, nullptr);
    } else {
        setIpConfig(IpConfigMode::STATIC, &loaded_config);
    }
    
    return ESP_OK;
}

IpConfigMode WirelessManager::getIpMode() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    IpConfigMode mode = m_ip_mode;
    xSemaphoreGive(m_mutex);
    return mode;
}

StaticIpConfig WirelessManager::getStaticConfig() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    StaticIpConfig config = m_static_config;
    xSemaphoreGive(m_mutex);
    return config;
}
