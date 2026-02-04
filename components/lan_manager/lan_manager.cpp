#include "lan_manager.h"
#include "ethernet_init.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>

static const char* TAG = "LanManager";

// NVS keys
static const char* NVS_NAMESPACE = "lan_config";
static const char* NVS_KEY_IP_MODE = "ip_mode";
static const char* NVS_KEY_IP = "ip";
static const char* NVS_KEY_NETMASK = "netmask";
static const char* NVS_KEY_GATEWAY = "gateway";

LanManager& LanManager::getInstance() {
    static LanManager instance;
    return instance;
}

LanManager::LanManager()
    : m_initialized(false)
    , m_using_existing_eth(false)
    , m_status(EthConnectionStatus::DISCONNECTED)
    , m_ip_mode(EthIpConfigMode::DHCP)
    , m_eth_netif(nullptr)
    , m_eth_handle(nullptr)
    , m_eth_event_handler(nullptr)
    , m_ip_event_handler(nullptr)
{
    m_mutex = xSemaphoreCreateMutex();
}

LanManager::~LanManager() {
    deinit();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

esp_err_t LanManager::init() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (m_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        xSemaphoreGive(m_mutex);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing LAN Manager...");

    // Check if ethernet is already initialized via ethernet_init()
    m_eth_handle = ethernet_get_handle();
    if (m_eth_handle != nullptr) {
        ESP_LOGI(TAG, "Using existing Ethernet initialization");
        m_using_existing_eth = true;
        
        // Get the default Ethernet netif
        esp_netif_t* netif = nullptr;
        esp_netif_t* temp_netif = esp_netif_next(nullptr);
        while (temp_netif != nullptr) {
            if (esp_netif_get_desc(temp_netif) && 
                strcmp(esp_netif_get_desc(temp_netif), "eth") == 0) {
                netif = temp_netif;
                break;
            }
            temp_netif = esp_netif_next(temp_netif);
        }
        
        if (netif == nullptr) {
            ESP_LOGE(TAG, "Failed to find Ethernet netif");
            return ESP_FAIL;
        }
        m_eth_netif = netif;
    } else {
        ESP_LOGI(TAG, "Ethernet not initialized, using ethernet_init()");
        
        // Call the existing ethernet_init function
        esp_err_t ret = ethernet_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize Ethernet: %s", esp_err_to_name(ret));
            return ret;
        }
        
        m_eth_handle = ethernet_get_handle();
        if (m_eth_handle == nullptr) {
            ESP_LOGE(TAG, "Failed to get Ethernet handle after init");
            return ESP_FAIL;
        }
        
        // Get the Ethernet netif
        esp_netif_t* netif = nullptr;
        esp_netif_t* temp_netif = esp_netif_next(nullptr);
        while (temp_netif != nullptr) {
            if (esp_netif_get_desc(temp_netif) && 
                strcmp(esp_netif_get_desc(temp_netif), "eth") == 0) {
                netif = temp_netif;
                break;
            }
            temp_netif = esp_netif_next(temp_netif);
        }
        
        if (netif == nullptr) {
            ESP_LOGE(TAG, "Failed to find Ethernet netif");
            return ESP_FAIL;
        }
        m_eth_netif = netif;
        m_using_existing_eth = true;
    }

    // Register our own event handlers for status tracking
    esp_err_t ret = esp_event_handler_instance_register(ETH_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &eth_event_handler,
                                                        this,
                                                        &m_eth_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Ethernet event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_ETH_GOT_IP,
                                              &ip_event_handler,
                                              this,
                                              &m_ip_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, m_eth_event_handler);
        return ret;
    }

    // Get MAC address
    uint8_t mac[6];
    if (esp_eth_ioctl(m_eth_handle, ETH_CMD_G_MAC_ADDR, mac) == ESP_OK) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        m_mac_address = mac_str;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "LAN Manager initialized successfully (MAC: %s)", m_mac_address.c_str());
    
    xSemaphoreGive(m_mutex);
    
    // Load and apply saved configuration
    loadConfig();
    
    return ESP_OK;
}

esp_err_t LanManager::deinit() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        xSemaphoreGive(m_mutex);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing LAN Manager...");

    // Unregister event handlers
    if (m_eth_event_handler) {
        esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, m_eth_event_handler);
        m_eth_event_handler = nullptr;
    }
    if (m_ip_event_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, m_ip_event_handler);
        m_ip_event_handler = nullptr;
    }

    // Note: We don't stop/destroy Ethernet if it was initialized by ethernet_init()
    // as other parts of the system may be using it

    m_initialized = false;
    m_status = EthConnectionStatus::DISCONNECTED;
    m_eth_netif = nullptr;
    m_eth_handle = nullptr;
    
    ESP_LOGI(TAG, "LAN Manager deinitialized");
    xSemaphoreGive(m_mutex);
    return ESP_OK;
}

