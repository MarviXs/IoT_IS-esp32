#pragma once
#ifndef LIB_OTA_OTA_H_
#define LIB_OTA_OTA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"

/**
 * @brief Perform OTA update by checking for new firmware version from backend server. Will restart the device if update is successful. 
 *        If the device is already on the latest version, no action is taken.
 * 
 * @param accessToken Device access token used for authentication with the backend server.
 * @return esp_err_t ESP_OK if update is successful or no update is needed, error code otherwise.
 */
esp_err_t perform_ota_update(const char* accessToken);

/**
 * @brief Update the backend server with the current firmware version of the device.
 * 
 * @param accessToken Device access token used for authentication with the backend server.
 * @param versionNumber Current firmware version number to report.
 * @return esp_err_t ESP_OK if the version update is successful, error code otherwise.
 */
esp_err_t updateFirmwareVersion(const char* accessToken, const char* versionNumber);

#ifdef __cplusplus
}
#endif

#endif // LIB_OTA_OTA_H_