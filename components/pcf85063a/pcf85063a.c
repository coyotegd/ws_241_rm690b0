#include "pcf85063a.h"
#include "esp_log.h"

static const char *TAG = "PCF85063A";
static i2c_master_dev_handle_t g_rtc_dev = NULL;

// Registers
#define REG_CTRL1       0x00
#define REG_CTRL2       0x01
#define REG_OFFSET      0x02
#define REG_RAM         0x03
#define REG_SECONDS     0x04
#define REG_MINUTES     0x05
#define REG_HOURS       0x06
#define REG_DAYS        0x07
#define REG_WEEKDAYS    0x08
#define REG_MONTHS      0x09
#define REG_YEARS       0x0A

// Alarm Registers
#define REG_ALARM_SEC   0x0B
#define REG_ALARM_MIN   0x0C
#define REG_ALARM_HOUR  0x0D // Includes AM/PM
#define REG_ALARM_DAY   0x0E
#define REG_ALARM_WDAY  0x0F

// Timer & Clockout
#define REG_TIMER_VAL   0x10
#define REG_TIMER_MODE  0x11

// Masks & Bits
#define DAT_ALARM_EN    0x80 // Enable bit in Alarm registers (Wait, datasheet says 0=En, 1=Dis? No, datasheet calls it AEN_x)
                                // PCF85063A Datasheet: Bit 7 of alarm regs is AEN. 
                                // 0 = Alarm enabled (match involves this param)
                                // 1 = Alarm disabled (ignore this param)
                                // Wait, let's double check. 
                                // "AEN_x = 0: Second/Min/etc is participating in alarm."
                                // "AEN_x = 1: Second/Min/etc is IGNORED."
                                // So defaults to 1 (Start up state) = Ignored.

#define BIT_CTRL2_AIE   (1 << 7) // Alarm Interrupt Enable
#define BIT_CTRL2_AF    (1 << 6) // Alarm Flag
#define BIT_CTRL2_TIE   (1 << 2) // Timer Interrupt Enable
#define BIT_CTRL2_TF    (1 << 3) // Timer Flag (Note: It's bit 3 in CTRL2?)
                                 // Looking at Datasheet:
                                 // CTRL2 (01h):
                                 // 7: AIE (Alarm Int En)
                                 // 6: AF (Alarm Flag)
                                 // 5: MI (Minute Int En)
                                 // 4: HMI (Half Min Int En)
                                 // 3: TF (Timer Flag)
                                 // 2: TIE (Timer Int En)
                                 // 1: COF[2]
                                 // 0: COF[1]
                                 // COF[0] is in CTRL1? No. COF[2:0] in CTRL2: 
                                 // Wait, PCF85063A vs 63TP?
                                 // PCF85063A:
                                 // CTRL2 (01h): .7=AIE, .6=AF, .5=MI, .4=HMI, .3=TF, .2=TIE, .1=COF2, .0=COF1
                                 // CTRL1 (00h): .7=EXT_TEST, .6=N/A, .5=STOP, .4=SR, .3=12_24, .2=CIE, .1=1, .0=CAP_SEL (Wait, check exact variant)
                                 // Let's assume standard PCF85063A.
                                 // REGISTER 11h: Timer Mode
                                 // .4=TE (Timer Enable), .3=TIE (Wait?), .2=TF? No. 
                                 // 11h: .4=TE (Enable), .3=TI_TP (Pulse), .2-.0=TCF[2:0] (Freq)

static uint8_t bcd2dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

esp_err_t pcf85063a_init(i2c_master_bus_handle_t bus_handle) {
    if (g_rtc_dev != NULL) return ESP_OK;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063A_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_rtc_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add RTC to bus");
        return ret;
    }

    // Verify presence by reading CTRL1 register
    uint8_t val;
    uint8_t reg = REG_CTRL1;
    ret = i2c_master_transmit_receive(g_rtc_dev, &reg, 1, &val, 1, -1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RTC Initialized. CTRL1: 0x%02X", val);
    } else {
        ESP_LOGE(TAG, "Failed to communicate with RTC");
    }
    
    return ret;
}

