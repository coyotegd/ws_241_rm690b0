#include "tca9554.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "TCA9554";
static i2c_master_dev_handle_t g_i2c_dev = NULL;

#define REG_INPUT  0x00
#define REG_OUTPUT 0x01
#define REG_POL    0x02
#define REG_CONFIG 0x03

esp_err_t tca9554_init(i2c_master_bus_handle_t bus_handle) {
    if (g_i2c_dev != NULL) {
        return ESP_OK; // Already initialized
    }
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TCA9554_ADDR,
        .scl_speed_hz = 100000, // TCA9554 supports up to 400k, safe default
    };
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to I2C bus");
        return ret;
    }
    
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(g_i2c_dev, data, 2, -1);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(g_i2c_dev, &reg, 1, val, 1, -1);
}

esp_err_t tca9554_set_direction(uint8_t pin_mask, uint8_t mode) {
    uint8_t conf;
    esp_err_t ret = read_reg(REG_CONFIG, &conf);
    if (ret != ESP_OK) return ret;
    
    if (mode == TCA_INPUT) {
        conf |= pin_mask; // Set bit for input
    } else {
        conf &= ~pin_mask; // Clear bit for output
    }
    
    return write_reg(REG_CONFIG, conf);
}

esp_err_t tca9554_set_level(uint8_t pin_mask, uint8_t level) {
    uint8_t out_reg;
    esp_err_t ret = read_reg(REG_OUTPUT, &out_reg);
    if (ret != ESP_OK) return ret;

    if (level) {
        out_reg |= pin_mask;
    } else {
        out_reg &= ~pin_mask;
    }

    return write_reg(REG_OUTPUT, out_reg);
}

esp_err_t tca9554_get_level(uint8_t pin_mask, int *level) {
    uint8_t in_reg;
    esp_err_t ret = read_reg(REG_INPUT, &in_reg);
    if (ret != ESP_OK) return ret;

    *level = (in_reg & pin_mask) ? 1 : 0;
    return ESP_OK;
}
