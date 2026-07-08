#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REMOTE_CAR_CMD_STOP = 0,
    REMOTE_CAR_CMD_FORWARD,
    REMOTE_CAR_CMD_BACKWARD,
    REMOTE_CAR_CMD_LEFT,
    REMOTE_CAR_CMD_RIGHT,
} remote_car_cmd_t;

typedef struct {
    remote_car_cmd_t cmd;
    uint8_t speed;
    uint32_t seq;
    bool ok;
} car_link_control_result_t;

esp_err_t car_link_init(void);
const char *car_link_client_id(void);
esp_err_t car_link_send_heartbeat(void);
esp_err_t car_link_send_control(remote_car_cmd_t cmd, uint8_t speed,
                                car_link_control_result_t *result);
esp_err_t car_link_send_gimbal(int pan_percent, int tilt_percent, bool center);
const char *car_link_cmd_to_string(remote_car_cmd_t cmd);

#ifdef __cplusplus
}
#endif