esp_err_t LanManager::setIpConfig(EthIpConfigMode mode, const EthStaticIpConfig* config) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        xSemaphoreGive(m_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    m_ip_mode = mode;

    if (mode == EthIpConfigMode::STATIC) {
        if (config == nullptr) {
            ESP_LOGE(TAG, "Static IP config cannot be null");
            xSemaphoreGive(m_mutex);
            return ESP_ERR_INVALID_ARG;
        }

        m_static_config = *config;

        // Stop DHCP client
        esp_err_t ret = esp_netif_dhcpc_stop(m_eth_netif);
        if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            ESP_LOGE(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(ret));
            xSemaphoreGive(m_mutex);
            return ret;
        }

        // Set static IP
        esp_netif_ip_info_t ip_info = {};
        esp_netif_str_to_ip4(config->ip.c_str(), &ip_info.ip);
        esp_netif_str_to_ip4(config->gateway.c_str(), &ip_info.gw);
        esp_netif_str_to_ip4(config->netmask.c_str(), &ip_info.netmask);

        ret = esp_netif_set_ip_info(m_eth_netif, &ip_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set IP info: %s", esp_err_to_name(ret));
            xSemaphoreGive(m_mutex);
            return ret;
        }

        // Update internal state
        m_current_ip = config->ip;
        m_current_netmask = config->netmask;
        m_current_gateway = config->gateway;
        
        // Static IP is configured, so we're effectively connected
        m_status = EthConnectionStatus::CONNECTED;

        // Set DNS servers
        if (!config->dns1.empty()) {
            esp_netif_dns_info_t dns_info = {};
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_str_to_ip4(config->dns1.c_str(), &dns_info.ip.u_addr.ip4);
            ret = esp_netif_set_dns_info(m_eth_netif, ESP_NETIF_DNS_MAIN, &dns_info);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set DNS1: %s", esp_err_to_name(ret));
            }
        }

        if (!config->dns2.empty()) {
            esp_netif_dns_info_t dns_info = {};
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_str_to_ip4(config->dns2.c_str(), &dns_info.ip.u_addr.ip4);
            ret = esp_netif_set_dns_info(m_eth_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set DNS2: %s", esp_err_to_name(ret));
            }
        }

        ESP_LOGI(TAG, "Static IP configured: %s", config->ip.c_str());
        
        // Get callbacks before releasing mutex
        StatusCallback status_cb = m_status_callback;
        IpCallback ip_cb = m_ip_callback;
        std::string ip_str = m_current_ip;
        std::string netmask_str = m_current_netmask;
        std::string gateway_str = m_current_gateway;
        
        xSemaphoreGive(m_mutex);
        
        // Call callbacks outside mutex
        if (status_cb) {
            status_cb(EthConnectionStatus::CONNECTED, "Connected");
        }
        if (ip_cb) {
            ip_cb(ip_str, netmask_str, gateway_str);
        }
        
        return ESP_OK;
    } else {
        // Start DHCP client
        esp_err_t ret = esp_netif_dhcpc_start(m_eth_netif);
        if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGE(TAG, "Failed to start DHCP client: %s", esp_err_to_name(ret));
            xSemaphoreGive(m_mutex);
            return ret;
        }

        ESP_LOGI(TAG, "DHCP enabled");
        xSemaphoreGive(m_mutex);
    }

    return ESP_OK;
}

EthConnectionStatus LanManager::getStatus() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    EthConnectionStatus status = m_status;
    xSemaphoreGive(m_mutex);
    return status;
}

bool LanManager::isConnected() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    bool connected = (m_status == EthConnectionStatus::CONNECTED);
    xSemaphoreGive(m_mutex);
    return connected;
}

std::string LanManager::getIpAddress() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string ip = m_current_ip;
    xSemaphoreGive(m_mutex);
    return ip;
}

std::string LanManager::getNetmask() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string netmask = m_current_netmask;
    xSemaphoreGive(m_mutex);
    return netmask;
}

std::string LanManager::getGateway() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string gateway = m_current_gateway;
    xSemaphoreGive(m_mutex);
    return gateway;
}

std::string LanManager::getMacAddress() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string mac = m_mac_address;
    xSemaphoreGive(m_mutex);
    return mac;
}

void LanManager::setStatusCallback(StatusCallback callback) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_status_callback = callback;
    xSemaphoreGive(m_mutex);
}

void LanManager::setIpCallback(IpCallback callback) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_ip_callback = callback;
    xSemaphoreGive(m_mutex);
}

