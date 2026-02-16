# Waveshare 2.41" AMOLED ESP-IDF Driver (RM690B0)

## Overview

This project contains a comprehensive ESP-IDF implementation for the Waveshare 2.41" AMOLED Display (RM690B0 controller, ESP32-S3), including display driver, touch controller, power management, and example applications.

Getting this display to work required significant reverse-engineering and debugging, as standard drivers produced visual artifacts, misalignment, or failed initialization. This document summarizes the journey and the final technical solution.

## The Challenge

We started with a working reference from a LilyGo T-Display S3 (same RM690B0 driver IC) but struggled to replicate it on the Waveshare hardware.

- **Initial Drivers**: Generic `rm690b0` components failed or produced garbage.
- **Porting Attempts**: Direct ports of Arduino libraries resulted in "tearing," wrong colors, or no image.
- **Hardware Differences**: The Waveshare board uses a different interface configuration (QSPI/1-bit vs 4-bit) and command structure than standard SPI displays.

## The Breakthroughs

### 1. The "Command Wrapper" Protocol (The Key Fix)

Standard SPI drivers send commands (CMD) as 8-bit values and Data (DAT) as 8-bit values.
The RM690B0 in this specific mode requires a **QSPI Command Wrapper**:

- Every standard command (e.g., `0x2A` Set Column) must be wrapped in a specific QSPI packet:
  - **Opcode**: `0x02` (Single Byte)
  - **Address**: `00 [CMD] 00` (24-bit address, where CMD is the actual instruction)
  - **Data**: The parameters follow.

We implemented a custom `rm_send_cmd` function using ESP-IDF's `SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR` to construct this packet structure bit-perfectly.

### 2. Initialization & Rotations

- **Init Sequence**: We discarded the Waveshare official init code (which was flaky) and ported the specialized sequence from the working LilyGo driver.
- **Rotation Tuning**: The display memory (`600x450`) is larger than the visible area.
  - **Issue**: Objects at `(0,0)` were often cut off or invisible.
  - **Fix**: We empirically found the offsets. In Landscape (Rotation 0), `offset_y = 16` must be applied to align the visible area with the memory buffer.

### 3. The "Red Box Line" Glitch

After getting the display running, we faced a persistent bug:

- **Symptom**: The first object drawn (Red Box at `0,0`) appeared as a squashed horizontal line, while all subsequent objects were perfect.
- **Cause**: The `fill_screen` (Clear) function was not releasing the Chip Select (CS) line correctly (`SPI_TRANS_CS_KEEP_ACTIVE` remained set).
- **Result**: The display interpreted the *next* command (Draw Red Box) as merely "more black pixels" for the previous clear command, ignoring the new coordinate instructions.
- **Solution**: We fixed the transaction logic in `rm690b0.c` to explicitly release the bus and clear the CS flag at the end of every block operation (`fill_screen`, `draw_rect`).

### 4. Hardware Dependency: TCA9554 IO Expander

Unlike the LilyGo implementation where the display is powered directly, the Waveshare board routes the display's power delivery through a **TCA9554** I2C IO Expander.

## Hardware Notes

- **Power Management (ETA6098)**: The board uses an ETA6098 for battery charging and power management. This component is **passive to the programmer**, meaning it requires no software configuration, I2C/SPI communication, or driver initialization. It operates autonomously based on its hardware configuration to handle battery charging and system power rails.

- **Power Enable**: The display 3.3V/1.8V rails are controlled by **TCA9554 Pin 1 (EXIO1)**.
- **Requirement**: The driver must initialize the I2C bus and the TCA9554 first, then drive EXIO1 HIGH to power on the AMOLED panel *before* attempting any SPI communication. Failing to do this results in a responsive SPI bus but a black, unpowered screen.
- **Tearing Effect (TE)**: The TE pin is also routed to the TCA9554 (EXIO0) rather than a direct GPIO, requiring polling over I2C if VSYNC synchronization is needed.
- **Status LEDs**:
  - **Red LED**: Indicates the board is powered (USB or Battery).
  - **Green LED**: Indicates Charging status. (On = Charging, Off = Charged).

## Final Status

- **Interface**: QSPI (Quad SPI) @ 40MHz+
- **Driver**: Custom `rm690b0` component (located in `components/rm690b0`).
- **Features**:
  - Full color (RGB565).
  - Correct orientations (Landscape/Portrait).
  - High-speed block transfers using DMA.
  - 50x50 Test Pattern renders perfectly (Red, Green, Blue, White, Yellow).

## Touch Controller (FT6336U)

We have successfully implemented a driver for the FT6336U capacitive touch controller found on this board.