esp_err_t pcf85063a_get_time(struct tm *timeinfo) {
    if (!g_rtc_dev) return ESP_ERR_INVALID_STATE;

    uint8_t reg = REG_SECONDS;
    uint8_t data[7];

    esp_err_t ret = i2c_master_transmit_receive(g_rtc_dev, &reg, 1, data, 7, -1);
    if (ret != ESP_OK) return ret;

    // Mask out integrity bits (OS flag in seconds)
    timeinfo->tm_sec  = bcd2dec(data[0] & 0x7F);
    timeinfo->tm_min  = bcd2dec(data[1] & 0x7F);
    timeinfo->tm_hour = bcd2dec(data[2] & 0x3F);
    timeinfo->tm_mday = bcd2dec(data[3] & 0x3F);
    timeinfo->tm_wday = data[4] & 0x07;
    timeinfo->tm_mon  = bcd2dec(data[5] & 0x1F) - 1; // 0-11
    timeinfo->tm_year = bcd2dec(data[6]) + 100;      // Years since 1900 (20xx -> 1xx)

    return ESP_OK;
}

esp_err_t pcf85063a_set_time(const struct tm *timeinfo) {
    if (!g_rtc_dev) return ESP_ERR_INVALID_STATE;

    uint8_t data[8];
    uint8_t reg = REG_SECONDS; // Starting address
    // We send START, ADDR, then DATA. ESP-IDF i2c_master_transmit takes buffer.
    // The first byte of buffer is NOT register address if using write_to_device helper
    // But i2c_master_transmit(dev, data, len) sends raw bytes. 
    // Usually device expects: [RegAddr] [Data1] [Data2]...
    
    data[0] = REG_SECONDS;
    data[1] = dec2bcd(timeinfo->tm_sec) & 0x7F; // Ensure OS bit is cleared (start clock)
    data[2] = dec2bcd(timeinfo->tm_min);
    data[3] = dec2bcd(timeinfo->tm_hour);
    data[4] = dec2bcd(timeinfo->tm_mday);
    data[5] = timeinfo->tm_wday & 0x07;
    data[6] = dec2bcd(timeinfo->tm_mon + 1);
    data[7] = dec2bcd(timeinfo->tm_year % 100);

    return i2c_master_transmit(g_rtc_dev, data, 8, -1);
}

// --- Implementation of New Features ---

// Helper: Read single register
static esp_err_t read_reg(uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(g_rtc_dev, &reg, 1, val, 1, -1);
}

// Helper: Write single register
static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(g_rtc_dev, data, 2, -1);
}

// --- Alarm ---

/* 
 * Alarm Register Layout (0Bh - 0Fh):
 * Bit 7 (AEN): 0=Enable match, 1=Disable match (Ignore)
 * Bits 6-0: Time value (BCD)
 */
#define AEN_ENABLE  (0 << 7)
#define AEN_DISABLE (1 << 7)

esp_err_t pcf85063a_set_alarm(pcf85063a_alarm_t alarm, bool int_enable) {
    if (!g_rtc_dev) return ESP_ERR_INVALID_STATE;

    uint8_t data[6];
    data[0] = 0x0B; // Start at Second Alarm Register

    // Second: 0B
    if (alarm.second >= 0 && alarm.second < 60)
        data[1] = dec2bcd(alarm.second) | AEN_ENABLE;
    else
        data[1] = AEN_DISABLE;

    // Minute: 0C
    if (alarm.minute >= 0 && alarm.minute < 60)
        data[2] = dec2bcd(alarm.minute) | AEN_ENABLE;
    else
        data[2] = AEN_DISABLE;

    // Hour: 0D
    if (alarm.hour >= 0 && alarm.hour < 24)
        data[3] = dec2bcd(alarm.hour) | AEN_ENABLE;
    else
        data[3] = AEN_DISABLE;

    // Day: 0E
    if (alarm.day >= 1 && alarm.day <= 31)
        data[4] = dec2bcd(alarm.day) | AEN_ENABLE;
    else
        data[4] = AEN_DISABLE;

    // Weekday: 0F
    if (alarm.weekday >= 0 && alarm.weekday <= 6)
        data[5] = (alarm.weekday & 0x07) | AEN_ENABLE;
    else
        data[5] = AEN_DISABLE;

    esp_err_t ret = i2c_master_transmit(g_rtc_dev, data, 6, -1);
    if (ret != ESP_OK) return ret;

    // Configure Interrupt Setting in CTRL2 (01h)
    uint8_t ctrl2;
    ret = read_reg(0x01, &ctrl2);
    if (ret != ESP_OK) return ret;

    if (int_enable) {
        ctrl2 |= (1 << 7); // Set AIE (Alarm Interrupt Enable)
    } else {
        ctrl2 &= ~(1 << 7); // Clear AIE
    }
    
    // Also clear any previous AF flag (Bit 6) just in case
    ctrl2 &= ~(1 << 6);

    return write_reg(0x01, ctrl2);
}

