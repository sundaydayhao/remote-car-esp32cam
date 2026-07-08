#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    bool pressed;
    bool valid;
} joystick_state_t;

esp_err_t joystick_init(void);
void joystick_read_drive(joystick_state_t *state);
void joystick_read_camera(joystick_state_t *state);
bool joystick_estop_pressed(void);

#ifdef __cplusplus
}
#endif
