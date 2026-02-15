#include "ws_241_hal.h"
#include "rm690b0.h"
#include "ft6336u.h"
#include "tca9554.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/rtc_io.h" /* Included for sleep GPIO functions */
#include "iot_button.h" /* Espressif Button Component */
#include "button_gpio.h" /* Espressif Button Component (GPIO Specific) */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_BUTTON_GPIO    0

/*
 * Note on Power Management:
 * The ETA6098 Power Management IC used on this board is passive to the programmer.
 * It manages battery charging and system power autonomously and does not require
 * any software configuration or driver implementation.
 *
 * LED Status Indicators:
 * - Red LED: Indicates the board is powered (USB or Battery).
 * - Green LED: Indicates Charging status (On = Charging, Off = Charged).
 */

static const char *TAG = "WS_241_HAL";

// TCA9554 Pins (based on schematic)
// EXIO1 -> PWR_EN (Power Enable for Display)
// EXIO0 -> OLED_TE (Tearing Effect Input from Display)
// EXIO2 -> TP_INT (Touch Interrupt)
// EXIO3 -> IMU_INT2
// EXIO4 -> IMU_INT1
// EXIO5/6 -> Unknown
#define TCA_PIN_PWR_EN      (1 << 1)
#define TCA_PIN_TE          (1 << 0)
#define TCA_PIN_TP_INT      (1 << 2)

static rm690b0_config_t g_disp_conf = {
    .cs_io = WS_241_QSPI_CS,
    .clk_io = WS_241_QSPI_CLK,
    .d0_io = WS_241_QSPI_D0,
    .d1_io = WS_241_QSPI_D1,
    .d2_io = WS_241_QSPI_D2,
    .d3_io = WS_241_QSPI_D3,
    .rst_io = WS_241_DISP_RST,
    .te_io = -1, // TE is on TCA9554, not direct GPIO
    .host_id = WS_241_QSPI_HOST,
};

static void power_button_task(void *pvParameters) {
    const TickType_t POLL_DELAY = pdMS_TO_TICKS(100);
    const int LONG_PRESS_MS = 1500; // 1.5 Seconds
    
    ESP_LOGI(TAG, "Power Button Task Started (Active Low)");

    while (1) {
        // Logic 0 = Pressed, Logic 1 = Released
        if (gpio_get_level(WS_241_PWR_BTN_GPIO) == 0) { // Button Pressed (Active Low)
            uint32_t press_duration = 0;
            ESP_LOGI(TAG, "Power Button Pressed. Holding...");

            while (gpio_get_level(WS_241_PWR_BTN_GPIO) == 0) {
                vTaskDelay(POLL_DELAY);
                press_duration += 100;
                
                if (press_duration >= LONG_PRESS_MS) {
                    ESP_LOGI(TAG, "Long Press Detected (%d ms). Preparing for Sleep...", press_duration);
                    
                    // VISUAL FEEDBACK: Turn screen BLACK and OFF immediately
                    ESP_LOGI(TAG, "Turning off Display...");
                    rm690b0_fill_screen(0x0000); // Write Black
                    ws_241_hal_set_display_power(false); // Cut Power
                    
                    // Wait for release so we don't wake up immediately
                    ESP_LOGI(TAG, "Release button to enter Light Sleep");
                    while (gpio_get_level(WS_241_PWR_BTN_GPIO) == 0) {
                        vTaskDelay(POLL_DELAY);
                    }
                    
                    // Configure Wakeup
                    ESP_LOGI(TAG, "Entering Light Sleep...");
                    
                    // Enable GPIO15 as wake source (LO Level - press to wake)
                    gpio_wakeup_enable(WS_241_PWR_BTN_GPIO, GPIO_INTR_LOW_LEVEL);
                    esp_sleep_enable_gpio_wakeup();

                    // CRITICAL: Hold Power Latch (GPIO16) HIGH during sleep
                    // Prevents PMIC from cutting power when digital domain power is gated
                    gpio_hold_en(WS_241_PWR_LATCH_GPIO);
                    
                    // Enter Sleep
                    uart_wait_tx_idle_polling(0);
                    esp_light_sleep_start();
                    
                    // WAKE UP
                    ESP_LOGI(TAG, "Woke up from Light Sleep!");

                    // Release the hold on GPIO16 so we can control it normally again
                    gpio_hold_dis(WS_241_PWR_LATCH_GPIO);

                    // Re-Enable Display Power
                    ws_241_hal_set_display_power(true);
                    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for power stable
                    rm690b0_init(&g_disp_conf);     // Re-Init Display Driver

                    // VISUAL CONFIRMATION: Fill screen Green to indicate Wake
                    rm690b0_fill_screen(0x07E0); // Bright Green
                    vTaskDelay(pdMS_TO_TICKS(1500)); // Hold for 1.5 seconds

                    // Restore Test Pattern (Boxes)
                    ESP_LOGI(TAG, "Restoring Test Pattern...");
                    rm690b0_run_test_pattern();

                    // Debounce after wake

                    vTaskDelay(pdMS_TO_TICKS(500));
                    break; 
                }
            }
        }
        vTaskDelay(POLL_DELAY);
    }
}

