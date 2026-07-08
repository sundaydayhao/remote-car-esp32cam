#include "control_protocol.h"

#include <stdbool.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lock;
static car_control_status_t s_status = {
    .last_source = CAR_CONTROL_SOURCE_RESERVED,
    .last_client_id = "none",
};

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static const char *safe_client_id(const char *client_id)
{
    return (client_id != NULL && client_id[0] != '\0') ? client_id : "anonymous";
}

static void update_status_locked(const char *client_id,
                                 car_control_source_t source,
                                 uint32_t seq,
                                 bool is_control)
{
    s_status.last_seq = seq;
    s_status.last_source = source;
    strlcpy(s_status.last_client_id, safe_client_id(client_id),
            sizeof(s_status.last_client_id));

    uint32_t now = now_ms();
    s_status.last_heartbeat_ms = now;
    if (is_control) {
        s_status.last_control_ms = now;
    }
}

esp_err_t control_protocol_init(void)
{
    if (s_lock != NULL) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    return s_lock == NULL ? ESP_ERR_NO_MEM : ESP_OK;
}

esp_err_t control_protocol_apply(const car_control_message_t *message,
                                 motor_status_t *motor_status_out)
{
    if (message == NULL || s_lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = motor_control_drive(message->motion, message->speed);

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        update_status_locked(message->client_id, message->source, message->seq, true);
        xSemaphoreGive(s_lock);
    }

    if (motor_status_out != NULL) {
        motor_control_get_status(motor_status_out);
    }

    return ret;
}

void control_protocol_heartbeat(const char *client_id,
                                car_control_source_t source,
                                uint32_t seq)
{
    if (s_lock == NULL) {
        return;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        update_status_locked(client_id, source, seq, false);
        xSemaphoreGive(s_lock);
    }
}

void control_protocol_get_status(car_control_status_t *status)
{
    if (status == NULL || s_lock == NULL) {
        return;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        *status = s_status;
        xSemaphoreGive(s_lock);
    }
}

const char *control_protocol_source_to_string(car_control_source_t source)
{
    switch (source) {
    case CAR_CONTROL_SOURCE_WEB:
        return "web";
    case CAR_CONTROL_SOURCE_VOICE:
        return "voice";
    case CAR_CONTROL_SOURCE_REMOTE:
        return "remote";
    case CAR_CONTROL_SOURCE_API:
        return "api";
    case CAR_CONTROL_SOURCE_RESERVED:
    default:
        return "reserved";
    }
}

car_control_source_t control_protocol_source_from_string(const char *value)
{
    if (value == NULL) {
        return CAR_CONTROL_SOURCE_API;
    }
    if (strcmp(value, "web") == 0) {
        return CAR_CONTROL_SOURCE_WEB;
    }
    if (strcmp(value, "voice") == 0) {
        return CAR_CONTROL_SOURCE_VOICE;
    }
    if (strcmp(value, "remote") == 0) {
        return CAR_CONTROL_SOURCE_REMOTE;
    }
    if (strcmp(value, "reserved") == 0) {
        return CAR_CONTROL_SOURCE_RESERVED;
    }
    return CAR_CONTROL_SOURCE_API;
}
