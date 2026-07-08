#include "web_server.h"

#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_intercom.h"
#include "camera_app.h"
#include "control_protocol.h"
#include "motor_control.h"
#include "ultrasonic.h"
#include "wifi_ap.h"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server = NULL;

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req,
                           (const char *)index_html_start,
                           (ssize_t)(index_html_end - index_html_start));
}

static esp_err_t snapshot_handler(httpd_req_t *req)
{
    if (!camera_app_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera not ready");
        return ESP_FAIL;
    }

    camera_fb_t *fb = camera_app_fb_get();
    if (fb == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t ret = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    camera_app_fb_return(fb);
    return ret;
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    if (!camera_app_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera not ready");
        return ESP_FAIL;
    }

    static const char *boundary = "--frame\r\n";
    char part_header[96];

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");

    while (true) {
        camera_fb_t *fb = camera_app_fb_get();
        if (fb == NULL) {
            ESP_LOGW(TAG, "camera frame unavailable");
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        int header_len = snprintf(part_header, sizeof(part_header),
                                  "Content-Type: image/jpeg\r\n"
                                  "Content-Length: %u\r\n\r\n",
                                  (unsigned int)fb->len);

        esp_err_t ret = httpd_resp_send_chunk(req, boundary, strlen(boundary));
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, part_header, header_len);
        }
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, "\r\n", 2);
        }

        camera_app_fb_return(fb);

        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "stream closed: %s", esp_err_to_name(ret));
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    return ESP_OK;
}

static uint8_t parse_speed(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return CONFIG_CAR_MOTOR_DEFAULT_SPEED;
    }

    int speed = atoi(value);
    if (speed < 0) {
        speed = 0;
    } else if (speed > 100) {
        speed = 100;
    }
    return (uint8_t)speed;
}

static uint32_t parse_u32(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    return (uint32_t)strtoul(value, NULL, 10);
}

