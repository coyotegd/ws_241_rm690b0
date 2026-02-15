#ifndef RM690B0_H
#define RM690B0_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// Screen Dimensions
#define RM690B0_WIDTH     450
#define RM690B0_HEIGHT    600

// Colors (RGB565)
#define RM_COLOR_RED        0xF800
#define RM_COLOR_GREEN      0x07E0
#define RM_COLOR_BLUE       0x001F
#define RM_COLOR_WHITE      0xFFFF
#define RM_COLOR_BLACK      0x0000
#define RM_COLOR_YELLOW     0xFFE0
#define RM_COLOR_DARK_GREY  0x4208
#define RM_COLOR_CYAN       0x07FF
#define RM_COLOR_MAGENTA    0xF81F

typedef struct {
    int cs_io;
    int clk_io;
    int d0_io; // Data 0
    int d1_io; // Data 1
    int d2_io; // Data 2 (WP)
    int d3_io; // Data 3 (HOLD)
    int rst_io;
    int te_io;
    spi_host_device_t host_id;
} rm690b0_config_t;

/**
 * @brief Initialize the RM690B0 display driver.
 * Assumes SPI bus is already initialized.
 * @param config Configuration struct with pin assignments
 * @return ESP_OK on success
 */
esp_err_t rm690b0_init(const rm690b0_config_t *config);

/**
 * @brief Set the active window for drawing
 */
esp_err_t rm690b0_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief Write raw pixel data to the display
 */
esp_err_t rm690b0_write_pixels(const uint16_t *data, size_t pixel_count);

/**
 * @brief Fill the screen with a solid color
 */
esp_err_t rm690b0_fill_screen(uint16_t color);

/**
 * @brief Draw a rectangle outline
 */
void rm690b0_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Set display rotation (MADCTL)
 * @param rotation 0: Portrait (USB Bottom), 1: Landscape (USB Right), 2: Inv Port, 3: Inv Land
 */
void rm690b0_set_rotation(uint8_t rotation);

/**
 * @brief Get the current display rotation
 * @return rotation 0-3
 */
uint8_t rm690b0_get_rotation(void);

/**
 * @brief Run the built-in test pattern sequence (blocking)
 */
void rm690b0_run_test_pattern(void);

#ifdef __cplusplus
}
#endif

#endif