- **Component**: `components/ft6336u`
- **Interface**: I2C (Address `0x38`) on `IO47` (SDA) / `IO48` (SCL).
- **Driver Integration**:
  - Initialized automatically in `ws_241_hal_init`.
  - Continuously polled in a background task (`touch_test_task`).
  - **Rotation Sync**: The touch coordinates are automatically transformed to match the active display rotation (0-3).

### Test & Usage

- **Visual Feedback**: A small **Cyan (4x4)** square is drawn under your finger point.
- **Dragging**: Moving your finger draws a continuous line.
- **Orientation**: Change the screen rotation with the **BOOT** button; drawing near the corners proves the coordinate transformation logic is correct.

## Screen Rotation & Alignment

The driver supports 4 hardware-accelerated rotation modes, tuned specifically for this panel's memory map offsets to ensure edge-to-edge alignment without artifacts or cut-off pixels.

**Cycle Logic**: Pressing the **BOOT Button (GPIO 0)** cycles through rotations Counter-Clockwise (0 -> 1 -> 2 -> 3).

| Rotation ID     | Orientation | USB Position | MADCTL | Alignment Offsets | Notes                                     |
| :-------------- | :---------- | :----------- | :----- | :---------------- | :---------------------------------------- |
| **0 (Default)** | Landscape   | **Bottom**   | `0xA0` | `X=0, Y=16`       | Perfect 600x450 centering.                |
| **1**           | Portrait    | **Right**    | `0xC0` | `X=14, Y=0`       | Tuned to fix left-shift and artifacts.    |
| **2**           | Landscape   | **Top**      | `0x60` | `X=0, Y=14`       | Inverted Landscape.                       |
| **3**           | Portrait    | **Left**     | `0x00` | `X=16, Y=0`       | Tuned to fix severe left-shift artifacts. |

*Note: The RM690B0 controller's internal RAM (often 480x600 or similar) is larger than the 450x600 physical panel, necessitating these specific start offsets (`CASET`/`RASET`) to align the active window correctly.*

## Build & Flash

```bash
idf.py build flash monitor
```

## Power Management & Controls

### Button Controls

| Button    | GPIO    | Action                 | Behavior                                           |
| :-------- | :------ | :--------------------- | :------------------------------------------------- |
| **Power** | GPIO 15 | Short Press            | Available for Application Logic (Currently logged) |
| **Power** | GPIO 15 | **Long Press (>1.5s)** | **Enter Light Sleep Mode**                         |
| **Reset** | EN      | Press                  | Hard Hardware Reset                                |

### Sleep & Wake Behavior

The firmware implements a robust "Soft Power" scheme using the onboard latching circuit and ESP32 Light Sleep.

1. **Power Latch (GPIO 16)**:
   - The board's power management (ETA6098) is controlled by a latch circuit.
   - **Boot**: Firmware immediately pulls **GPIO 16 HIGH** to keep the system powered.
   - **Sleep**: Firmware maintains this HIGH state during sleep to prevent a full power cut.

2. **Entering Sleep**:
   - Hold **Power Button** for **1.5 seconds**.
   - **Visual Feedback**: Screen turns **BLACK** immediately to indicate the command was received.
   - **Action**: System enters ESP32 Light Sleep, turning off the display, Wi-Fi, and high-speed clocks.
   - *Note: USB Serial connection will drop during sleep as potential power saving measure.*

3. **Waking Up**:
   - **Action**: Press the **Power Button** (Active Low trigger).
   - **Visual Feedback**: Screen fills with **EMERALD GREEN** for 1.5 seconds to confirm successful wake-up.
   - **Restore**: The main application interface (Test Pattern) is redrawn, and normal operation resumes.

## Repository Scope & Future Development

This repository provides a complete hardware abstraction layer and driver suite for the Waveshare 2.41" AMOLED display board. The project is designed to be extensible, supporting additional features and components that complement the display hardware.

### Current Features

- ✅ Display driver (RM690B0 via QSPI)
- ✅ Touch controller (FT6336U via I2C)
- ✅ Power management with sleep/wake
- ✅ Hardware abstraction layer (TCA9554 IO expander)
- ✅ Button controls and interrupt handling

### Future Development Welcome

The repository is open to contributions that enhance the Waveshare 2.41" AMOLED display experience:

- Graphics libraries and UI frameworks
- Example applications (games, utilities, demos)
- Network connectivity features (Wi-Fi, BLE)
- Additional sensor integrations
- Extended power management features

> **Questions about repository naming?** See [REPOSITORY_NAMING_GUIDE.md](REPOSITORY_NAMING_GUIDE.md) for guidance on when to extend this repository vs. creating a new one.

## External Resources

- [Espressif Component Registry](https://components.espressif.com/) - Browse and discover ESP-IDF components.