void LanManager::eth_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
    LanManager* manager = static_cast<LanManager*>(arg);
    xSemaphoreTake(manager->m_mutex, portMAX_DELAY);

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "Ethernet Link Up");
            manager->m_status = EthConnectionStatus::LINK_UP;
            
            StatusCallback status_cb = manager->m_status_callback;
            EthConnectionStatus status = manager->m_status;
            xSemaphoreGive(manager->m_mutex);
            
            if (status_cb) {
                status_cb(status, "Link up");
            }
            
            uint8_t mac_addr[6];
            esp_eth_handle_t eth_handle = *(esp_eth_handle_t*)event_data;
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            return;
        }
        
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            manager->m_status = EthConnectionStatus::LINK_DOWN;
            manager->m_current_ip.clear();
            manager->m_current_netmask.clear();
            manager->m_current_gateway.clear();
            
            {
                StatusCallback status_cb = manager->m_status_callback;
                EthConnectionStatus status = manager->m_status;
                xSemaphoreGive(manager->m_mutex);
                
                if (status_cb) {
                    status_cb(status, "Link down");
                }
            }
            return;
            
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            manager->m_status = EthConnectionStatus::DISCONNECTED;
            
            {
                StatusCallback status_cb = manager->m_status_callback;
                EthConnectionStatus status = manager->m_status;
                xSemaphoreGive(manager->m_mutex);
                
                if (status_cb) {
                    status_cb(status, "Started");
                }
            }
            return;
            
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            manager->m_status = EthConnectionStatus::DISCONNECTED;
            
            {
                StatusCallback status_cb = manager->m_status_callback;
                EthConnectionStatus status = manager->m_status;
                xSemaphoreGive(manager->m_mutex);
                
                if (status_cb) {
                    status_cb(status, "Stopped");
                }
            }
            return;
            
        default:
            xSemaphoreGive(manager->m_mutex);
            return;
    }
}

void LanManager::ip_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data) {
    LanManager* manager = static_cast<LanManager*>(arg);

    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
        const esp_netif_ip_info_t* ip_info = &event->ip_info;

        char ip_str[16], mask_str[16], gw_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info->ip));
        snprintf(mask_str, sizeof(mask_str), IPSTR, IP2STR(&ip_info->netmask));
        snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&ip_info->gw));

        xSemaphoreTake(manager->m_mutex, portMAX_DELAY);
        manager->m_current_ip = ip_str;
        manager->m_current_netmask = mask_str;
        manager->m_current_gateway = gw_str;
        manager->m_status = EthConnectionStatus::CONNECTED;

        StatusCallback status_cb = manager->m_status_callback;
        IpCallback ip_cb = manager->m_ip_callback;
        EthConnectionStatus status = manager->m_status;
        xSemaphoreGive(manager->m_mutex);

        ESP_LOGI(TAG, "Ethernet Got IP Address");
        ESP_LOGI(TAG, "IP: %s", ip_str);
        ESP_LOGI(TAG, "Netmask: %s", mask_str);
        ESP_LOGI(TAG, "Gateway: %s", gw_str);

        if (status_cb) {
            status_cb(status, "Connected");
        }

        if (ip_cb) {
            ip_cb(ip_str, mask_str, gw_str);
        }
    }
}

esp_err_t LanManager::saveConfig() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    // Save IP mode
    uint8_t mode = (m_ip_mode == EthIpConfigMode::DHCP) ? 1 : 0;
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
        ESP_LOGI(TAG, "LAN config saved to NVS (Mode: %s)", 
                 mode ? "DHCP" : "Static");
    } else {
        ESP_LOGE(TAG, "Failed to save LAN config: %s", esp_err_to_name(err));
    }
    
    return err;
}

esp_err_t LanManager::loadConfig() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved LAN config in NVS, using DHCP");
        // Apply default DHCP configuration
        setIpConfig(EthIpConfigMode::DHCP, nullptr);
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
    
    m_ip_mode = (mode == 1) ? EthIpConfigMode::DHCP : EthIpConfigMode::STATIC;
    read_str(NVS_KEY_IP, m_static_config.ip);
    read_str(NVS_KEY_NETMASK, m_static_config.netmask);
    read_str(NVS_KEY_GATEWAY, m_static_config.gateway);
    
    EthIpConfigMode loaded_mode = m_ip_mode;
    EthStaticIpConfig loaded_config = m_static_config;
    
    xSemaphoreGive(m_mutex);
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Loaded LAN config from NVS (Mode: %s)", 
             (loaded_mode == EthIpConfigMode::DHCP) ? "DHCP" : "Static");
    if (loaded_mode == EthIpConfigMode::STATIC) {
        ESP_LOGI(TAG, "  IP: %s", loaded_config.ip.c_str());
        ESP_LOGI(TAG, "  Netmask: %s", loaded_config.netmask.c_str());
        ESP_LOGI(TAG, "  Gateway: %s", loaded_config.gateway.c_str());
    }
    
    // Apply the loaded configuration
    if (loaded_mode == EthIpConfigMode::DHCP) {
        setIpConfig(EthIpConfigMode::DHCP, nullptr);
    } else {
        setIpConfig(EthIpConfigMode::STATIC, &loaded_config);
    }
    
    return ESP_OK;
}

EthIpConfigMode LanManager::getIpMode() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    EthIpConfigMode mode = m_ip_mode;
    xSemaphoreGive(m_mutex);
    return mode;
}

EthStaticIpConfig LanManager::getStaticConfig() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    EthStaticIpConfig config = m_static_config;
    xSemaphoreGive(m_mutex);
    return config;
}
