#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rm690b0.h"
#include "ws_241_hal.h"

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Waveshare 2.41 Display Test");

    esp_err_t ret = ws_241_hal_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HAL Init Failed!");
        return;
    }

    ESP_LOGI(TAG, "Display Initialized. Using rm690b0 driver.");

    // Run Test Pattern
    rm690b0_run_test_pattern();
}
