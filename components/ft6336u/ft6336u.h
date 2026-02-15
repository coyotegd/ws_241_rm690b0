#ifndef FT6336U_H
#define FT6336U_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FT6336U_I2C_ADDRESS     0x38

/**
 * @brief Initialize the FT6336U touch controller
 * @param bus_handle I2C master bus handle
 * @return ESP_OK on success
 */
esp_err_t ft6336u_init(i2c_master_bus_handle_t bus_handle);

/**
 * @brief Get the current touch coordinates
 * @param x Pointer to store X coordinate
 * @param y Pointer to store Y coordinate
 * @return true if touched, false otherwise
 */
bool ft6336u_get_touch(uint16_t *x, uint16_t *y);

/**
 * @brief Set the rotation of the touch coordinates
 * @param rotation 0, 1, 2, or 3 (matches display rotation)
 */
void ft6336u_set_rotation(uint8_t rotation);

#ifdef __cplusplus
}
#endif

#endif // FT6336U_H
