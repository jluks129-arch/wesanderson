/**
 * @file web_server.h
 *
 * @brief Minimal HTTP server — call web_server_start() after WiFi is up.
 *
 * COPYRIGHT NOTICE: (c) 2025 Byte Lab Grupa d.o.o.
 * All rights reserved.
 */

#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------- INCLUDES ----------------------------------
#include "esp_err.h"

//---------------------- PUBLIC FUNCTION PROTOTYPES --------------------------

/**
 * @brief Start the HTTP server and register URI handlers.
 *
 * @return ESP_OK on success, or an error code.
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop the HTTP server.
 */
void web_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __WEB_SERVER_H__ */
