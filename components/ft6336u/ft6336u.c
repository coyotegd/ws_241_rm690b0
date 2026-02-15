#include "ft6336u.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "FT6336U";
static i2c_master_dev_handle_t g_dev_handle = NULL;
static uint8_t g_rotation = 0;

// Registers
#define FT6336U_REG_MODE        0x00
#define FT6336U_REG_GESTURE     0x01
#define FT6336U_REG_TOUCH_CNT   0x02
#define FT6336U_REG_P1_XH       0x03
#define FT6336U_REG_P1_XL       0x04
#define FT6336U_REG_P1_YH       0x05
#define FT6336U_REG_P1_YL       0x06

// Screen Dimensions (Native Portrait)
#define FT6336U_WIDTH_NATIVE  450
#define FT6336U_HEIGHT_NATIVE 600

esp_err_t ft6336u_init(i2c_master_bus_handle_t bus_handle) {
    if (g_dev_handle != NULL) return ESP_OK;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6336U_I2C_ADDRESS,
        .scl_speed_hz = 100000, // 400kHz might be too fast for some clones, try 100 first
    };
    // Revert to 400k if needed.

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add FT6336U to I2C bus");
        return ret;
    }
    
    // Simple probe: Read Chip ID (Register 0xA8)
    uint8_t chip_id = 0;
    uint8_t cmd = 0xA8;
    
    // Using transmit_receive to read register
    ret = i2c_master_transmit_receive(g_dev_handle, &cmd, 1, &chip_id, 1, -1);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Initialized FT6336U. Chip ID: 0x%02X", chip_id);
    } else {
        ESP_LOGE(TAG, "Failed to detect FT6336U at 0x%02X", FT6336U_I2C_ADDRESS);
        // Don't fail completely, maybe it's asleep?
    }
    
    return ESP_OK;
}

void ft6336u_set_rotation(uint8_t rotation) {
    g_rotation = rotation & 0x03; // Limit to 0-3
    ESP_LOGI(TAG, "Touch Rotation set to %d", g_rotation);
}

// Map raw coordinates to screen coordinates based on rotation
// Assumes Hardware Native is Portrait (Rot 3 logic)
static void apply_rotation(uint16_t *x, uint16_t *y) {
    uint16_t raw_x = *x;
    uint16_t raw_y = *y;
    
    // RM690B0 Orientations for FT6336U Coordinate Mapping
    // Native (Rot 3 = Portrait USB Left): X(0-450), Y(0-600)

    switch (g_rotation) {
        case 3: // Native (USB Left) -> No Change
            *x = raw_x;
            *y = raw_y;
            break;

        case 0: // Landscape (USB Bottom) - 90 CW
            // Old (0,0) Top-Left -> New Top-Right
            // Old (0,600) Bottom-Left -> New Top-Left (0,0)
            // Old (450,0) Top-Right -> New Bottom-Right
            // Old (450,600) Bottom-Right -> New Bottom-Left
            
            // Map: (0,600) -> (0,0)
            // New X = 600 - Old Y 
            // Map: (0,0) -> (600,0)
            // New Y = Old X
            *x = FT6336U_HEIGHT_NATIVE - raw_y;
            *y = raw_x;
            break;

        case 1: // Portrait (USB Right) - 180 CW from Native
            // Old (0,0) -> New Bottom-Right (450, 600)
            // New X = 450 - Old X
            // New Y = 600 - Old Y
            *x = FT6336U_WIDTH_NATIVE - raw_x;
            *y = FT6336U_HEIGHT_NATIVE - raw_y;
            break;

        case 2: // Landscape (USB Top) - 270 CW from Native (-90)
            // Old (0,0) -> New Bottom-Left (0, 450)
            // Old (450,0) -> New Top-Left (0,0)
            // New Y = 450 - Old X
            // New X = Old Y
            *x = raw_y;
            *y = FT6336U_WIDTH_NATIVE - raw_x;
            break;
    }
}

bool ft6336u_get_touch(uint16_t *x, uint16_t *y) {
    if (g_dev_handle == NULL) return false;

    uint8_t data[6];
    uint8_t reg = FT6336U_REG_TOUCH_CNT; // Start reading from 0x02

    esp_err_t ret = i2c_master_transmit_receive(g_dev_handle, &reg, 1, data, 5, -1);
                                                 
    if (ret != ESP_OK) return false;
    
    uint8_t touch_cnt = data[0] & 0x0F;
    if (touch_cnt == 0 || touch_cnt > 2) return false; // Usually 0-2
    
    // Parse Point 1
    // X High: Bits 3:0 of 0x03. Bits 7:6 are Event Flag.
    uint16_t X = ((data[1] & 0x0F) << 8) | data[2];
    uint16_t Y = ((data[3] & 0x0F) << 8) | data[4];
    
    *x = X;
    *y = Y;
    
    apply_rotation(x, y);
    
    return true;
}
