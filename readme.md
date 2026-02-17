# Waveshare 2.41" AMOLED ESP-IDF Driver V5.5 Stack

## Overview

This repository contains a full **ESP-IDF v5.5** implementation for the **Waveshare 2.41" AMOLED Display Board** (ESP32-S3). It features completely custom drivers written from scratch to leverage the modern `i2c_master` and `spi_master` APIs, abandoning legacy drivers entirely.

| Component | Hardware | Controller | Interface |
| :--- | :--- | :--- | :--- |
| **MCU** | ESP32-S3 | - | - |
| **Display** | 2.41" AMOLED | RM690B0 | QSPI (1-bit cmd, 4-bit data) |
| **Touch** | Capacitive | FT6336U | I2C (0x38) |
| **IMU** | 6-Axis | QMI8658C | I2C (0x6B) |
| **RTC** | Real Time Clock | PCF85063A | I2C (0x51) |
| **IO** | IO Expander | TCA9554 | I2C (0x20) |
| **Power** | PMIC | ETA6098 | Hardware-managed (No SW) |

All drivers are located in `components/` and are designed to be independent but orchestrated by the **HAL**.

---

## 🏗️ Hardware Abstraction Layer (HAL)

The `ws_241_hal` component centralizes initialization and provides a clean API for the application.

### HAL Initialization

```c
#include "ws_241_hal.h"

void app_main(void) {
    // Initializes Bus, I2C devices, Display, and Power
    if (ws_241_hal_init() == ESP_OK) {
        ESP_LOGI("APP", "System Ready");
    }
}
```

### HAL Features

| Function | Description |
| :--- | :--- |
| `ws_241_hal_get_imu_data(qmi_data_t *data)` | Reads latest Accel/Gyro data from QMI8658C |
| `ws_241_hal_start_touch_test()` | Launches a FreeRTOS task to draw on screen with touch |
| **Automatic Power Management** | Handles Display Power (via TCA9554) and Keep-Alive Latch (GPIO16) |

---

## 📺 RM690B0 (Display Driver)

A highly optimized QSPI driver for the AMOLED panel.

- **Resolution**: 600x450 (with offsets handled internally)
- **Color Depth**: 16-bit RGB565 (Big Endian)
- **Frame Buffer**: Supports partial updates and direct drawing.

### Display API Reference

```c
#include "rm690b0.h"

// Initialize (Done by HAL automatically)
rm690b0_init(&config);

// Clear Screen
rm690b0_fill_screen(0x0000); // Black

// Draw Rectangle
rm690b0_fill_rect(x, y, w, h, 0xF800); // Red

// Draw Buffer (Bitmap)
rm690b0_draw_bitmap(x, y, w, h, buffer);

// Set Brightness (0-255)
rm690b0_set_brightness(200);

// Screen Control
rm690b0_display_on();
rm690b0_display_off();
```

---

## 🕒 PCF85063A (RTC Driver)

A full-featured driver for the ultra-low power Real-Time Clock.

### RTC Time Management

```c
#include "pcf85063a.h"

struct tm t;
// Get Time
pcf85063a_get_time(&t);
printf("Time: %02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);

// Set Time
t.tm_hour = 12; t.tm_min = 0; t.tm_sec = 0;
pcf85063a_set_time(&t);
```

### RTC Advanced Features

| Feature | Function | Description |
| :--- | :--- | :--- |
| **Alarm** | `pcf85063a_set_alarm()` | Trigger interrupt at specific Second, Minute, Hour, Day, or Weekday. Supports wildcards (-1). |
| **Timer** | `pcf85063a_set_timer()` | 8-bit Countdown Timer (4Hz to 1/60Hz clock source). Triggers INT on zero. |
| **Clock Out** | `pcf85063a_set_clkout()` | Output 1Hz - 32kHz square wave on CLK pin. |
| **RAM** | `pcf85063a_read_ram()` | Read/Write 1 byte of battery-backed RAM. |

---

## 🧭 QMI8658C (IMU Driver)

Driver for the 6-Axis Inertial Measurement Unit (Accel + Gyro).

- **Standard**: I2C (Driver uses I2C)
- **Orientation**: ENU (East-North-Up) convention applied.

### IMU API Reference

```c
#include "qmi8658c.h"

// Configuration
qmi8658c_init(i2c_handle);

// Read Data
qmi_data_t data;
qmi8658c_read_data(&data);

printf("Acc: %.2f, %.2f, %.2f g\n", data.acc_x, data.acc_y, data.acc_z);
printf("Gyr: %.2f, %.2f, %.2f dps\n", data.gyr_x, data.gyr_y, data.gyr_z);
```

---

## 👆 FT6336U (Touch Driver)

Capacitive touch controller driver.

### Touch API Reference

```c
#include "ft6336u.h"

// Check for touch
if (ft6336u_read_touch_data(&x, &y, &pressed)) {
    if (pressed) {
        printf("Touch at %d, %d\n", x, y);
    }
}
```

---

## 🔌 TCA9554 (IO Expander)

Controls auxiliary power lines and interrupts.
*Used internally by HAL to power on the Display (PWR_EN).*

### IO API Reference

```c
#include "tca9554.h"

// Set Pin Direction (Input/Output)
tca9554_set_direction(PIN_NUM, TCA_OUTPUT);

// Write Level
tca9554_set_level(PIN_NUM, 1);

// Read Level
int level = tca9554_get_level(PIN_NUM);
```

---

## Pinout Mapping (ESP32-S3)

| Function | GPIO | Notes |
| :--- | :--- | :--- |
| **I2C SDA** | 47 | Shared bus |
| **I2C SCL** | 48 | Shared bus |
| **QSPI CS** | 9 | Display Chip Select |
| **QSPI CLK** | 10 | Display Clock |
| **QSPI D0** | 11 | Data 0 |
| **QSPI D1** | 12 | Data 1 |
| **QSPI D2** | 13 | Data 2 |
| **QSPI D3** | 14 | Data 3 |
| **Display RST** | 21 | Display Reset |
| **Power Latch** | 16 | Hold HIGH to keep power on |
| **Power Btn** | 15 | Input, Active Low |
| **IO INT** | 18 | Interrupt from TCA9554 |