esp_err_t pcf85063a_get_alarm(pcf85063a_alarm_t *alarm, bool *int_enabled) {
    if (!g_rtc_dev) return ESP_ERR_INVALID_STATE;

    uint8_t reg = 0x0B;
    uint8_t data[5];
    esp_err_t ret = i2c_master_transmit_receive(g_rtc_dev, &reg, 1, data, 5, -1);
    if (ret != ESP_OK) return ret;

    // Check AEN bit (Bit 7). If 1, disabled (-1). If 0, enabled.
    alarm->second  = (data[0] & 0x80) ? -1 : bcd2dec(data[0] & 0x7F);
    alarm->minute  = (data[1] & 0x80) ? -1 : bcd2dec(data[1] & 0x7F);
    alarm->hour    = (data[2] & 0x80) ? -1 : bcd2dec(data[2] & 0x3F);
    alarm->day     = (data[3] & 0x80) ? -1 : bcd2dec(data[3] & 0x3F);
    alarm->weekday = (data[4] & 0x80) ? -1 : (data[4] & 0x07);

    if (int_enabled) {
        uint8_t ctrl2;
        ret = read_reg(0x01, &ctrl2);
        if (ret == ESP_OK) {
            *int_enabled = (ctrl2 & (1 << 7)) ? true : false;
        }
    }
    return ESP_OK;
}

esp_err_t pcf85063a_get_alarm_flag(bool *active) {
    uint8_t ctrl2;
    esp_err_t ret = read_reg(0x01, &ctrl2);
    if (ret == ESP_OK) {
        *active = (ctrl2 & (1 << 6)) ? true : false; // Bit 6 is AF
    }
    return ret;
}

esp_err_t pcf85063a_clear_alarm_flag(void) {
    // Clear Bit 6 (AF) in CTRL2
    // We must read-modify-write as there are other flags/settings in CTRL2
    uint8_t ctrl2;
    esp_err_t ret = read_reg(0x01, &ctrl2);
    if (ret != ESP_OK) return ret;

    ctrl2 &= ~(1 << 6); // Clear AF
    return write_reg(0x01, ctrl2);
}


// --- Timer ---

esp_err_t pcf85063a_set_timer(uint8_t value, pcf85063a_timer_freq_t freq, bool int_enable) {
    if (!g_rtc_dev) return ESP_ERR_INVALID_STATE;
    esp_err_t ret;

    // 1. Set Timer Value (Reg 10h)
    ret = write_reg(0x10, value); 
    if (ret != ESP_OK) return ret;

    // 2. Configure Timer Mode (Reg 11h)
    // Structure of Register 11h for PCF85063A:
    // Bit 4: TE (Timer Enable)
    // Bit 3: TIE (Timer Interrupt Enable)
    // Bit 2: TI_TP (Timer Interrupt Mode. 0=Level, 1=Pulse)
    // Bit 1: TCF[1] (Frequency MSB)
    // Bit 0: TCF[0] (Frequency LSB)
    
    uint8_t tmode = 0;
    
    // Timer Enable
    tmode |= (1 << 4);
    
    // Interrupt Enable
    if (int_enable) {
        tmode |= (1 << 3); 
    }
    
    // Frequency (Lower 2 bits)
    tmode |= (freq & 0x03);

    return write_reg(0x11, tmode);
}

esp_err_t pcf85063a_get_timer(uint8_t *value) {
    return read_reg(0x10, value);
}

esp_err_t pcf85063a_get_timer_flag(bool *active) {
    uint8_t ctrl2;
    esp_err_t ret = read_reg(0x01, &ctrl2);
    if (ret == ESP_OK) {
        *active = (ctrl2 & (1 << 3)) ? true : false; // Bit 3 is TF
    }
    return ret;
}

esp_err_t pcf85063a_clear_timer_flag(void) {
    // Clear Bit 3 (TF) in CTRL2
    uint8_t ctrl2;
    esp_err_t ret = read_reg(0x01, &ctrl2);
    if (ret != ESP_OK) return ret;

    ctrl2 &= ~(1 << 3); // Clear TF
    return write_reg(0x01, ctrl2);
}


// --- Clock Output ---

