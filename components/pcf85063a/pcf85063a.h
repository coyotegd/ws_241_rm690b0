#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default I2C Address for PCF85063A
#define PCF85063A_I2C_ADDR 0x51

/**
 * @brief Initialize the PCF85063A RTC
 * @param bus_handle Handle to the I2C master bus
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_init(i2c_master_bus_handle_t bus_handle);

/**
 * @brief Get the current time from the RTC
 * @param[out] timeinfo Pointer to struct tm to fill
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_get_time(struct tm *timeinfo);

/**
 * @brief Set the RTC time
 * @param timeinfo Pointer to struct tm containing the new time
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_set_time(const struct tm *timeinfo);

/**
 * @brief Print the current time to the log (Debug helper)
 */
void pcf85063a_test_read(void);


// --- Alarm Functions ---

typedef struct {
    int second;     // 0-59, or -1 to disable
    int minute;     // 0-59, or -1 to disable
    int hour;       // 0-23, or -1 to disable
    int day;        // 1-31, or -1 to disable
    int weekday;    // 0-6 (Sun-Sat), or -1 to disable
} pcf85063a_alarm_t;

/**
 * @brief Set the RTC Alarm
 * 
 * Triggers INT pin LOW when time matches. 
 * Use -1 for fields you want to ignore (e.g. set only minute=30 for every hour).
 * 
 * @param alarm Configuration struct. 
 * @param int_enable If true, asserts INT pin when alarm matches
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_set_alarm(pcf85063a_alarm_t alarm, bool int_enable);

/**
 * @brief Get the current Alarm setting
 * @param[out] alarm Pointer to struct to fill
 * @param[out] int_enabled Pointer to bool (optional)
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_get_alarm(pcf85063a_alarm_t *alarm, bool *int_enabled);

/**
 * @brief Check if the alarm flag is set (interrupt triggered)
 * @param[out] active True if alarm has triggered
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_get_alarm_flag(bool *active);

/**
 * @brief Clear the alarm interrupt flag
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_clear_alarm_flag(void);


// --- Timer Functions ---

typedef enum {
    PCF85063A_TIMER_FREQ_4096HZ = 0b00, // 4.096 kHz
    PCF85063A_TIMER_FREQ_64HZ   = 0b01, // 64 Hz
    PCF85063A_TIMER_FREQ_1HZ    = 0b10, // 1 Hz
    PCF85063A_TIMER_FREQ_1_60HZ = 0b11  // 1/60 Hz (1 per minute)
} pcf85063a_timer_freq_t;

/**
 * @brief Configure and Start the Countdown Timer
 * @param value 8-bit countdown value (0-255)
 * @param freq Timer input frequency source
 * @param int_enable If true, asserts INT pin when timer reaches 0
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_set_timer(uint8_t value, pcf85063a_timer_freq_t freq, bool int_enable);

/**
 * @brief Read the current Timer value
 * @param[out] value Current countdown value
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_get_timer(uint8_t *value);

/**
 * @brief Check if the timer flag is set (interrupt triggered)
 * @param[out] active True if timer has triggered
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_get_timer_flag(bool *active);

/**
 * @brief Clear the timer interrupt flag
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_clear_timer_flag(void);


// --- Clock Output Functions ---

typedef enum {
    PCF85063A_CLKOUT_32768HZ = 0b000,
    PCF85063A_CLKOUT_16384HZ = 0b001,
    PCF85063A_CLKOUT_8192HZ  = 0b010,
    PCF85063A_CLKOUT_4096HZ  = 0b011,
    PCF85063A_CLKOUT_2048HZ  = 0b100,
    PCF85063A_CLKOUT_1024HZ  = 0b101,
    PCF85063A_CLKOUT_1HZ     = 0b110,
    PCF85063A_CLKOUT_OFF     = 0b111 // Disable CLKOUT
} pcf85063a_clkout_t;

/**
 * @brief Configure the Clock Output (CO) pin
 * @param freq Frequency to output
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_set_clkout(pcf85063a_clkout_t freq);


// --- RAM ---

/**
 * @brief Write generic data to the single byte of RAM
 * @param data Byte to write
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_write_ram(uint8_t data);

/**
 * @brief Read data from the single byte of RAM
 * @param[out] data Pointer to byte to fill
 * @return ESP_OK on success
 */
esp_err_t pcf85063a_read_ram(uint8_t *data);

#ifdef __cplusplus
}
#endif
