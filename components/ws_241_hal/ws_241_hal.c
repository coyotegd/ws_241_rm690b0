#include "ws_241_hal.h"
#include "rm690b0.h"
#include "tca9554.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

static esp_err_t i2c_bus_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WS_241_I2C_SDA,
        .scl_io_num = WS_241_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = WS_241_I2C_FREQ_HZ,
    };
    return i2c_param_config(WS_241_I2C_HOST, &conf) | i2c_driver_install(WS_241_I2C_HOST, conf.mode, 0, 0, 0);
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

esp_err_t ws_241_hal_init(void) {
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing Hardware Abstraction Layer...");

    // 1. Initialize I2C Bus
    ret = i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C Init Failed");
        return ret;
    }
    ESP_LOGI(TAG, "I2C Initialized");

    // 2. Initialize TCA9554 (IO Expander)
    tca9554_init(WS_241_I2C_HOST); 

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

    ESP_LOGI(TAG, "HAL Initialization Complete");
    return ESP_OK;
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
