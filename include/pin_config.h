#pragma once

// LilyGo T-Display S3 AMOLED 1.43" (CO5300 + FT3168)

// Display (QSPI)
#define LCD_CS    10
#define LCD_SCLK  12
#define LCD_SDIO0 11
#define LCD_SDIO1 13
#define LCD_SDIO2 14
#define LCD_SDIO3 15
#define LCD_RST   17
#define LCD_EN    16   // panel power enable, must be HIGH

#define LCD_WIDTH  466
#define LCD_HEIGHT 466

// Touch (I2C)
#define IIC_SDA 7
#define IIC_SCL 6
#define TP_INT  9

// Battery voltage ADC
#define BATTERY_VOLTAGE_ADC_DATA 4
