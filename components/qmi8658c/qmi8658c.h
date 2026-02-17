#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QMI8658C_I2C_ADDR 0x6B
#define QMI8658C_WHO_AM_I 0x05 // Default Who Am I value

typedef struct {
    float x;
    float y;
    float z;
} qmi_data_axis_t;

typedef struct {
    qmi_data_axis_t acc;
    qmi_data_axis_t gyro;
    float temperature;
} qmi_data_t;

/**
 * @brief Initialize the QMI8658C 6-axis IMU
 * @param bus_handle Handle to the I2C master bus
 * @return ESP_OK on success
 */
esp_err_t qmi8658c_init(i2c_master_bus_handle_t bus_handle);

/**
 * @brief Read accelerometer and gyroscope data
 * @param[out] data Pointer to data structure to fill
 * @return ESP_OK on success
 */
esp_err_t qmi8658c_read_data(qmi_data_t *data);

#ifdef __cplusplus
}
#endif
