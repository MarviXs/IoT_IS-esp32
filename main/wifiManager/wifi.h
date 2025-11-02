#ifndef WIFI_H_
#define WIFI_H_

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_init();

esp_err_t wifi_set_creds(const char* ssid, const char* password);

bool wifi_get_connected_status();

#ifdef __cplusplus
}
#endif
#endif // WIFI_H_