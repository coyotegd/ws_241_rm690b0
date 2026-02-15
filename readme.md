# Waveshare 2.41" AMOLED ESP-IDF Driver (RM690B0)

## Overview

This project contains a working ESP-IDF implementation for the Waveshare 2.41" AMOLED Display (RM690B0 controller, ESP32-S3).

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
  - **Fix**: We empirically found the Y-offsets. In Landscape (Rotation 1), `offset_y = 18` must be applied to align the visible area with the memory buffer.

### 3. The "Red Box Line" Glitch

After getting the display running, we faced a persistent bug:

- **Symptom**: The first object drawn (Red Box at `0,0`) appeared as a squashed horizontal line, while all subsequent objects were perfect.
- **Cause**: The `fill_screen` (Clear) function was not releasing the Chip Select (CS) line correctly (`SPI_TRANS_CS_KEEP_ACTIVE` remained set).
- **Result**: The display interpreted the *next* command (Draw Red Box) as merely "more black pixels" for the previous clear command, ignoring the new coordinate instructions.
- **Solution**: We fixed the transaction logic in `rm690b0.c` to explicitly release the bus and clear the CS flag at the end of every block operation (`fill_screen`, `draw_rect`).

### 4. Hardware Dependency: TCA9554 IO Expander

Unlike the LilyGo implementation where the display is powered directly, the Waveshare board routes the display's power delivery through a **TCA9554** I2C IO Expander.

- **Power Enable**: The display 3.3V/1.8V rails are controlled by **TCA9554 Pin 1 (EXIO1)**.
- **Requirement**: The driver must initialize the I2C bus and the TCA9554 first, then drive EXIO1 HIGH to power on the AMOLED panel *before* attempting any SPI communication. Failing to do this results in a responsive SPI bus but a black, unpowered screen.
- **Tearing Effect (TE)**: The TE pin is also routed to the TCA9554 (EXIO0) rather than a direct GPIO, requiring polling over I2C if VSYNC synchronization is needed.

## Final Status

- **Interface**: QSPI (Quad SPI) @ 40MHz+
- **Driver**: Custom `rm690b0` component (located in `components/rm690b0`).
- **Features**:
  - Full color (RGB565).
  - Correct orientations (Landscape/Portrait).
  - High-speed block transfers using DMA.
  - 50x50 Test Pattern renders perfectly (Red, Green, Blue, White, Yellow).

## Build & Flash

```bash
idf.py build flash monitor
```
