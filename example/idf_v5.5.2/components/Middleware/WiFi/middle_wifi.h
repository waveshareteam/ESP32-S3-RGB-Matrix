#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    esp_err_t last_err;      // Last error code
    bool sta_configured;     // Whether STA credentials are configured
    bool sta_connected;      // Whether STA is connected
    uint32_t sta_ip;         // STA IP address
    int sta_rssi;            // STA signal strength (RSSI)
    bool ap_on;              // Whether AP mode is enabled
    int ap_clients;          // Number of connected AP clients
    uint32_t ap_ip;          // AP IP address
} middle_wifi_status_t;

void middle_wifi_set_sta_config(const char *ssid, const char *password); // Set STA SSID and password

esp_err_t middle_wifi_init(void); // Initialize WiFi service and start background polling

esp_err_t middle_wifi_get_status(middle_wifi_status_t *out); // Get current WiFi service status
