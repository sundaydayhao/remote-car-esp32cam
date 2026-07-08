#include <stdlib.h>
#include <stdio.h>

#include "audio_client.h"
#include "car_link.h"
#include "display_app.h"
#include "joystick.h"
#include "wifi_client.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "remote_main";

static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }
}

static uint8_t speed_from_axis(int value)
{
    int speed = abs(value);
    if (speed == 0) {
        return 0;
    }
    if (speed < CONFIG_REMOTE_MIN_DRIVE_SPEED) {
        speed = CONFIG_REMOTE_MIN_DRIVE_SPEED;
    }
    if (speed > 100) {
        speed = 100;
    }
    return (uint8_t)speed;
}

static remote_car_cmd_t command_from_drive_joystick(const joystick_state_t *state,
                                                    uint8_t *speed_out)
{
    if (speed_out != NULL) {
        *speed_out = 0;
    }
    if (state == NULL || !state->valid || joystick_estop_pressed() || state->pressed) {
        return REMOTE_CAR_CMD_STOP;
    }

    int abs_x = abs(state->x);
    int abs_y = abs(state->y);
    if (abs_x == 0 && abs_y == 0) {
        return REMOTE_CAR_CMD_STOP;
    }

    if (abs_y >= abs_x) {
        if (speed_out != NULL) {
            *speed_out = speed_from_axis(state->y);
        }
        return state->y >= 0 ? REMOTE_CAR_CMD_FORWARD : REMOTE_CAR_CMD_BACKWARD;
    }

    if (speed_out != NULL) {
        *speed_out = speed_from_axis(state->x);
    }
    return state->x >= 0 ? REMOTE_CAR_CMD_RIGHT : REMOTE_CAR_CMD_LEFT;
}

static void heartbeat_task(void *arg)
{
    (void)arg;

    while (true) {
        if (wifi_client_is_connected()) {
            esp_err_t ret = car_link_send_heartbeat();
            display_app_set_status(ret == ESP_OK ? "heartbeat ok" : "heartbeat failed");
        } else {
            display_app_set_status("waiting for car Wi-Fi");
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_REMOTE_HEARTBEAT_PERIOD_MS));
    }
}

static void drive_control_task(void *arg)
{
    (void)arg;

    remote_car_cmd_t last_cmd = REMOTE_CAR_CMD_STOP;
    uint8_t last_speed = 0;

    while (true) {
        joystick_state_t drive = {0};
        joystick_read_drive(&drive);

        uint8_t speed = 0;
        remote_car_cmd_t cmd = command_from_drive_joystick(&drive, &speed);

        if (wifi_client_is_connected()) {
            car_link_control_result_t result = {0};
            esp_err_t ret = car_link_send_control(cmd, speed, &result);
            char status[96];
            snprintf(status, sizeof(status), "%s %u%% %s",
                     car_link_cmd_to_string(cmd), speed,
                     ret == ESP_OK ? "ok" : "failed");
            display_app_set_status(status);
            if (ret == ESP_OK && (cmd != last_cmd || speed != last_speed)) {
                ESP_LOGI(TAG, "drive %s speed=%u x=%d y=%d",
                         car_link_cmd_to_string(cmd), speed, drive.x, drive.y);
                last_cmd = cmd;
                last_speed = speed;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_REMOTE_CONTROL_PERIOD_MS));
    }
}

static void camera_control_task(void *arg)
{
    (void)arg;

    int last_x = 0;
    int last_y = 0;
    bool last_pressed = false;

    while (true) {
        joystick_state_t camera = {0};
        joystick_read_camera(&camera);

        bool changed = camera.x != last_x || camera.y != last_y ||
                       camera.pressed != last_pressed;
        if (wifi_client_is_connected() && camera.valid && changed) {
            esp_err_t ret = car_link_send_gimbal(camera.x, camera.y, camera.pressed);
            ESP_LOGI(TAG, "gimbal pan=%d tilt=%d center=%d %s",
                     camera.x, camera.y, camera.pressed,
                     ret == ESP_OK ? "sent" : "failed");
            last_x = camera.x;
            last_y = camera.y;
            last_pressed = camera.pressed;
        }

        vTaskDelay(pdMS_TO_TICKS(120));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 remote controller booting");

    init_nvs();

    ESP_ERROR_CHECK(display_app_start());
    ESP_ERROR_CHECK(car_link_init());
    ESP_ERROR_CHECK(joystick_init());
    ESP_ERROR_CHECK(audio_client_start());
    ESP_ERROR_CHECK(wifi_client_start());

    xTaskCreate(heartbeat_task, "heartbeat", 4096, NULL, 4, NULL);
    xTaskCreate(drive_control_task, "drive_control", 4096, NULL, 5, NULL);
    xTaskCreate(camera_control_task, "camera_control", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "remote ready, client=%s", car_link_client_id());
}
