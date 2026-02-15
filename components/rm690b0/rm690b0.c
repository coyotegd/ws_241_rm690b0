#include <stddef.h>
#include "rm690b0.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_attr.h"
#include <string.h>

static const char *TAG = "rm690b0";

// --- Internal Global Config ---
static rm690b0_config_t g_conf;
static spi_device_handle_t spi_handle;

// --- Internal Transaction buffers ---
DRAM_ATTR static uint8_t s_caset_data[4];
DRAM_ATTR static uint8_t s_raset_data[4];

// Current display dimensions 
static uint16_t current_width = RM690B0_WIDTH;
static uint16_t current_height = RM690B0_HEIGHT;
static uint16_t offset_x = 0;
static uint16_t offset_y = 0;
static uint8_t s_rotation = 0;

// Helper: Send Command (Variable CMD/ADDR phases for QSPI wrapper)
static void rm_send_cmd(uint8_t cmd, const uint8_t *data, size_t len) {
    // Acquire bus to ensure atomic command sequence if needed
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);

    spi_transaction_ext_t t = {0};
    t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR; // The QSPI wrapper uses this to send "02 00 CMD 00"
    
    // Command Phase: 1 byte Opcode (0x02)
    t.base.cmd = 0x02;
    t.command_bits = 8;

    // Address Phase: 3 bytes (00 CMD 00) -> 24 bits
    t.base.addr = ((uint32_t)cmd) << 8; 
    t.address_bits = 24;

    t.base.length = len * 8;
    t.base.tx_buffer = data;

    esp_err_t ret = spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI Transfer Error: %s", esp_err_to_name(ret));
    }
    
    spi_device_release_bus(spi_handle);
}

// Send rotation command
void rm690b0_set_rotation(uint8_t rotation) {
    uint8_t madctl = 0;
    s_rotation = rotation;
    switch (rotation) {
        case 0: // Landscape (USB Bottom) - Default
            madctl = 0xA0; // MV, MY
            current_width = 600;
            current_height = 450;
            offset_x = 0; 
            offset_y = 16; 
            break;

        case 1: // Portrait (USB Right) - CCW 90
            madctl = 0xC0; // MY, MX
            current_width = 450; 
            current_height = 600;
            offset_x = 14; 
            offset_y = 0; 
            break;
            
        case 2: // Landscape (USB Top) - CCW 180
            madctl = 0x60; // MV, MX
            current_width = 600;
            current_height = 450;
            offset_x = 0; 
            offset_y = 14;
            break;
            
        case 3: // Portrait (USB Left) - CCW 270
            madctl = 0x00; // Native
            current_width = 450;
            current_height = 600;
            offset_x = 16; 
            offset_y = 0;
            break;
    }
    ESP_LOGI(TAG, "Set Rotation %d: %dx%d (OffX:%d OffY:%d)", rotation, current_width, current_height, offset_x, offset_y);
    rm_send_cmd(0x36, &madctl, 1);
}

uint8_t rm690b0_get_rotation(void) {
    return s_rotation;
}

esp_err_t rm690b0_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    // Apply Rotation Offsets
    x1 += offset_x;
    x2 += offset_x;
    y1 += offset_y;
    y2 += offset_y;

    // Align X to even (QSPI requirement often)
    x1 &= ~1;
    x2 |= 1;

    s_caset_data[0] = (x1 >> 8); s_caset_data[1] = (x1 & 0xFF);
    s_caset_data[2] = (x2 >> 8); s_caset_data[3] = (x2 & 0xFF);
    
    s_raset_data[0] = (y1 >> 8); s_raset_data[1] = (y1 & 0xFF);
    s_raset_data[2] = (y2 >> 8); s_raset_data[3] = (y2 & 0xFF);

    rm_send_cmd(0x2A, s_caset_data, 4); // CASET
    rm_send_cmd(0x2B, s_raset_data, 4); // RASET
    vTaskDelay(pdMS_TO_TICKS(1)); 
    return ESP_OK;
}


