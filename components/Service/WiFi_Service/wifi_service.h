#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    esp_err_t last_err;

    bool sta_configured;
    bool sta_connected;
    uint32_t sta_ip;
    int sta_rssi;

    bool ap_on;
    int ap_clients;
    uint32_t ap_ip;
} wifi_service_status_t;

void wifi_service_set_enabled(bool enable);
esp_err_t wifi_service_get_status(wifi_service_status_t *out);

