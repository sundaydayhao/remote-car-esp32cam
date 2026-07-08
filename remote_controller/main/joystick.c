#include "joystick.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "joystick";

typedef struct {
    int gpio;
    adc_unit_t unit;
    adc_channel_t channel;
    bool enabled;
} adc_axis_t;

static adc_oneshot_unit_handle_t s_adc1;
static adc_oneshot_unit_handle_t s_adc2;
static adc_axis_t s_drive_x = {.gpio = CONFIG_REMOTE_DRIVE_JOY_X_GPIO};
static adc_axis_t s_drive_y = {.gpio = CONFIG_REMOTE_DRIVE_JOY_Y_GPIO};
static adc_axis_t s_camera_x = {.gpio = CONFIG_REMOTE_CAMERA_JOY_X_GPIO};
static adc_axis_t s_camera_y = {.gpio = CONFIG_REMOTE_CAMERA_JOY_Y_GPIO};

#ifdef CONFIG_REMOTE_INVERT_DRIVE_X
#define REMOTE_INVERT_DRIVE_X_VALUE true
#else
#define REMOTE_INVERT_DRIVE_X_VALUE false
#endif

#ifdef CONFIG_REMOTE_INVERT_DRIVE_Y
#define REMOTE_INVERT_DRIVE_Y_VALUE true
#else
#define REMOTE_INVERT_DRIVE_Y_VALUE false
#endif

#ifdef CONFIG_REMOTE_INVERT_CAMERA_X
#define REMOTE_INVERT_CAMERA_X_VALUE true
#else
#define REMOTE_INVERT_CAMERA_X_VALUE false
#endif

#ifdef CONFIG_REMOTE_INVERT_CAMERA_Y
#define REMOTE_INVERT_CAMERA_Y_VALUE true
#else
#define REMOTE_INVERT_CAMERA_Y_VALUE false
#endif

static bool gpio_valid(int gpio)
{
    return gpio >= 0 && gpio <= 39;
}

static esp_err_t configure_button(int gpio)
{
    if (!gpio_valid(gpio)) {
        return ESP_OK;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cfg);
}

static esp_err_t configure_axis(adc_axis_t *axis)
{
    if (axis == NULL || !gpio_valid(axis->gpio)) {
        return ESP_OK;
    }

    esp_err_t ret = adc_oneshot_io_to_channel(axis->gpio, &axis->unit, &axis->channel);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GPIO%d is not ADC capable", axis->gpio);
        return ESP_OK;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    adc_oneshot_unit_handle_t handle = axis->unit == ADC_UNIT_1 ? s_adc1 : s_adc2;
    ret = adc_oneshot_config_channel(handle, axis->channel, &chan_cfg);
    if (ret == ESP_OK) {
        axis->enabled = true;
        ESP_LOGI(TAG, "GPIO%d mapped to ADC unit %d channel %d",
                 axis->gpio, axis->unit, axis->channel);
    }
    return ret;
}

static int read_axis_percent(const adc_axis_t *axis, bool invert)
{
    if (axis == NULL || !axis->enabled) {
        return 0;
    }

    int raw = 0;
    adc_oneshot_unit_handle_t handle = axis->unit == ADC_UNIT_1 ? s_adc1 : s_adc2;
    if (adc_oneshot_read(handle, axis->channel, &raw) != ESP_OK) {
        return 0;
    }

    int centered = raw - 2048;
    int percent = (centered * 100) / 2048;
    if (percent < -100) {
        percent = -100;
    } else if (percent > 100) {
        percent = 100;
    }

    if (abs(percent) < CONFIG_REMOTE_JOYSTICK_DEADZONE_PERCENT) {
        percent = 0;
    }

    return invert ? -percent : percent;
}

static bool read_button(int gpio)
{
    if (!gpio_valid(gpio)) {
        return false;
    }
    return gpio_get_level((gpio_num_t)gpio) == 0;
}

esp_err_t joystick_init(void)
{
    adc_oneshot_unit_init_cfg_t adc1_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_cfg, &s_adc1));

    adc_oneshot_unit_init_cfg_t adc2_cfg = {
        .unit_id = ADC_UNIT_2,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc2_cfg, &s_adc2));

    ESP_ERROR_CHECK(configure_axis(&s_drive_x));
    ESP_ERROR_CHECK(configure_axis(&s_drive_y));
    ESP_ERROR_CHECK(configure_axis(&s_camera_x));
    ESP_ERROR_CHECK(configure_axis(&s_camera_y));

    ESP_ERROR_CHECK(configure_button(CONFIG_REMOTE_DRIVE_JOY_BUTTON_GPIO));
    ESP_ERROR_CHECK(configure_button(CONFIG_REMOTE_CAMERA_JOY_BUTTON_GPIO));
    ESP_ERROR_CHECK(configure_button(CONFIG_REMOTE_ESTOP_BUTTON_GPIO));

    return ESP_OK;
}

void joystick_read_drive(joystick_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->x = read_axis_percent(&s_drive_x, REMOTE_INVERT_DRIVE_X_VALUE);
    state->y = read_axis_percent(&s_drive_y, REMOTE_INVERT_DRIVE_Y_VALUE);
    state->pressed = read_button(CONFIG_REMOTE_DRIVE_JOY_BUTTON_GPIO);
    state->valid = s_drive_x.enabled || s_drive_y.enabled;
}

void joystick_read_camera(joystick_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->x = read_axis_percent(&s_camera_x, REMOTE_INVERT_CAMERA_X_VALUE);
    state->y = read_axis_percent(&s_camera_y, REMOTE_INVERT_CAMERA_Y_VALUE);
    state->pressed = read_button(CONFIG_REMOTE_CAMERA_JOY_BUTTON_GPIO);
    state->valid = s_camera_x.enabled || s_camera_y.enabled;
}

bool joystick_estop_pressed(void)
{
    return read_button(CONFIG_REMOTE_ESTOP_BUTTON_GPIO);
}
