#ifndef PROVISIONING_H_
#define PROVISIONING_H_

#include "esp_err.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"


#ifdef __cplusplus
extern "C" {
#endif


#ifndef FORCE_PROV_GPIO
#define FORCE_PROV_GPIO GPIO_NUM_0    // pick a safe pin for your board
#endif

#ifndef FORCE_PROV_ACTIVE_LEVEL
#define FORCE_PROV_ACTIVE_LEVEL 0            // 0 for active-low, 1 for active-high
#endif

#ifndef FORCE_PROV_HOLD_MS
#define FORCE_PROV_HOLD_MS     300           // require being held this long at boot
#endif

bool force_prov_requested(void);

esp_err_t start_provisioning();

#ifdef __cplusplus
}
#endif
#endif // PROVISIONING_H_
