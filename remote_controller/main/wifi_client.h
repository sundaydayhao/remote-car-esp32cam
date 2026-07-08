#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_client_start(void);
bool wifi_client_is_connected(void);

#ifdef __cplusplus
}
#endif
