#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "motor_control.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_CONTROL_CLIENT_ID_MAX_LEN 31

typedef enum {
    CAR_CONTROL_SOURCE_WEB = 0,
    CAR_CONTROL_SOURCE_VOICE,
    CAR_CONTROL_SOURCE_REMOTE,
    CAR_CONTROL_SOURCE_API,
    CAR_CONTROL_SOURCE_RESERVED,
} car_control_source_t;

typedef struct {
    car_motion_t motion;
    uint8_t speed;
    uint32_t seq;
    car_control_source_t source;
    char client_id[CAR_CONTROL_CLIENT_ID_MAX_LEN + 1];
} car_control_message_t;

typedef struct {
    uint32_t last_seq;
    uint32_t last_heartbeat_ms;
    uint32_t last_control_ms;
    car_control_source_t last_source;
    char last_client_id[CAR_CONTROL_CLIENT_ID_MAX_LEN + 1];
} car_control_status_t;

esp_err_t control_protocol_init(void);
esp_err_t control_protocol_apply(const car_control_message_t *message,
                                 motor_status_t *motor_status_out);
void control_protocol_heartbeat(const char *client_id,
                                car_control_source_t source,
                                uint32_t seq);
void control_protocol_get_status(car_control_status_t *status);
const char *control_protocol_source_to_string(car_control_source_t source);
car_control_source_t control_protocol_source_from_string(const char *value);

#ifdef __cplusplus
}
#endif
