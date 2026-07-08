#include "car_link.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"

static const char *TAG = "car_link";

static char s_client_id[24] = "remote";
static uint32_t s_seq;

static uint32_t next_seq(void)
{
    s_seq++;
    if (s_seq == 0) {
        s_seq = 1;
    }
    return s_seq;
}

static esp_err_t http_get(const char *path)
{
    char url[256];
    int len = snprintf(url, sizeof(url), "%s%s", CONFIG_REMOTE_CAR_BASE_URL, path);
    if (len < 0 || len >= (int)sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 800,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GET %s failed: %s", path, esp_err_to_name(ret));
        return ret;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "GET %s returned HTTP %d", path, status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t car_link_init(void)
{
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(s_client_id, sizeof(s_client_id), "remote-%02x%02x%02x",
             mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "client id: %s", s_client_id);
    return ESP_OK;
}

const char *car_link_client_id(void)
{
    return s_client_id;
}

const char *car_link_cmd_to_string(remote_car_cmd_t cmd)
{
    switch (cmd) {
    case REMOTE_CAR_CMD_FORWARD:
        return "forward";
    case REMOTE_CAR_CMD_BACKWARD:
        return "backward";
    case REMOTE_CAR_CMD_LEFT:
        return "left";
    case REMOTE_CAR_CMD_RIGHT:
        return "right";
    case REMOTE_CAR_CMD_STOP:
    default:
        return "stop";
    }
}

esp_err_t car_link_send_heartbeat(void)
{
    char path[128];
    uint32_t seq = next_seq();
    int len = snprintf(path, sizeof(path),
                       "/heartbeat?seq=%lu&source=remote&client=%s",
                       (unsigned long)seq, s_client_id);
    if (len < 0 || len >= (int)sizeof(path)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return http_get(path);
}

esp_err_t car_link_send_control(remote_car_cmd_t cmd, uint8_t speed,
                                car_link_control_result_t *result)
{
    if (speed > 100) {
        speed = 100;
    }

    char path[160];
    uint32_t seq = next_seq();
    int len = snprintf(path, sizeof(path),
                       "/control?cmd=%s&speed=%u&seq=%lu&source=remote&client=%s",
                       car_link_cmd_to_string(cmd), speed, (unsigned long)seq, s_client_id);
    if (len < 0 || len >= (int)sizeof(path)) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = http_get(path);
    if (result != NULL) {
        result->cmd = cmd;
        result->speed = speed;
        result->seq = seq;
        result->ok = ret == ESP_OK;
    }
    return ret;
}

esp_err_t car_link_send_gimbal(int pan_percent, int tilt_percent, bool center)
{
    if (pan_percent < -100) {
        pan_percent = -100;
    } else if (pan_percent > 100) {
        pan_percent = 100;
    }
    if (tilt_percent < -100) {
        tilt_percent = -100;
    } else if (tilt_percent > 100) {
        tilt_percent = 100;
    }

    char path[160];
    uint32_t seq = next_seq();
    int len = snprintf(path, sizeof(path),
                       "/gimbal?pan=%d&tilt=%d&center=%s&seq=%lu&source=remote&client=%s",
                       pan_percent, tilt_percent, center ? "true" : "false",
                       (unsigned long)seq, s_client_id);
    if (len < 0 || len >= (int)sizeof(path)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return http_get(path);
}