esp_err_t rm690b0_write_pixels(const uint16_t *data, size_t pixel_count) {
    if (pixel_count == 0) return ESP_OK;

    size_t len_bytes = pixel_count * 2;
    const size_t CHUNK_SIZE = 32 * 1024; 
    size_t sent = 0;
    
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    
    while (sent < len_bytes) {
        size_t chunk = (len_bytes - sent > CHUNK_SIZE) ? CHUNK_SIZE : (len_bytes - sent);
        
        spi_transaction_ext_t t = {0};
        t.base.flags = SPI_TRANS_MODE_QIO;

        if (sent == 0) {
            // First chunk: Send Write RAM command header
            // 0x32 00 2C 00
            t.base.flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
            t.base.cmd = 0x32;
            t.base.addr = 0x002C00;
            t.command_bits = 8;
            t.address_bits = 24;
        } else {
            // Subsequent chunks: Just data, but keep CS active from previous
            t.command_bits = 0;
            t.address_bits = 0;
            t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE; 
        }

        if (sent + chunk < len_bytes) {
            t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE; 
        }
        
        t.base.length = chunk * 8;
        t.base.tx_buffer = ((uint8_t*)data) + sent;

        spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
        sent += chunk;
    }
    spi_device_release_bus(spi_handle);
    return ESP_OK;
}

esp_err_t rm690b0_init(const rm690b0_config_t *config) {
    g_conf = *config;
    
    ESP_LOGI(TAG, "Initializing RM690B0 (LilyGo logic port)...");

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 60 * 1000 * 1000, 
        .mode = 0,
        .spics_io_num = config->cs_io,
        .queue_size = 10,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    // Safe fallback to 40MHz
    devcfg.clock_speed_hz = 40 * 1000 * 1000; 

    esp_err_t ret = spi_bus_add_device(config->host_id, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return ret;
    }

    // 2. Hardware Reset
    if (config->rst_io >= 0) {
        gpio_reset_pin(config->rst_io);
        gpio_set_direction(config->rst_io, GPIO_MODE_OUTPUT);
        
        gpio_set_level(config->rst_io, 1);
        vTaskDelay(pdMS_TO_TICKS(20)); // LilyGo: 200
        gpio_set_level(config->rst_io, 0);
        vTaskDelay(pdMS_TO_TICKS(100)); // LilyGo: 300
        gpio_set_level(config->rst_io, 1);
        vTaskDelay(pdMS_TO_TICKS(100)); // LilyGo: 200
    }

    // 3. Init Commands
    rm_send_cmd(0xFE, (uint8_t[]){0x20}, 1);
    rm_send_cmd(0x26, (uint8_t[]){0x0A}, 1);
    rm_send_cmd(0x24, (uint8_t[]){0x80}, 1);
    rm_send_cmd(0x5A, (uint8_t[]){0x51}, 1);
    rm_send_cmd(0x5B, (uint8_t[]){0x2E}, 1);
    rm_send_cmd(0xFE, (uint8_t[]){0x00}, 1);
    
    rm_send_cmd(0x3A, (uint8_t[]){0x55}, 1); // COLMOD 16bit
    
    rm_send_cmd(0xC2, NULL, 0); 
    vTaskDelay(pdMS_TO_TICKS(10));
    
    rm_send_cmd(0x35, (uint8_t[]){0x00}, 1); // TE ON
    
    rm_send_cmd(0x51, (uint8_t[]){0x00}, 1); // Brightness 0
    
    rm_send_cmd(0x11, NULL, 0); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));

    // Force default rotation to match software state or reset it
    // If not set, mismatch between s_rotation and HW state can occur
    rm690b0_set_rotation(0); 

    rm_send_cmd(0x29, NULL, 0); // Display On
    vTaskDelay(pdMS_TO_TICKS(120));
    
    rm_send_cmd(0x51, (uint8_t[]){0xFF}, 1); // Brightness Max
    
    ESP_LOGI(TAG, "RM690B0 Init Complete.");
    return ESP_OK;
}

