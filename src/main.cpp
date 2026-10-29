#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"

// ---- Display setup ---------------------------------------------------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

// Column offset 4 (kept even: CO5300 writes in 2x2 blocks). The extra +1 px
// needed to center the glass is done in software via CX below.
Arduino_GFX *gfx = new Arduino_CO5300(bus, LCD_RST, 0 /* rotation */, false /* IPS */,
                                      LCD_WIDTH, LCD_HEIGHT,
                                      4 /* col offset 1 */, 0, 0, 0);

// Screen geometry: the glass is round, so only a circle of the 466x466 area is visible.
const int16_t W = LCD_WIDTH;
const int16_t H = LCD_HEIGHT;
const int16_t CX = W / 2 + 1; // 234 = calibrated visual center (equivalent to col offset 5)
const int16_t CY = H / 2 - 1; // 232 = calibrated visual center
const int16_t R = W / 2 - 1;  // 232 = outermost visible ring (glass edge)
const int16_t UI_R = R - 5;   // 227 = UI boundary: all interface content stays inside this

void drawBorderTest()
{
    gfx->fillScreen(BLACK);

    // Screen-limit rings: if the panel is aligned you should see all three
    gfx->drawCircle(CX, CY, R, RED);        // r=232, should touch the edge
    gfx->drawCircle(CX, CY, UI_R, YELLOW);  // r=227, UI boundary
    gfx->drawCircle(CX, CY, R - 20, GREEN); // r=212, reference only

    // Crosshair through the center
    gfx->drawFastHLine(0, CY, W, DARKGREY);
    gfx->drawFastVLine(CX, 0, H, DARKGREY);

    // Labels
    gfx->setTextColor(WHITE);
    gfx->setTextSize(3);
    gfx->setCursor(CX - 80, CY - 70);
    gfx->print("HELLO OLED");

    gfx->setTextSize(2);
    gfx->setCursor(CX - 66, CY + 30);
    gfx->printf("%dx%d", W, H);

    gfx->setTextColor(CYAN);
    gfx->setCursor(CX - 6, 30);
    gfx->print("N");
    gfx->setCursor(CX - 6, H - 45);
    gfx->print("S");
    gfx->setCursor(CX - 203, CY - 20);
    gfx->print("W");
    gfx->setCursor(CX + 191, CY - 20);
    gfx->print("E");
}

void setup()
{
    Serial.begin(115200);

    pinMode(LCD_EN, OUTPUT);
    digitalWrite(LCD_EN, HIGH); // power the panel

    if (!gfx->begin())
    {
        Serial.println("gfx->begin() failed!");
    }

    drawBorderTest();

    // CO5300 starts at brightness 0, so the screen stays black until this ramp
    for (int i = 0; i <= 255; i++)
    {
        gfx->Display_Brightness(i);
        delay(3);
    }

    Serial.println("Display ready");
}

void loop()
{
    // Simple live text: erase a small box and redraw only it (no full-screen clear = no flicker)
    static uint32_t last = 0;
    if (millis() - last >= 1000)
    {
        last = millis();
        gfx->fillRect(CX - 70, CY + 60, 140, 24, BLACK);
        gfx->setTextColor(GREEN);
        gfx->setTextSize(2);
        gfx->setCursor(CX - 66, CY + 64);
        gfx->printf("t=%lus", (unsigned long)(millis() / 1000));
    }
}
