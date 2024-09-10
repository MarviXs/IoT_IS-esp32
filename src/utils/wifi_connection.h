#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

/**
 * @brief Start the WiFi connection task.
 * 
 * This function initializes and starts a FreeRTOS task that handles WiFi connection.
 * It will attempt to connect to the specified WiFi network and retry if the connection fails.
 * 
 * @param ssid The SSID of the WiFi network to connect to.
 * @param password The password of the WiFi network.
 */
void start_wifi_connection(const char *ssid, const char *password);

#endif // WIFI_CONNECTION_H
