#include "qmi8658c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "QMI8658C";
static i2c_master_dev_handle_t g_qmi_dev = NULL;

// Registers
#define REG_WHO_AM_I 0x00
#define REG_CTRL1    0x02
#define REG_CTRL2    0x03
#define REG_CTRL3    0x04
#define REG_CTRL5    0x06
#define REG_CTRL7    0x08
#define REG_AX_L     0x35
#define REG_TEMP_L   0x33

esp_err_t qmi8658c_init(i2c_master_bus_handle_t bus_handle) {
    if (g_qmi_dev != NULL) return ESP_OK;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658C_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_qmi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to bus");
        return ret;
    }

    // Check WHO_AM_I
    uint8_t id = 0;
    uint8_t reg = REG_WHO_AM_I;
    ret = i2c_master_transmit_receive(g_qmi_dev, &reg, 1, &id, 1, -1);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device found. ID: 0x%02X", id);
        if (id != QMI8658C_WHO_AM_I) {
           ESP_LOGW(TAG, "Unexpected Device ID (Expected 0x%02X)", QMI8658C_WHO_AM_I);
        }
    } else {
        ESP_LOGE(TAG, "Failed to communicate with device");
        return ret;
    }

    // Configure Device
    // 1. CTRL1: 0x60 -> Address Auto-Increment + Serial Interface Mode
    // 2. CTRL2: ACC Config -> 2g Scale, 1000Hz ODR
    // 3. CTRL3: GYRO Config -> 512dps Scale, 1000Hz ODR
    // 4. CTRL7: Enable Sensors -> Enable Accel + Gyro
    
    // Scale Definitions:
    // Acc 2g = 16384 LSB/g
    // Gyro 512dps = 64 LSB/dps
    
    uint8_t config_data[][2] = {
        {REG_CTRL1, 0x60}, 
        {REG_CTRL2, 0x03}, // Acc: 2g (Bits 6:4=000), 1000Hz (Bits 3:0=0011=3)
        {REG_CTRL3, 0x53}, // Gyro: 512dps (Bits 6:4=101=5), 1000Hz (Bits 3:0=0011=3)
        {REG_CTRL7, 0x03}, // Enable Accel & Gyro
    };
    
    for (int i = 0; i < 4; i++) {
        ret = i2c_master_transmit(g_qmi_dev, config_data[i], 2, -1);
        if (ret != ESP_OK) return ret;
    }
    
    ESP_LOGI(TAG, "Initialized: Acc 2g/1kHz, Gyro 512dps/1kHz");
    return ESP_OK;
}

esp_err_t qmi8658c_read_data(qmi_data_t *data) {
    if (g_qmi_dev == NULL) return ESP_ERR_INVALID_STATE;

    uint8_t reg = REG_AX_L;
    uint8_t raw_data[12];
    
    // Read 12 bytes
    esp_err_t ret = i2c_master_transmit_receive(g_qmi_dev, &reg, 1, raw_data, 12, -1);
    if (ret != ESP_OK) return ret;

    int16_t ax = (raw_data[1] << 8) | raw_data[0];
    int16_t ay = (raw_data[3] << 8) | raw_data[2];
    int16_t az = (raw_data[5] << 8) | raw_data[4];
    int16_t gx = (raw_data[7] << 8) | raw_data[6];
    int16_t gy = (raw_data[9] << 8) | raw_data[8];
    int16_t gz = (raw_data[11] << 8) | raw_data[10];

    // Conversion Factors
    // Acc Range 2g: 1g = 16384 LSB. Output in m/s^2 (1g = 9.80665 m/s^2)
    // Factor = 9.80665 / 16384.0 ~= 0.00059855
    const float acc_scale = 9.80665f / 16384.0f;
    
    // Gyro Range 512dps: 1dps = 64 LSB. Output in dps
    // Factor = 1.0 / 64.0 = 0.015625
    const float gyro_scale = 1.0f / 64.0f;

    data->acc.x = ax * acc_scale;
    data->acc.y = ay * acc_scale;
    data->acc.z = az * acc_scale;
    
    data->gyro.x = gx * gyro_scale;
    data->gyro.y = gy * gyro_scale;
    data->gyro.z = gz * gyro_scale;

    return ESP_OK;
}