static void boot_button_click_cb(void *arg, void *usr_data) {
    uint8_t r = rm690b0_get_rotation();

    // User confirmed Rotation 1 is CCW.
    // So we want the sequence 0 -> 1 -> 2 -> 3.
    // This is simply (r + 1) % 4.
    r = (r + 1) % 4;

    ESP_LOGI(TAG, "Boot Button Pressed: Rotating Screen to %d", r);
    rm690b0_set_rotation(r);
    ft6336u_set_rotation(r);
    // Explicitly clear screen to remove artifacts from previous orientation
    rm690b0_fill_screen(RM_COLOR_BLACK);
    rm690b0_run_test_pattern();
}

static adc_oneshot_unit_handle_t g_adc_handle = NULL;
static const adc_channel_t g_adc_channel = ADC_CHANNEL_7; // GPIO18 is ADC2_CH7

static i2c_master_bus_handle_t g_i2c_bus_handle = NULL;

static esp_err_t i2c_bus_init(void) {
    if (g_i2c_bus_handle != NULL) return ESP_OK;

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = WS_241_I2C_HOST,
        .scl_io_num = WS_241_I2C_SCL,
        .sda_io_num = WS_241_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&i2c_mst_config, &g_i2c_bus_handle);
}

static esp_err_t spi_bus_init(void) {
    spi_bus_config_t buscfg = {
        .data0_io_num = WS_241_QSPI_D0,
        .data1_io_num = WS_241_QSPI_D1,
        .sclk_io_num = WS_241_QSPI_CLK,
        .data2_io_num = WS_241_QSPI_D2,
        .data3_io_num = WS_241_QSPI_D3,
        .max_transfer_sz = 450 * 600 * 2, // Full frame buffer size possible
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };
    return spi_bus_initialize(WS_241_QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
}

esp_err_t ws_241_hal_get_battery_voltage(uint32_t *voltage_mv) {
    int adc_raw;
    if (g_adc_handle == NULL) return ESP_ERR_INVALID_STATE;
    
    // Attempt ADC Read
    if (adc_oneshot_read(g_adc_handle, g_adc_channel, &adc_raw) != ESP_OK) {
         return ESP_FAIL;
    }
    
    // Standard formula: V = raw * LSB_SIZE * DIVIDER_RATIO
    // Calibration based on user measurement:
    // Code reported ~1025mV when Actual was 4150mV.
    // Factor = 4150 / 1025 = ~4.05
    // Previous multiplier was 2. New multiplier should be ~8.1 (2 * 4.05).
    // Let's use 8.1 as the multiplier.
    // voltage_mv = adc_raw * 3300 * 8.1 / 4095
    // Simplified: voltage_mv = adc_raw * 26730 / 4095
    // Let's keep it readable:
    *voltage_mv = (uint32_t)adc_raw * 3300 * 81 / (4095 * 10); 
    
    return ESP_OK;
}

static void battery_monitor_task(void *pvParameters) {
    uint32_t voltage = 0;
    const TickType_t delay_15_mins = pdMS_TO_TICKS(15 * 60 * 1000);

    while (1) {
        if (ws_241_hal_get_battery_voltage(&voltage) == ESP_OK) {
            ESP_LOGI(TAG, "Battery Voltage: %d mV", (int)voltage);
        } else {
            ESP_LOGE(TAG, "Failed to read battery voltage");
        }
        vTaskDelay(delay_15_mins);
    }
}

static esp_err_t adc_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &g_adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    return adc_oneshot_config_channel(g_adc_handle, g_adc_channel, &config);
}

esp_err_t ws_241_hal_init(void) {
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing Hardware Abstraction Layer...");

    // 0. Hold Power On (Latch)
    // GPIO16 controls the system power latch. Must be held HIGH to keep system running.
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << WS_241_PWR_LATCH_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_conf);
    gpio_set_level(WS_241_PWR_LATCH_GPIO, 1);
    
    // Explicitly configure GPIO16 to stay HIGH during Light Sleep
    // Note: gpio_sleep_set_level is defunct in newer ESP-IDF for S3? 
    // Actually, it should be available via "driver/gpio.h" but might be hidden.
    // However, esp_sleep_enable_gpio_switch(true) allows state persistence.
    // Let's try gpio_hold_en() if consistent persistence is needed, although that's for Deep Sleep pads.
    // Valid Light Sleep retention: 
    // gpio_set_level(16, 1); (done above)
    // + default config retains state.
    // If we want to use the dedicated API:
