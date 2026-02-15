#pragma once

#include "esp_err.h"
#include "rm690b0.h"
#include "tca9554.h"
#include "qmi8658c.h"
#include "pcf85063a.h"
#include "ft6336u.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
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

#ifdef __cplusplus
}
#endif
