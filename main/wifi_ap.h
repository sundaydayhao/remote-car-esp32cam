#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool started;
    uint8_t station_count;
    uint8_t max_station_count;
    uint8_t channel;
    char ssid[33];
    char mode[8];
} wifi_ap_status_t;

esp_err_t wifi_ap_start(void);
void wifi_ap_get_status(wifi_ap_status_t *status);
