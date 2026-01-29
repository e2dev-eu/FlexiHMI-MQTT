#ifndef ETHERNET_INIT_H
#define ETHERNET_INIT_H

#include "esp_err.h"
#include "esp_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Ethernet with IP101GR PHY on RMII interface
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ethernet_init(void);

/**
 * @brief Get the Ethernet handle
 * 
 * @return esp_eth_handle_t Ethernet handle or NULL
 */
esp_eth_handle_t ethernet_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_INIT_H
