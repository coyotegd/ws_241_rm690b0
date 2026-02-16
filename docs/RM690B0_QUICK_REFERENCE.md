# RM690B0 Driver Comparison - Quick Reference

**Full Analysis**: [RM690B0_COMPARISON_ANALYSIS.md](RM690B0_COMPARISON_ANALYSIS.md)

## At a Glance

### Three RM690B0 Implementations Compared

| Repository | Hardware | Focus | Code Size | SPI Speed |
|-----------|----------|-------|-----------|-----------|
| **ws_241_rm690b0** | Waveshare 2.41" | Educational | ~360 lines | 40 MHz |
| **t4-s3_hal_bsp-lvgl** | LilyGo T4-S3 | Production | ~716 lines | 80 MHz |
| **LilyGo T-Display S3** | LilyGo T-Display | Reference | N/A | N/A |

## Key Differences

### Hardware Peripherals

| Component | ws_241 (Waveshare) | t4-s3 (LilyGo) |
|-----------|-------------------|----------------|
| **Display Controller** | RM690B0 | RM690B0 |
| **Touch IC** | FT6336U | CST226SE |
| **Power Management** | TCA9554 + ETA6098 | SY6970 PMIC |
| **IO Expander** | TCA9554 (I2C) | None (direct GPIO) |
| **RTC** | PCF85063A (stub) | None |
| **IMU** | QMI8658C (stub) | None |
| **SD Card** | None | Yes |

### Software Architecture

| Feature | ws_241 | t4-s3 |
|---------|--------|-------|
| **API Functions** | 8 | 18+ |
| **Async Operations** | ❌ No | ✅ Yes (FreeRTOS task) |
| **LVGL Integration** | ❌ No | ✅ Yes (BSP) |
| **Callbacks** | ❌ No | ✅ VSYNC/Error/Power |
| **Configuration** | Struct-based | Hardcoded pins |
| **Transfer Mode** | Polling only | Polling + Async |

### Display Calibration

| Rotation | ws_241 Dims | ws_241 Offsets | t4-s3 Dims | t4-s3 Offsets |
|----------|-------------|----------------|------------|---------------|
| 0 (USB Bottom) | 600×450 | (0, 16) | 600×446 | (0, 18) |
| 1 (USB Right) | 450×600 | (14, 0) | 446×600 | (18, 0) |
| 2 (USB Top) | 600×450 | (0, 14) | 600×446 | (0, 18) |
| 3 (USB Left) | 450×600 | (16, 0) | 446×600 | (18, 0) |

## Shared Technology

Both implementations use:
- ✅ Same RM690B0 controller IC (450×600 AMOLED)
- ✅ QSPI command wrapper protocol (`0x02 00 [CMD] 00`)
- ✅ LilyGo-derived initialization sequence
- ✅ 32KB chunked data transfer
- ✅ `SPI_TRANS_CS_KEEP_ACTIVE` for multi-chunk transfers
- ✅ Rotation offset compensation
- ✅ RGB565 color format

## Use Case Selection

### Choose ws_241_rm690b0 if you need:
- ✅ Learning display driver development
- ✅ Understanding QSPI protocol
- ✅ Waveshare 2.41" hardware
- ✅ Simple, direct hardware control
- ✅ Educational/exploratory projects
- ✅ TCA9554 IO expander features
- ✅ Future IMU/RTC expansion

### Choose t4-s3_hal_bsp-lvgl if you need:
- ✅ Production graphical applications
- ✅ LVGL UI framework
- ✅ LilyGo T4-S3 hardware
- ✅ Async display updates
- ✅ VSYNC synchronization
- ✅ SD card integration
- ✅ Higher performance (80 MHz SPI)

## Performance Comparison

| Metric | ws_241 | t4-s3 |
|--------|--------|-------|
| **SPI Clock** | 40 MHz | 80 MHz |
| **Est. Throughput** | ~5 MB/s | ~10 MB/s |
| **Parallelism** | None (blocking) | LVGL + worker task |
| **Full Screen Refresh** | ~54ms | ~27ms |

## Parent-Child Relationship

```
LilyGo T-Display S3 (Reference)
    │
    ├──> ws_241_rm690b0 (Waveshare)
    │    └── Educational fork
    │        └── Minimal implementation
    │        └── TCA9554 integration
    │
    └──> t4-s3_hal_bsp-lvgl (LilyGo T4-S3)
         └── Production fork
         └── LVGL integration
         └── Full BSP
```

**Relationship**: Both repositories are **siblings** that independently adapted the original LilyGo T-Display S3 driver for their respective hardware platforms. They are not parent-child but rather parallel implementations sharing a common ancestor.

## Migration Considerations

### From ws_241 to t4-s3:

**Hardware**: ⚠️ Not directly compatible (different peripherals)

**Code**: 
- Change initialization API (no config struct)
- Switch to LVGL flush functions
- Update rotation enums
- Integrate with LVGL BSP

### From t4-s3 to ws_241:

**Hardware**: ⚠️ Not directly compatible

**Code**:
- Add configuration struct
- Remove LVGL dependencies
- Implement TCA9554 power control
- Use simpler synchronous API

## Quick Stats

| Metric | ws_241_rm690b0 | t4-s3_hal_bsp-lvgl |
|--------|----------------|---------------------|
| **Total Code Lines** | ~360 | ~716 |
| **Public Functions** | 8 | 18+ |
| **Components** | 6 (4 active, 2 stubs) | 7 (all active) |
| **Default Rotation** | 0 (USB Bottom) | 270 (USB Right) |
| **Init Speed** | ~250ms | ~250ms (similar) |
| **Documentation** | Excellent (journey-style) | Good (API-focused) |

## Conclusion

**Same Controller, Different Philosophy**:
- **ws_241**: Minimal, educational, reverse-engineering focused
- **t4-s3**: Production, LVGL-integrated, performance-optimized

Both are valid implementations serving different needs. Choose based on your hardware and use case requirements.

---

**For detailed technical analysis**, see: [RM690B0_COMPARISON_ANALYSIS.md](RM690B0_COMPARISON_ANALYSIS.md)