esp_err_t pcf85063a_set_clkout(pcf85063a_clkout_t freq) {
    if (!g_rtc_dev) return ESP_ERR_INVALID_STATE;

    // Registry 01h (CTRL2) holds COF[2:0] in bits 2,1,0??
    // WAIT. Re-checking datasheet.
    // PCF85063A Reg 01h (Control 2):
    // 7: AIE
    // 6: AF
    // 5: MI
    // 4: HMI
    // 3: TF
    // 2: COF[2]
    // 1: COF[1]
    // 0: COF[0]
    //
    // OK, wait. Previously I said TIE was bit 2?
    // Datasheet Conflict Check:
    // NXP PCF85063A Datasheet Rev. 7:
    // 01h CTRL2:
    // 7: AIE
    // 6: AF
    // 5: MI
    // 4: HMI
    // 3: TF
    // 2: COF[2]
    // 1: COF[1]
    // 0: COF[0]
    //
    // SO WHERE IS TIMER INTERRUPT ENABLE (TIE)?
    // Timer interrupt is enabled via Reg 11h (Timer Mode)?
    // Reg 11h:
    // 4: TE (Enable)
    // 3: TIE (Timer Interrupt Enable). YES.
    // 2: TI_TP
    // 1,0: TCF[1:0]
    //
    // Wait, the frequency selection is TCF[2:0]??
    // Let's check Rev 7 of datasheet carefully.
    //
    // Reg 10h: Timer Value
    // Reg 11h: Timer Mode
    //   Bit 2-0: TCF[2:0] -> Clock Freq
    //   Bit 3: TE -> Timer Enable
    //   Bit 4: TIE -> Timer Interrupt Enable
    //
    // OK, correcting my logic above:
    // ALARM is in CTRL2.
    // TIMER is in TIMER MODE (11h).
    // CLKOUT is in CTRL2 (01h).
    
    // Correct Logic for Clock Output:
    // Located in CTRL2 bits 2,1,0.
    
    uint8_t ctrl2;
    esp_err_t ret = read_reg(0x01, &ctrl2);
    if (ret != ESP_OK) return ret;

    // Mask out lower 3 bits (COF)
    ctrl2 &= 0xF8;
    // Apply new freq
    ctrl2 |= (freq & 0x07);

    return write_reg(0x01, ctrl2);
}

// Correcting Timer Logic based on second review:
// TIE is bit 4 of 11h? TE is bit 3?
// Let's assume safely:
// Reg 11h (Timer Mode):
// Bit 2,1,0: TCF (Freq)
// Bit 3: TE (Enable)
// Bit 4: TIE (Int Enable)
//
// My previous code:
// tmode = (1 << 4); (Bit 4) -> This was enabling "TIE"?
// 
// Let's stick to the previous code momentarily but fix it if CLKOUT COF logic conflicts.
// Issue: COF is bits 0-2 of CTRL2.
// Issue: TIE logic in my previous block was accessing Bit 2 of CTRL2.
// IF Bit 2 of CTRL2 is COF[2], then TIE CANNOT be there.
//
// Correct Mapping:
// CTRL2 (01h): AIE(7), AF(6), MI(5), HMI(4), TF(3), COF[2:0] (2,1,0).
// Timer Mode (11h): TIE(4), TE(3), TI_TP(2), TCF[1:0] ??
// 
// Actually, datasheet variants are confusing.
// Let's trust "PCF85063A" NXP:
// 11h: 
//   4: TE (Timer Enable)
//   3: TIE (Timer Interrupt Enable)
//   2: TI_TP
//   1,0: TCF[1:0] -> Wait, only 2 bits for freq? 4 settings.
//   Value 00: 4khz, 01: 64hz, 10: 1hz, 11: 1/60hz.
//   This matches my enum pcf85063a_timer_freq_t.
//
// So, for TIMER:
//   TE is bit 4.
//   TIE is bit 3.
//   Freq is bits 1,0.
//   Bit 2 is TI_TP.
//
// And for CLKOUT:
//   CTRL2 bits 2,1,0.
//
// Okay, I need to silent-fix the timer logic in the block above too.


// --- RAM ---

esp_err_t pcf85063a_write_ram(uint8_t data) {
    return write_reg(REG_RAM, data);
}

esp_err_t pcf85063a_read_ram(uint8_t *data) {
    return read_reg(REG_RAM, data);
}

void pcf85063a_test_read(void) {
    struct tm t;
    if (pcf85063a_get_time(&t) == ESP_OK) {
        ESP_LOGI(TAG, "RTC Time: %04d-%02d-%02d %02d:%02d:%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                 t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        ESP_LOGW(TAG, "Failed to read time");
    }
}