static void normalize_client_id(char *value)
{
    if (value == NULL || value[0] == '\0') {
        strlcpy(value, "anonymous", CAR_CONTROL_CLIENT_ID_MAX_LEN + 1);
        return;
    }

    value[CAR_CONTROL_CLIENT_ID_MAX_LEN] = '\0';
    for (char *p = value; *p != '\0'; p++) {
        bool ok = (*p >= '0' && *p <= '9') ||
                  (*p >= 'a' && *p <= 'z') ||
                  (*p >= 'A' && *p <= 'Z') ||
                  *p == '-' || *p == '_' || *p == '.' || *p == ':';
        if (!ok) {
            *p = '_';
        }
    }
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void url_decode_in_place(char *value)
{
    char *src = value;
    char *dst = value;

    while (*src != '\0') {
        if (*src == '%' && hex_value(src[1]) >= 0 && hex_value(src[2]) >= 0) {
            *dst++ = (char)((hex_value(src[1]) << 4) | hex_value(src[2]));
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool text_has_any(const char *text, const char *a, const char *b,
                         const char *c, const char *d)
{
    return (a != NULL && strstr(text, a) != NULL) ||
           (b != NULL && strstr(text, b) != NULL) ||
           (c != NULL && strstr(text, c) != NULL) ||
           (d != NULL && strstr(text, d) != NULL);
}

static car_motion_t motion_from_voice_text(const char *text, bool *recognized)
{
    if (recognized != NULL) {
        *recognized = true;
    }

    if (text == NULL || text[0] == '\0') {
        if (recognized != NULL) {
            *recognized = false;
        }
        return CAR_MOTION_STOP;
    }

    if (text_has_any(text, "停止", "停车", "刹车", "stop") ||
        text_has_any(text, "停下", "别动", "不要动", NULL)) {
        return CAR_MOTION_STOP;
    }
    if (text_has_any(text, "原地左", "左旋", "spin left", "rotate left")) {
        return CAR_MOTION_SPIN_LEFT;
    }
    if (text_has_any(text, "原地右", "右旋", "spin right", "rotate right")) {
        return CAR_MOTION_SPIN_RIGHT;
    }
    if (text_has_any(text, "左转", "向左", "往左", "turn left") ||
        text_has_any(text, "左边", "left", NULL, NULL)) {
        return CAR_MOTION_LEFT;
    }
    if (text_has_any(text, "右转", "向右", "往右", "turn right") ||
        text_has_any(text, "右边", "right", NULL, NULL)) {
        return CAR_MOTION_RIGHT;
    }
    if (text_has_any(text, "后退", "倒车", "向后", "backward") ||
        text_has_any(text, "back", NULL, NULL, NULL)) {
        return CAR_MOTION_BACKWARD;
    }
    if (text_has_any(text, "前进", "向前", "往前", "forward") ||
        text_has_any(text, "go ahead", "go", NULL, NULL)) {
        return CAR_MOTION_FORWARD;
    }

    if (recognized != NULL) {
        *recognized = false;
    }
    return CAR_MOTION_STOP;
}

static esp_err_t control_handler(httpd_req_t *req)
{
    char query[192] = {0};
    char cmd[32] = "stop";
    char speed_value[8] = {0};
    char seq_value[16] = {0};
    char source_value[16] = "web";
    char client_id[CAR_CONTROL_CLIENT_ID_MAX_LEN + 1] = "anonymous";

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "cmd", cmd, sizeof(cmd));
        httpd_query_key_value(query, "speed", speed_value, sizeof(speed_value));
        httpd_query_key_value(query, "seq", seq_value, sizeof(seq_value));
        httpd_query_key_value(query, "source", source_value, sizeof(source_value));
        httpd_query_key_value(query, "client", client_id, sizeof(client_id));
    }
    normalize_client_id(client_id);

    car_control_message_t message = {
        .motion = car_motion_from_string(cmd),
        .speed = parse_speed(speed_value),
        .seq = parse_u32(seq_value),
        .source = control_protocol_source_from_string(source_value),
    };
    strlcpy(message.client_id, client_id, sizeof(message.client_id));

    motor_status_t status = {0};
    esp_err_t drive_ret = control_protocol_apply(&message, &status);

    char body[384];
    int len = snprintf(body, sizeof(body),
                       "{\"ok\":%s,\"cmd\":\"%s\",\"speed\":%u,"
                       "\"left\":%d,\"right\":%d,\"blocked\":%s,"
                       "\"distance_cm\":%d,\"seq\":%lu,"
                       "\"source\":\"%s\",\"client\":\"%s\"}\n",
                       drive_ret == ESP_OK ? "true" : "false",
                       car_motion_to_string(status.motion),
                       status.speed,
                       status.left_percent,
                       status.right_percent,
                       status.obstacle_blocked ? "true" : "false",
                       ultrasonic_get_distance_cm(),
                       (unsigned long)message.seq,
                       control_protocol_source_to_string(message.source),
                       message.client_id);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, len);
}

static esp_err_t voice_handler(httpd_req_t *req)
{
    char query[256] = {0};
    char text[128] = {0};
    char speed_value[8] = {0};
    char seq_value[16] = {0};
    char client_id[CAR_CONTROL_CLIENT_ID_MAX_LEN + 1] = "anonymous";

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "text", text, sizeof(text));
        httpd_query_key_value(query, "speed", speed_value, sizeof(speed_value));
        httpd_query_key_value(query, "seq", seq_value, sizeof(seq_value));
        httpd_query_key_value(query, "client", client_id, sizeof(client_id));
    }
    url_decode_in_place(text);
    normalize_client_id(client_id);

    bool recognized = false;
    car_motion_t motion = motion_from_voice_text(text, &recognized);
    esp_err_t drive_ret = ESP_ERR_NOT_FOUND;

    motor_status_t status = {0};
    if (recognized) {
        car_control_message_t message = {
            .motion = motion,
            .speed = parse_speed(speed_value),
            .seq = parse_u32(seq_value),
            .source = CAR_CONTROL_SOURCE_VOICE,
        };
        strlcpy(message.client_id, client_id, sizeof(message.client_id));
        drive_ret = control_protocol_apply(&message, &status);
    } else {
        motor_control_get_status(&status);
    }

    char body[384];
    int len = snprintf(body, sizeof(body),
                       "{\"ok\":%s,\"recognized\":%s,\"cmd\":\"%s\","
                       "\"speed\":%u,\"left\":%d,\"right\":%d,"
                       "\"blocked\":%s,\"distance_cm\":%d,\"camera\":%s,"
                       "\"seq\":%lu,\"source\":\"voice\",\"client\":\"%s\"}\n",
                       (recognized && drive_ret == ESP_OK) ? "true" : "false",
                       recognized ? "true" : "false",
                       recognized ? car_motion_to_string(motion) : car_motion_to_string(status.motion),
                       status.speed,
                       status.left_percent,
                       status.right_percent,
                       status.obstacle_blocked ? "true" : "false",
                       ultrasonic_get_distance_cm(),
                       camera_app_is_ready() ? "true" : "false",
                       (unsigned long)parse_u32(seq_value),
                       client_id);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, len);
}