esp_err_t rm690b0_fill_screen(uint16_t color) {
    rm690b0_set_window(0, 0, current_width - 1, current_height - 1);
    
    size_t pixel_count = current_width * current_height;
    size_t chunk_pixels = 8192; // 16KB buffer
    
    // Allocate buffer in DMA capable memory
    uint16_t *buffer = heap_caps_malloc(chunk_pixels * 2, MALLOC_CAP_DMA);
    if (!buffer) {
        ESP_LOGE(TAG, "OOM in fill_screen");
        return ESP_ERR_NO_MEM;
    }
    
    uint16_t color_be = (color >> 8) | (color << 8);
    for (size_t i = 0; i < chunk_pixels; i++) buffer[i] = color_be;
    
    size_t sent = 0;
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    
    while (sent < pixel_count) {
        size_t pixels_to_send = (pixel_count - sent > chunk_pixels) ? chunk_pixels : (pixel_count - sent);
        
        spi_transaction_ext_t t = {0};
        t.base.flags = SPI_TRANS_MODE_QIO;
        
        if (sent == 0) {
            t.base.flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
            t.base.cmd = 0x32; // WRRAM
            t.base.addr = 0x002C00;
            t.command_bits = 8;
            t.address_bits = 24;
        } else {
            t.base.flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
            t.command_bits = 0;
            t.address_bits = 0;
        }
        
        if (sent + pixels_to_send < pixel_count) {
             t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
        }
        
        t.base.length = pixels_to_send * 16; 
        t.base.tx_buffer = buffer;
        
        spi_device_polling_transmit(spi_handle, (spi_transaction_t *)&t);
        sent += pixels_to_send;
    }
    
    spi_device_release_bus(spi_handle);
    free(buffer);
    vTaskDelay(pdMS_TO_TICKS(10)); // Allow controller to recover after massive write
    return ESP_OK;
}

void rm690b0_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    rm690b0_set_window(x, y, x + w - 1, y + h - 1);
    
    size_t count = w * h;
    size_t chunk_max = 4096;
    uint16_t *buf = heap_caps_malloc(chunk_max * 2, MALLOC_CAP_DMA);
    if (!buf) return;
    
    uint16_t c = (color >> 8) | (color << 8);
    for(int i=0; i<chunk_max; i++) buf[i] = c;
    
    size_t sent = 0;
    spi_device_acquire_bus(spi_handle, portMAX_DELAY);
    
    while(sent < count) {
        size_t n = (count - sent > chunk_max) ? chunk_max : (count - sent);
        
        spi_transaction_ext_t t = {0};
        t.base.flags = SPI_TRANS_MODE_QIO;
        
        if (sent == 0) {
            t.base.flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
            t.base.cmd = 0x32;
            t.base.addr = 0x002C00;
            t.command_bits = 8;
            t.address_bits = 24;
        } else {
            t.base.flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
            t.command_bits = 0;
            t.address_bits = 0;
        }
        
        if (sent + n < count) t.base.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
        
        t.base.length = n * 16;
        t.base.tx_buffer = buf;
        spi_device_polling_transmit(spi_handle, (spi_transaction_t*)&t);
        
        sent += n;
    }
    spi_device_release_bus(spi_handle);
    free(buf);
}

void rm690b0_run_test_pattern(void) {
    ESP_LOGI(TAG, "Running Test Pattern (LilyGo Logic)");

    // Test in current Rotation
    // rm690b0_set_rotation(1); 
    
    // Clear display black first
    rm690b0_fill_screen(RM_COLOR_BLACK);
    
    // Top-Left RED
    rm690b0_draw_rect(0, 0, 50, 50, RM_COLOR_RED);
    
    // Top-Right GREEN
    rm690b0_draw_rect(current_width - 50, 0, 50, 50, RM_COLOR_GREEN);
    
    // Bottom-Right BLUE
    rm690b0_draw_rect(current_width - 50, current_height - 50, 50, 50, RM_COLOR_BLUE);
    
    // Bottom-Left WHITE
    rm690b0_draw_rect(0, current_height - 50, 50, 50, RM_COLOR_WHITE);
    
    // Center YELLOW
    rm690b0_draw_rect((current_width/2)-25, (current_height/2)-25, 50, 50, RM_COLOR_YELLOW);

    ESP_LOGI(TAG, "Test Pattern Drawn: Corners + Center");
}
