#pragma once
#ifndef LIB_OTA_OTA_H_
#define LIB_OTA_OTA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "esp_err.h"

/**
 * @brief Perform OTA update by checking for new firmware version from backend server. Will restart the device if update is successful. 
 *        If the device is already on the latest version, no action is taken.
 * 
 * @param accessToken Device access token used for authentication with the backend server.
 * @return esp_err_t ESP_OK if update is successful or no update is needed, error code otherwise.
 */
esp_err_t perform_ota_update(const char* base_url, const char* accessToken);

/**
 * @brief Update the backend server with the current firmware version of the device.
 * 
 * @param accessToken Device access token used for authentication with the backend server.
 * @param versionNumber Current firmware version number to report.
 * @return esp_err_t ESP_OK if the version update is successful, error code otherwise.
 */
esp_err_t updateFirmwareVersion(const char* base_url, const char* accessToken);

/**
 * @brief Mark the currently running application as valid to cancel any pending rollback.
 * 
 * @return esp_err_t 
 */
esp_err_t mark_app_valid_cancel_rollback();


#ifdef __cplusplus
}
#endif

#endif // LIB_OTA_OTA_H_