static esp_err_t heartbeat_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char seq_value[16] = {0};
    char source_value[16] = "web";
    char client_id[CAR_CONTROL_CLIENT_ID_MAX_LEN + 1] = "anonymous";

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "seq", seq_value, sizeof(seq_value));
        httpd_query_key_value(query, "source", source_value, sizeof(source_value));
        httpd_query_key_value(query, "client", client_id, sizeof(client_id));
    }
    normalize_client_id(client_id);

    car_control_source_t source = control_protocol_source_from_string(source_value);
    uint32_t seq = parse_u32(seq_value);
    control_protocol_heartbeat(client_id, source, seq);

    char body[192];
    int len = snprintf(body, sizeof(body),
                       "{\"ok\":true,\"seq\":%lu,\"source\":\"%s\","
                       "\"client\":\"%s\"}\n",
                       (unsigned long)seq,
                       control_protocol_source_to_string(source),
                       client_id);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, len);
}

#if CONFIG_HTTPD_WS_SUPPORT && CONFIG_CAR_ENABLE_INTERCOM
static esp_err_t audio_ws_handler(httpd_req_t *req)
{
    int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) {
        esp_err_t ret = audio_intercom_set_ws_client(req->handle, fd);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "audio websocket rejected: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    httpd_ws_frame_t frame = {0};
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        audio_intercom_clear_ws_client(fd);
        ESP_LOGW(TAG, "audio ws frame header failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (frame.len > CONFIG_CAR_AUDIO_MAX_INCOMING_BYTES) {
        audio_intercom_clear_ws_client(fd);
        ESP_LOGW(TAG, "audio ws frame too large: %u", (unsigned int)frame.len);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *payload = NULL;
    if (frame.len > 0) {
        payload = malloc(frame.len);
        if (payload == NULL) {
            return ESP_ERR_NO_MEM;
        }
        frame.payload = payload;
        ret = httpd_ws_recv_frame(req, &frame, frame.len);
        if (ret != ESP_OK) {
            free(payload);
            audio_intercom_clear_ws_client(fd);
            ESP_LOGW(TAG, "audio ws payload failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    if (frame.type == HTTPD_WS_TYPE_BINARY && frame.len > 0) {
        ret = audio_intercom_play_pcm(frame.payload, frame.len);
        if (ret == ESP_ERR_TIMEOUT) {
            ret = ESP_OK;
        }
    } else if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        audio_intercom_clear_ws_client(fd);
    }

    free(payload);
    return ret == ESP_ERR_NOT_SUPPORTED ? ESP_OK : ret;
}
#endif

static esp_err_t status_handler(httpd_req_t *req)
{
    motor_status_t status = {0};
    motor_control_get_status(&status);
    car_control_status_t control_status = {0};
    control_protocol_get_status(&control_status);
    wifi_ap_status_t wifi_status = {0};
    wifi_ap_get_status(&wifi_status);

    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t age_ms = now - status.last_command_ms;
    uint32_t heartbeat_age_ms = control_status.last_heartbeat_ms == 0
                                    ? 0
                                    : now - control_status.last_heartbeat_ms;
    uint32_t control_age_ms = control_status.last_control_ms == 0
                                  ? 0
                                  : now - control_status.last_control_ms;

    char body[768];
    int len = snprintf(body, sizeof(body),
                       "{\"uptime_s\":%lu,\"cmd\":\"%s\",\"speed\":%u,"
                       "\"left\":%d,\"right\":%d,\"blocked\":%s,"
                       "\"distance_cm\":%d,\"camera\":%s,"
                       "\"audio_ready\":%s,\"audio_client\":%s,"
                       "\"last_command_age_ms\":%lu,"
                       "\"control_age_ms\":%lu,\"heartbeat_age_ms\":%lu,"
                       "\"control_seq\":%lu,\"control_source\":\"%s\","
                       "\"control_client\":\"%s\","
                       "\"network\":{\"transport\":\"wifi\",\"mode\":\"%s\","
                       "\"ssid\":\"%s\",\"channel\":%u,\"station_count\":%u,"
                       "\"max_station_count\":%u},"
                       "\"expansion\":{\"cellular_reserved\":true,"
                       "\"cellular_active\":false}}\n",
                       (unsigned long)uptime_s,
                       car_motion_to_string(status.motion),
                       status.speed,
                       status.left_percent,
                       status.right_percent,
                       status.obstacle_blocked ? "true" : "false",
                       ultrasonic_get_distance_cm(),
                       camera_app_is_ready() ? "true" : "false",
                       audio_intercom_is_ready() ? "true" : "false",
                       audio_intercom_has_client() ? "true" : "false",
                       (unsigned long)age_ms,
                       (unsigned long)control_age_ms,
                       (unsigned long)heartbeat_age_ms,
                       (unsigned long)control_status.last_seq,
                       control_protocol_source_to_string(control_status.last_source),
                       control_status.last_client_id,
                       wifi_status.mode,
                       wifi_status.ssid,
                       wifi_status.channel,
                       wifi_status.station_count,
                       wifi_status.max_station_count);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, len);
}

esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.max_uri_handlers = 10;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "http server start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
    };
    const httpd_uri_t uri_stream = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
    };
    const httpd_uri_t uri_snapshot = {
        .uri = "/jpg",
        .method = HTTP_GET,
        .handler = snapshot_handler,
    };
    const httpd_uri_t uri_control = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = control_handler,
    };
    const httpd_uri_t uri_status = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
    };
    const httpd_uri_t uri_voice = {
        .uri = "/voice",
        .method = HTTP_GET,
        .handler = voice_handler,
    };
    const httpd_uri_t uri_heartbeat = {
        .uri = "/heartbeat",
        .method = HTTP_GET,
        .handler = heartbeat_handler,
    };
#if CONFIG_HTTPD_WS_SUPPORT && CONFIG_CAR_ENABLE_INTERCOM
    const httpd_uri_t uri_audio = {
        .uri = "/audio",
        .method = HTTP_GET,
        .handler = audio_ws_handler,
        .is_websocket = true,
    };
#endif

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_index));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_stream));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_snapshot));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_control));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_status));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_voice));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_heartbeat));
#if CONFIG_HTTPD_WS_SUPPORT && CONFIG_CAR_ENABLE_INTERCOM
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uri_audio));
#endif

    ESP_LOGI(TAG, "web server ready");
    return ESP_OK;
}
