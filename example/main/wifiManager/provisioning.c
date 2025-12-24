#include "provisioning.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define TAG "PROV"

bool force_prov_requested(void)
{
    // Configure GPIO
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << FORCE_PROV_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = (FORCE_PROV_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (FORCE_PROV_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io);

    // Quick sample: if not asserted now, bail fast
    int level = gpio_get_level(FORCE_PROV_GPIO);
    if (level != FORCE_PROV_ACTIVE_LEVEL) return false;

    // Require a short hold to avoid accidental triggers
    int64_t start = esp_timer_get_time(); // us
    while ((esp_timer_get_time() - start) < (FORCE_PROV_HOLD_MS * 1000)) {
        if (gpio_get_level(FORCE_PROV_GPIO) != FORCE_PROV_ACTIVE_LEVEL) {
            ESP_LOGI(TAG, "Force-provision pin released before hold time");
            return false;
        }
    }

    ESP_LOGW(TAG, "Force provisioning requested (GPIO %d level %d)", FORCE_PROV_GPIO, FORCE_PROV_ACTIVE_LEVEL);
    return true;
}

esp_err_t start_provisioning(){
    // Configuration for the provisioning manager
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    const bool force = force_prov_requested();

   if (force || !provisioned) {
        if (force && provisioned) {
            ESP_LOGW(TAG, "Device is provisioned but force-provision pin is held — starting provisioning");
        } else if (!provisioned) {
            ESP_LOGI(TAG, "Not provisioned — starting provisioning");
        }

        // Build a unique SoftAP SSID, e.g., using MAC
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
        char service_name[32];
        snprintf(service_name, sizeof(service_name), "SDG_IS_%02X%02X%02X", mac[3], mac[4], mac[5]);

        // Prefer security 1 with PoP
        const char *pop = "sdg4857"; 
        ESP_LOGI(TAG, "Starting provisioning SSID:%s (SEC1 + PoP)", service_name);

        ESP_ERROR_CHECK(
            wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, (void *)pop, service_name,
                                             NULL /* service_key=null => open AP passphrase */));

        wifi_prov_mgr_wait();   // returns after WIFI_PROV_END
        
        wifi_prov_mgr_deinit();
    } else {
        ESP_LOGI(TAG, "Already provisioned; starting STA");
        // No provisioning: just bring up STA with stored creds
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    return ESP_OK;
}