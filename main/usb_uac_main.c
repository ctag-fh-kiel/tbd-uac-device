/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <math.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_device_uac.h"
#include "codec.h"

static const char *TAG = "usb_uac_main";

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
    uint32_t bytes_written = 0;
    //ESP_LOGI(TAG, "uac_device_output_cb: %"PRIu32"", len);
    //bsp_extra_i2s_write(buf, len, &bytes_written, 0);
    i2s_write(buf, len, &bytes_written);
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    //ESP_LOGI(TAG, "uac_device_input_cb: %"PRIu32"", len);
    /*
    if (bsp_extra_i2s_read(buf, len, bytes_read, 0) != ESP_OK) {
        ESP_LOGE(TAG, "i2s read failed");
    }
    */
    uint32_t br = 0;
    i2s_read(buf, len, &br);
    *bytes_read = br;
    return ESP_OK;
}

static void uac_device_set_mute_cb(uint32_t mute, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_mute_cb: %"PRIu32"", mute);
    SetMute(mute, mute);
}

static void uac_device_set_volume_cb(uint32_t volume, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    SetOutputLevels(volume, volume);
}

void app_main(void)
{
    InitCodec();
    //bsp_extra_codec_set_fs(CONFIG_UAC_SAMPLE_RATE, 16, CONFIG_UAC_SPEAKER_CHANNEL_NUM);

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .set_mute_cb = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx = NULL,
    };

    uac_device_init(&config);
}
