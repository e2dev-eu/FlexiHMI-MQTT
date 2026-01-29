#include "ethernet_init.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "status_info_ui.h"

static const char *TAG = "ethernet";

// ESP32-P4-Function-EV-Board Ethernet configuration
// IP101GR PHY with RMII interface
#define ETH_PHY_ADDR            1
#define ETH_PHY_RST_GPIO        GPIO_NUM_51  // PHY reset GPIO

// RMII GPIO configuration for ESP32-P4
#define ETH_RMII_TX_EN          GPIO_NUM_49
#define ETH_RMII_TXD0           GPIO_NUM_34
#define ETH_RMII_TXD1           GPIO_NUM_35
#define ETH_RMII_RXD0           GPIO_NUM_36
#define ETH_RMII_RXD1           GPIO_NUM_37
#define ETH_RMII_CRS_DV         GPIO_NUM_38
#define ETH_RMII_CLK_MODE       EMAC_CLK_EXT_IN
#define ETH_RMII_CLK_GPIO       GPIO_NUM_50

// MDC/MDIO pins
#define ETH_MDC_GPIO            GPIO_NUM_31
#define ETH_MDIO_GPIO           GPIO_NUM_52

static esp_eth_handle_t eth_handle = NULL;
static esp_netif_t *eth_netif = NULL;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    
    // Update status info UI
    char ip_str[16], mask_str[16], gw_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info->ip));
    snprintf(mask_str, sizeof(mask_str), IPSTR, IP2STR(&ip_info->netmask));
    snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&ip_info->gw));
    status_info_update_network(ip_str, mask_str, gw_str);
}

esp_err_t ethernet_init(void)
{
    ESP_LOGI(TAG, "Initializing Ethernet (IP101GR on RMII)...");

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance of esp-netif for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&netif_cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    
    phy_config.phy_addr = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;
    phy_config.reset_timeout_ms = 50;  // Increase reset timeout

    // Install GPIO ISR handler to be able to service SPI Eth modules interrupts
    gpio_install_isr_service(0);

    // Init EMAC for RMII mode
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdc_num = ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = ETH_MDIO_GPIO;
    esp32_emac_config.clock_config.rmii.clock_mode = ETH_RMII_CLK_MODE;
    esp32_emac_config.clock_config.rmii.clock_gpio = ETH_RMII_CLK_GPIO;
    
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

    // Install IP101 PHY driver
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    // Install Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Register user defined event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Start Ethernet driver
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "Ethernet initialization complete");
    
    return ESP_OK;
}

esp_eth_handle_t ethernet_get_handle(void)
{
    return eth_handle;
}
