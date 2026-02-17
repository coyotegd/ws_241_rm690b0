#pragma once

#include "esp_err.h"
#include "rm690b0.h"
#include "tca9554.h"
#include "qmi8658c.h"
#include "pcf85063a.h"
#include "ft6336u.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"


/**
 * @brief Start the touch test task (draw on screen)
 */
void ws_241_hal_start_touch_test(void);

/**
 * @brief Get the latest IMU data from QMI8658C
 * @param[out] data Pointer to qmi_data_t structure to fill
 * @return ESP_OK on success
 */
esp_err_t ws_241_hal_get_imu_data(qmi_data_t *data);

#ifdef __cplusplus
}
#endif

// --- Hardware Definitions ---

// I2C Bus (Touch, IMU, RTC, IO Expander)
#define WS_241_I2C_HOST         I2C_NUM_0
#define WS_241_I2C_SDA          47
#define WS_241_I2C_SCL          48
#define WS_241_I2C_FREQ_HZ      400000

// QSPI Bus (AMOLED Display)
#define WS_241_QSPI_HOST        SPI2_HOST
#define WS_241_QSPI_CS          9
#define WS_241_QSPI_CLK         10
#define WS_241_QSPI_D0          11
#define WS_241_QSPI_D1          12
#define WS_241_QSPI_D2          13
#define WS_241_QSPI_D3          14

// Display Control Pins (Direct GPIO)
#define WS_241_DISP_RST         21
// Note: Display TE (Tearing Effect) is connected to TCA9554 EXIO0

// IO Expander (TCA9554) Interrupt Pin (from TCA to ESP32)
#define WS_241_IO_EXP_INT       18

// Battery Voltage ADC Pin
#define WS_241_BAT_ADC_GPIO     18

// Power Management Pins
#define WS_241_PWR_LATCH_GPIO   16  // BAT_Control (Hold High to keep power on)
#define WS_241_PWR_BTN_GPIO     15  // Key_BAT (Button Input)

// --- Interface ---

/**
 * @brief Initialize all hardware components on the Waveshare 2.41 board.
 * 
 * This function handles the power-up sequence, bus initialization, and 
 * driver setup for:
 * 1. I2C Bus
 * 2. TCA9554 (IO Expander)
 * 3. Display Power (via TCA9554)
 * 4. QSPI Bus & RM690B0 Display
 * 5. FT6336U Touch Controller
 * 6. QMI8658C IMU
 * 7. PCF85063A RTC
 * 
 * @return ESP_OK on success, ESP_FAIL or other error codes on failure.
 */
esp_err_t ws_241_hal_init(void);

/**
 * @brief Get the RM690B0 display configuration used by HAL.
 * Useful if the application needs to re-init or access display details.
 */
const rm690b0_config_t* ws_241_hal_get_display_config(void);

/**
 * @brief Control Display Power via TCA9554.
 * @param enable true to turn ON, false to turn OFF.
 */
esp_err_t ws_241_hal_set_display_power(bool enable);

/**
 * @brief Get the Tearing Effect (TE) signal state from TCA9554.
 * @return Level of TE pin (0 or 1), or negative on error.
 */
int ws_241_hal_get_te_signal(void);

/**
 * @brief Turn off system power by releasing the latch (GPIO16).
 */
void ws_241_hal_power_off(void);

/**
 * @brief Get the current battery voltage in millivolts.
 * 
 * Uses the ADC on GPIO18 with a voltage divider.
 * 
 * @param[out] voltage_mv Pointer to store the voltage in mV
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ws_241_hal_get_battery_voltage(uint32_t *voltage_mv);

#ifdef __cplusplus
}
#endif
