#include "audio_client.h"

#include "esp_log.h"

static const char *TAG = "audio_client";

esp_err_t audio_client_start(void)
{
#if CONFIG_REMOTE_AUDIO_ENABLED
    ESP_LOGW(TAG, "audio placeholder enabled; WebSocket/I2S implementation is reserved");
#else
    ESP_LOGI(TAG, "audio disabled");
#endif
    return ESP_OK;
}
