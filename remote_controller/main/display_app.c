#include "display_app.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "display";

static SemaphoreHandle_t s_lock;
static char s_status[96] = "booting";

static bool gpio_valid(int gpio)
{
    return gpio >= 0 && gpio <= 39;
}

static void display_task(void *arg)
{
    (void)arg;

    while (true) {
        char status[sizeof(s_status)];
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            strlcpy(status, s_status, sizeof(status));
            xSemaphoreGive(s_lock);
        } else {
            strlcpy(status, "status unavailable", sizeof(status));
        }

        ESP_LOGI(TAG, "TFT placeholder %dx%d: %s",
                 CONFIG_REMOTE_TFT_WIDTH, CONFIG_REMOTE_TFT_HEIGHT, status);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

esp_err_t display_app_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int backlight_gpio = CONFIG_REMOTE_TFT_BACKLIGHT_GPIO;
    if (gpio_valid(backlight_gpio)) {
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << backlight_gpio,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&cfg));
        gpio_set_level((gpio_num_t)backlight_gpio, 1);
    }

#if CONFIG_REMOTE_TFT_ENABLED
    xTaskCreate(display_task, "display", 3072, NULL, 3, NULL);
#else
    ESP_LOGI(TAG, "TFT disabled");
#endif
    return ESP_OK;
}

void display_app_set_status(const char *status)
{
    if (s_lock == NULL || status == NULL) {
        return;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        strlcpy(s_status, status, sizeof(s_status));
        xSemaphoreGive(s_lock);
    }
}
