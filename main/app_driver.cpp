/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <esp_matter.h>
#include <app_priv.h>
#include <common_macros.h>

#include <device.h>
#include "driver/ledc.h"
#include <button_gpio.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

#define LIGHT_PWM_GPIO      GPIO_NUM_4
#define LIGHT_PWM_TIMER     LEDC_TIMER_0
#define LIGHT_PWM_MODE      LEDC_LOW_SPEED_MODE
#define LIGHT_PWM_CHANNEL   LEDC_CHANNEL_0
#define LIGHT_PWM_FREQ_HZ   500
#define LIGHT_PWM_RES       LEDC_TIMER_10_BIT
#define LIGHT_PWM_MAX       1023

typedef struct {
    bool power;
    uint8_t brightness; // Matter level 0..254
} light_pwm_handle_t;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

// Global variables to store current XY color coordinates
static uint16_t current_x = 0;
static uint16_t current_y = 0;

static uint32_t brightness_to_duty(uint8_t brightness)
{
    return ((uint32_t)brightness * LIGHT_PWM_MAX) / 254;
}

static void apply_light_state(light_pwm_handle_t *handle)
{
    uint32_t duty = handle->power
                      ? brightness_to_duty(handle->brightness)
                      : 0;

    ledc_set_fade_with_time(
        LIGHT_PWM_MODE,
        LIGHT_PWM_CHANNEL,
        duty,
        250);   // 250 мс

    ledc_fade_start(
        LIGHT_PWM_MODE,
        LIGHT_PWM_CHANNEL,
        LEDC_FADE_NO_WAIT);
}

static esp_err_t app_driver_light_set_power(light_pwm_handle_t *handle,
                                            esp_matter_attr_val_t *val)
{
    handle->power = val->val.b;
    apply_light_state(handle);
    return ESP_OK;
}

static esp_err_t app_driver_light_set_brightness(light_pwm_handle_t *handle,
                                                 esp_matter_attr_val_t *val)
{
    handle->brightness = val->val.u8;
    apply_light_state(handle);
    return ESP_OK;
}

static esp_err_t app_driver_light_set_hue(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    // int value = REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE);
    // return led_driver_set_hue(handle, value);
    return ESP_OK;
}

static esp_err_t app_driver_light_set_saturation(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    // int value = REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION);
    // return led_driver_set_saturation(handle, value);
    return ESP_OK;
}

static esp_err_t app_driver_light_set_temperature(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    // uint32_t value = REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR);
    // return led_driver_set_temperature(handle, value);
    return ESP_OK;
}

static esp_err_t app_driver_light_set_xy(led_driver_handle_t handle, uint16_t x, uint16_t y)
{
    // return led_driver_set_xy(handle, x, y);
    return ESP_OK;
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        light_pwm_handle_t *handle = (light_pwm_handle_t *)driver_handle;
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(handle, val);
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            }
        } else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                err = app_driver_light_set_hue(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                err = app_driver_light_set_saturation(handle, val);
            } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentX::Id) {
                current_x = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            } else if (attribute_id == ColorControl::Attributes::CurrentY::Id) {
                current_y = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            }
        }
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    light_pwm_handle_t *handle = (light_pwm_handle_t *)priv_data;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    attribute_t *attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting color */
    attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation) {
        /* Setting hue */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, &val);
        /* Setting saturation */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kColorTemperature) {
        /* Setting temperature */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentXAndCurrentY) {
        /* Setting XY coordinates */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id);
        attribute::get_val(attribute, &val);
        current_x = val.val.u16;
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id);
        attribute::get_val(attribute, &val);
        current_y = val.val.u16;
        err |= app_driver_light_set_xy(handle, current_x, current_y);
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }

    /* Setting power */
    attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    return err;
}

app_driver_handle_t app_driver_light_init()
{
    static light_pwm_handle_t light = {
        .power = false,
        .brightness = 254,
    };

    ledc_timer_config_t timer = {
        .speed_mode = LIGHT_PWM_MODE,
        .duty_resolution = LIGHT_PWM_RES,
        .timer_num = LIGHT_PWM_TIMER,
        .freq_hz = LIGHT_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = LIGHT_PWM_GPIO,
        .speed_mode = LIGHT_PWM_MODE,
        .channel = LIGHT_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LIGHT_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);

    ledc_fade_func_install(0);

    return (app_driver_handle_t)&light;
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}
