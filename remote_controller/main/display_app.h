#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_app_start(void);
void display_app_set_status(const char *status);

#ifdef __cplusplus
}
#endif
