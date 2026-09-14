#include "ui_display.h"
#include <stdio.h>

LGFX_ESP32_CYD tft;
UiDisplayMode currentUiMode = UI_MODE_GRID;

static LGFX_Sprite canvas(&tft);

// Onboard RGB LED pins (Active LOW)
#define RGB_RED_PIN   4
#define RGB_GREEN_PIN 16
#define RGB_BLUE_PIN  17

static void setRgbLed(bool red, bool green, bool blue) {
    digitalWrite(RGB_RED_PIN, red ? LOW : HIGH);
    digitalWrite(RGB_GREEN_PIN, green ? LOW : HIGH);
    digitalWrite(RGB_BLUE_PIN, blue ? LOW : HIGH);
}

void initUiDisplay() {
    pinMode(RGB_RED_PIN, OUTPUT);
    pinMode(RGB_GREEN_PIN, OUTPUT);
    pinMode(RGB_BLUE_PIN, OUTPUT);
    setRgbLed(true, false, false); // Red on init (Searching)

    tft.init();
    tft.setRotation(1); // Landscape 320x240
    tft.setBrightness(200); // Backlight 0-255

    canvas.createSprite(320, 240);
    canvas.setTextWrap(false);
}

static void renderGridTelemetryMode(const GpsTelemetryData& telemetry) {
    canvas.fillSprite(TFT_BLACK);

    // --- 1. Header Bar (Y: 0..28) ---
    uint16_t badgeColor = TFT_RED;
    const char* badgeText = "SEARCHING";

    if (telemetry.fixStatus == FIX_3D) {
        badgeColor = TFT_GREEN;
        badgeText = "3D LOCK";
        setRgbLed(false, true, false); // Green LED
    } else if (telemetry.fixStatus == FIX_2D) {
        badgeColor = TFT_YELLOW;
        badgeText = "2D LOCK";
        setRgbLed(true, true, false); // Yellow (Red+Green)
    } else {
        setRgbLed(true, false, false); // Red LED
    }

    // Header Background
    canvas.fillRect(0, 0, 320, 26, TFT_NAVY);
    canvas.drawFastHLine(0, 26, 320, TFT_DARKGREY);

    // Fix Badge
    canvas.fillRoundRect(4, 3, 76, 20, 4, badgeColor);
    canvas.setTextColor(TFT_BLACK, badgeColor);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.drawString(badgeText, 42, 13);

    // Satellites & HDOP
    char satBuf[32];
    snprintf(satBuf, sizeof(satBuf), "Sats: %u | HDOP: %.1f", (unsigned int)telemetry.satsLocked, telemetry.hdop);
    canvas.setTextColor(TFT_WHITE, TFT_NAVY);
    canvas.setTextDatum(middle_left);
    canvas.drawString(satBuf, 86, 13);

    // Mode Switch Button (Top Right)
    canvas.fillRoundRect(248, 3, 68, 20, 4, TFT_DARKCYAN);
    canvas.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    canvas.setTextDatum(middle_center);
    canvas.drawString("DRIVE >", 282, 13);

    // --- 2. Hero Section: Maidenhead Grid Square (Y: 30..115) ---
    canvas.fillRoundRect(4, 30, 312, 82, 6, TFT_DARKGREY);
    canvas.drawRoundRect(4, 30, 312, 82, 6, TFT_GOLD);

    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), "MAIDENHEAD LOCATOR [%u-DIGIT]", telemetry.gridPrecision);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    canvas.setTextDatum(top_center);
    canvas.drawString(titleBuf, 160, 34);

    // Grid Square String (Dynamic size based on precision)
    canvas.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    if (telemetry.gridPrecision == 10) {
        canvas.setFont(&fonts::Font4);
        canvas.setTextDatum(middle_center);
        canvas.drawString(telemetry.gridLocator, 160, 72);
    } else if (telemetry.gridPrecision == 8) {
        canvas.setFont(&fonts::Font6);
        canvas.setTextDatum(middle_center);
        canvas.drawString(telemetry.gridLocator, 160, 72);
    } else { // 6-digit
        canvas.setFont(&fonts::Font7);
        canvas.setTextDatum(middle_center);
        canvas.drawString(telemetry.gridLocator, 160, 72);
    }

    // Tap indicator
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(TFT_CYAN, TFT_DARKGREY);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("(Tap box to cycle 6/8/10 precision)", 160, 108);

    // --- 3. WGS84 Coordinates Block (Y: 116..155) ---
    canvas.fillRect(4, 116, 312, 38, TFT_BLACK);
    canvas.drawRoundRect(4, 116, 312, 38, 4, TFT_DARKCYAN);

    char latBuf[32], lonBuf[32];
    if (telemetry.fixStatus != FIX_SEARCHING && telemetry.fixStatus != FIX_NONE) {
        snprintf(latBuf, sizeof(latBuf), "LAT: %10.6f%c", fabs(telemetry.latitude), (telemetry.latitude >= 0) ? 'N' : 'S');
        snprintf(lonBuf, sizeof(lonBuf), "LON: %10.6f%c", fabs(telemetry.longitude), (telemetry.longitude >= 0) ? 'E' : 'W');
    } else {
        snprintf(latBuf, sizeof(latBuf), "LAT: --.------");
        snprintf(lonBuf, sizeof(lonBuf), "LON: ---.------");
    }

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(middle_left);
    canvas.drawString(latBuf, 12, 135);
    canvas.drawString(lonBuf, 165, 135);

    // --- 4. Dual Clock Block (Zulu & Local Time) (Y: 158..195) ---
    canvas.fillRoundRect(4, 158, 312, 36, 4, TFT_MAROON);

    char zuluBuf[32], localBuf[32];
    if (telemetry.utcYear > 0) {
        snprintf(zuluBuf, sizeof(zuluBuf), "UTC ZULU: %02u:%02u:%02uZ", 
                 telemetry.utcHour, telemetry.utcMinute, telemetry.utcSecond);
        snprintf(localBuf, sizeof(localBuf), "LOCAL: %02u:%02u:%02u", 
                 telemetry.localHour, telemetry.localMinute, telemetry.localSecond);
    } else {
        snprintf(zuluBuf, sizeof(zuluBuf), "UTC ZULU: --:--:--Z");
        snprintf(localBuf, sizeof(localBuf), "LOCAL: --:--:--");
    }

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_WHITE, TFT_MAROON);
    canvas.setTextDatum(middle_left);
    canvas.drawString(zuluBuf, 12, 176);
    canvas.drawString(localBuf, 165, 176);

    // --- 5. Altitude & Motion Footer (Y: 198..236) ---
    canvas.fillRoundRect(4, 198, 312, 38, 4, TFT_DARKGREY);

    char altBuf[32], motionBuf[32];
    if (telemetry.fixStatus != FIX_SEARCHING && telemetry.fixStatus != FIX_NONE) {
        snprintf(altBuf, sizeof(altBuf), "ALT: %.0fft (%.0fm)", telemetry.altFeet, telemetry.altMeters);
        snprintf(motionBuf, sizeof(motionBuf), "SPD: %.1fmph | %s", telemetry.speedMph, telemetry.cardinalHeading);
    } else {
        snprintf(altBuf, sizeof(altBuf), "ALT: --- ft");
        snprintf(motionBuf, sizeof(motionBuf), "SPD: 0.0mph");
    }

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREENYELLOW, TFT_DARKGREY);
    canvas.setTextDatum(middle_left);
    canvas.drawString(altBuf, 12, 217);
    canvas.drawString(motionBuf, 165, 217);
}

