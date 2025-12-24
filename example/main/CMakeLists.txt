idf_component_register( SRCS "main.cpp" "wifiManager/provisioning.c" "wifiManager/wifi.c" "sntp/sntp.c" "commands/commands.cpp"
                        REQUIRES ota iot_is jobs esp-idf-cxx wifi_provisioning sht4x scd41 esp_sgp4x esp_wifi nvs_flash esp_netif freertos esp_timer lwip driver
                        INCLUDE_DIRS "." "wifiManager" "sntp" "commands") 