#if SOC_GPIO_SUPPORT_SLP_SWITCH
    gpio_sleep_set_direction(WS_241_PWR_LATCH_GPIO, GPIO_MODE_OUTPUT);
    gpio_sleep_set_level(WS_241_PWR_LATCH_GPIO, 1);
#endif 
    
    ESP_LOGI(TAG, "Power Latch (GPIO16) set HIGH");

    // Initialize Power Button as Input (Active Low)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << WS_241_PWR_BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Pull up just in case
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);

    // 1. Initialize I2C Bus
    ret = i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C Init Failed");
        return ret;
    }
    ESP_LOGI(TAG, "I2C Initialized");

    // Initialize FT6336U Touch Controller
    ft6336u_init(g_i2c_bus_handle);

    // 2. Initialize TCA9554 (IO Expander)
    tca9554_init(g_i2c_bus_handle); 

    // 3. Enable Display Power
    ESP_LOGI(TAG, "Enabling Display Power via TCA9554...");
    // Configure PWR_EN as Output using tca9554 driver
    tca9554_set_direction(TCA_PIN_PWR_EN, TCA_OUTPUT);
    tca9554_set_level(TCA_PIN_PWR_EN, 1);
    
    // Wait for power to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // 4. Initialize SPI Bus for QSPI
    ESP_LOGI(TAG, "Initializing SPI Bus...");
    ret = spi_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI Bus Init Failed");
        return ret;
    }

    // 5. Initialize Display Driver (RM690B0)
    ESP_LOGI(TAG, "Initializing RM690B0 Display...");
    ret = rm690b0_init(&g_disp_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display Init Failed");
        return ret;
    }

    // 6. Initialize ADC for Battery Monitoring
    ESP_LOGI(TAG, "Initializing ADC...");
    adc_init();

    // 7. Start Battery Monitor Task (Every 15 Minutes)
    xTaskCreate(battery_monitor_task, "bat_mon", 2048, NULL, 5, NULL);

    // 8. Start Power Button Task
    xTaskCreate(power_button_task, "pwr_btn", 4096, NULL, 5, NULL);

    // 9. Initialize Boot Button (GPIO 0) for Screen Rotation
    button_config_t btn_cfg = {
        .long_press_time = 1500,
        .short_press_time = 100,
    };
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BOOT_BUTTON_GPIO,
        .active_level = 0,
    };
    
    button_handle_t btn_handle = NULL;
    esp_err_t err = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn_handle);

    if (err == ESP_OK && btn_handle) {
        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, boot_button_click_cb, NULL);
        ESP_LOGI(TAG, "Boot Button (GPIO0) Initialized for Rotation Control");
    } else {
        ESP_LOGE(TAG, "Failed to init Boot Button");
    }

    ESP_LOGI(TAG, "HAL Initialization Complete");
    return ESP_OK;
}

void ws_241_hal_touch_test_task(void *pvParameters) {
    uint16_t x = 0, y = 0;
    const uint16_t BRUSH_SIZE = 4;
    
    ESP_LOGI(TAG, "Touch Test Task Started. Draw on screen!");

    while (1) {
        if (ft6336u_get_touch(&x, &y)) {
            // Visualize touch with a small filled square
            // Center the brush on the touch point
            uint16_t px = (x > BRUSH_SIZE/2) ? (x - BRUSH_SIZE/2) : 0;
            uint16_t py = (y > BRUSH_SIZE/2) ? (y - BRUSH_SIZE/2) : 0;
            
            // Draw
            rm690b0_draw_rect(px, py, BRUSH_SIZE, BRUSH_SIZE, RM_COLOR_CYAN);
            
            // Log occasionally to avoid spam, or just remove for performance
            // ESP_LOGI(TAG, "Touch: %d, %d", x, y);
        }
        // Small delay to allow other tasks to run (Yield)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ws_241_hal_start_touch_test(void) {
    xTaskCreate(ws_241_hal_touch_test_task, "touch_test", 4096, NULL, 5, NULL);
}


const rm690b0_config_t* ws_241_hal_get_display_config(void) {
    return &g_disp_conf;
}

esp_err_t ws_241_hal_set_display_power(bool enable) {
    return tca9554_set_level(TCA_PIN_PWR_EN, enable ? 1 : 0);
}

int ws_241_hal_get_te_signal(void) {
    int level = 0;
    tca9554_get_level(TCA_PIN_TE, &level);
    return level;
}

void ws_241_hal_power_off(void) {
    ESP_LOGI(TAG, "Powering Down System...");
    // Release the Power Latch (GPIO16 Low)
    // If the power button is not held, this should cut system power.
    gpio_set_level(WS_241_PWR_LATCH_GPIO, 0);
    
    // Just in case power remains (USB connected?), loop forever
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Waiting for power off...");
    }
}