static void renderDrivingDashboardMode(const GpsTelemetryData& telemetry) {
    canvas.fillSprite(TFT_BLACK);

    // --- Top Bar (Y: 0..28) ---
    canvas.fillRect(0, 0, 320, 26, TFT_NAVY);
    canvas.drawFastHLine(0, 26, 320, TFT_DARKGREY);

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_CYAN, TFT_NAVY);
    canvas.setTextDatum(middle_left);
    canvas.drawString("DRIVING DASHBOARD", 8, 13);

    // Mode Switch Button (Top Right)
    canvas.fillRoundRect(248, 3, 68, 20, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.drawString("< GRID", 282, 13);

    // --- Left Box: Large Digital Speedometer (Y: 30..185, Width: 155) ---
    canvas.fillRoundRect(4, 30, 153, 155, 6, TFT_DARKGREY);
    canvas.drawRoundRect(4, 30, 153, 155, 6, TFT_GOLD);

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    canvas.setTextDatum(top_center);
    canvas.drawString("SPEED (MPH)", 80, 36);

    char speedStr[16];
    snprintf(speedStr, sizeof(speedStr), "%.1f", telemetry.speedMph);
    canvas.setFont(&fonts::Font7); // Big digits
    canvas.setTextColor(TFT_GREENYELLOW, TFT_DARKGREY);
    canvas.setTextDatum(middle_center);
    canvas.drawString(speedStr, 80, 105);

    char kphStr[32];
    snprintf(kphStr, sizeof(kphStr), "%.1f KPH", telemetry.speedKph);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_CYAN, TFT_DARKGREY);
    canvas.setTextDatum(bottom_center);
    canvas.drawString(kphStr, 80, 178);

    // --- Right Box: Heading & Direction (Y: 30..185, Width: 155) ---
    canvas.fillRoundRect(163, 30, 153, 155, 6, TFT_DARKGREY);
    canvas.drawRoundRect(163, 30, 153, 155, 6, TFT_CYAN);

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    canvas.setTextDatum(top_center);
    canvas.drawString("HEADING", 240, 36);

    // Cardinal Text (e.g. NW, NNE)
    canvas.setFont(&fonts::Font6);
    canvas.setTextColor(TFT_WHITE, TFT_DARKGREY);
    canvas.setTextDatum(middle_center);
    canvas.drawString(telemetry.cardinalHeading, 240, 95);

    // Exact degree
    char degStr[16];
    snprintf(degStr, sizeof(degStr), "%.0f deg", telemetry.headingDeg);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    canvas.setTextDatum(bottom_center);
    canvas.drawString(degStr, 240, 178);

    // --- Bottom Bar: Elevation & Mini Grid (Y: 190..236) ---
    canvas.fillRoundRect(4, 190, 312, 46, 6, TFT_NAVY);

    char altStr[32], gridStr[32];
    snprintf(altStr, sizeof(altStr), "ALT: %.0f ft", telemetry.altFeet);
    snprintf(gridStr, sizeof(gridStr), "GRID: %s", telemetry.gridLocator);

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_WHITE, TFT_NAVY);
    canvas.setTextDatum(middle_left);
    canvas.drawString(altStr, 12, 213);
    canvas.drawString(gridStr, 155, 213);
}

void updateUiDisplay(const GpsTelemetryData& telemetry) {
    if (currentUiMode == UI_MODE_GRID) {
        renderGridTelemetryMode(telemetry);
    } else {
        renderDrivingDashboardMode(telemetry);
    }

    canvas.pushSprite(0, 0);
}

void handleTouchInput(int16_t touchX, int16_t touchY) {
    if (touchX < 0 || touchY < 0) return;

    // Top Right Mode Switch Button (X: 240..320, Y: 0..30)
    if (touchX >= 240 && touchY <= 30) {
        if (currentUiMode == UI_MODE_GRID) {
            currentUiMode = UI_MODE_DRIVING;
        } else {
            currentUiMode = UI_MODE_GRID;
        }
        return;
    }

    // In Grid Mode: Tapping Hero Grid Box (X: 4..316, Y: 30..112)
    if (currentUiMode == UI_MODE_GRID && touchY >= 30 && touchY <= 112) {
        cycleGridPrecision();
        return;
    }
}
