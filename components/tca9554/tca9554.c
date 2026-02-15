#include "tca9554.h"
#include "esp_log.h"

static const char *TAG = "TCA9554";
static i2c_port_t g_i2c_num = I2C_NUM_0; 

#define REG_INPUT  0x00
#define REG_OUTPUT 0x01
#define REG_POL    0x02
#define REG_CONFIG 0x03

esp_err_t tca9554_init(i2c_port_t i2c_num) {
    g_i2c_num = i2c_num;
    ESP_LOGI(TAG, "Initialized with I2C host %d", i2c_num);
    return ESP_OK;
}

static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(g_i2c_num, TCA9554_ADDR, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_reg(uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(g_i2c_num, TCA9554_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
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
