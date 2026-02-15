#ifndef TCA9554_H
#define TCA9554_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// TCA9554 I2C Address (default for Waveshare 2.41, usually 0x20 w/ A0=A1=A2=GND)
#define TCA9554_ADDR 0x20

// Pin Directions
#define TCA_INPUT  1
#define TCA_OUTPUT 0

/**
 * Initialize the TCA9554 driver.
 * @param bus_handle I2C Master Bus Handle.
 * @return ESP_OK on success.
 */
esp_err_t tca9554_init(i2c_master_bus_handle_t bus_handle);

/**
 * Set the direction of a pin.
 * @param pin_mask Pin mask (1 << pin_number).
 * @param mode TCA_INPUT or TCA_OUTPUT.
 * @return ESP_OK on success.
 */
esp_err_t tca9554_set_direction(uint8_t pin_mask, uint8_t mode);

/**
 * Set the output level of a pin.
 * @param pin_mask Pin mask (1 << pin_number).
 * @param level 0 or 1.
 * @return ESP_OK on success.
 */
esp_err_t tca9554_set_level(uint8_t pin_mask, uint8_t level);

/**
 * Get the input level of a pin.
 * @param pin_mask Pin mask (1 << pin_number).
 * @param level Pointer to store level (0 or 1).
 * @return ESP_OK on success.
 */
esp_err_t tca9554_get_level(uint8_t pin_mask, int *level);

#ifdef __cplusplus
}
#endif

#endif // TCA9554_H

