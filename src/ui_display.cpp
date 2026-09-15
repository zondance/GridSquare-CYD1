#include "ui_display.h"
#include <stdio.h>
#include <esp_heap_caps.h>

LGFX_ESP32_CYD tft;
UiDisplayMode currentUiMode = UI_MODE_GRID;

static LGFX_Sprite canvas(&tft);

// Onboard RGB LED pins (Active LOW)
#define RGB_RED_PIN   4
#define RGB_GREEN_PIN 16
#define RGB_BLUE_PIN  17
#define BACKLIGHT_PIN 21
#define LDR_PIN       34  // CYD onboard Light Dependent Resistor (ADC1_CH6)

static uint8_t currentBrightness = 255;

static void setRgbLed(bool red, bool green, bool blue) {
    digitalWrite(RGB_RED_PIN, red ? LOW : HIGH);
    digitalWrite(RGB_GREEN_PIN, green ? LOW : HIGH);
    digitalWrite(RGB_BLUE_PIN, blue ? LOW : HIGH);
}

void initUiDisplay() {
    Serial.println("[UI] initUiDisplay() START");

    // 1. Explicitly enable TFT Backlight Pin
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);

    // 2. Initialize Status RGB LED
    pinMode(RGB_RED_PIN, OUTPUT);
    pinMode(RGB_GREEN_PIN, OUTPUT);
    pinMode(RGB_BLUE_PIN, OUTPUT);
    setRgbLed(true, false, false); // Red on init (Searching)

    // 3. Initialize LovyanGFX Panel & Backlight PWM
    tft.init();
    tft.setRotation(1);     // Landscape 320x240
    tft.setBrightness(255); // Max brightness
    Serial.printf("[UI] Panel ready: %dx%d, Heap free: %u, Largest block: %u\n",
                  tft.width(), tft.height(), ESP.getFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // 4. Create sprite for double-buffered rendering
    //    ESP32 without PSRAM cannot allocate a 153KB contiguous block for 16-bit 320x240.
    //    Strategy: try 16-bit first (best quality), fall back to 8-bit (256 colors).
    canvas.setPsram(true);
    canvas.setColorDepth(16);
    void* spriteResult = canvas.createSprite(320, 240);

    if (spriteResult == nullptr) {
        canvas.setPsram(false);
        spriteResult = canvas.createSprite(320, 240);
    }

    if (spriteResult == nullptr) {
        canvas.setColorDepth(8);
        spriteResult = canvas.createSprite(320, 240);
    }

    if (spriteResult == nullptr) {
        Serial.println("[UI] ERROR: All sprite allocations failed!");
    } else {
        Serial.printf("[UI] Sprite OK: depth=%d-bit, heap remaining: %u\n",
                      canvas.getColorDepth(), ESP.getFreeHeap());
    }
    canvas.setTextWrap(false);

    // 5. Render Boot Splash Screen
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setFont(&fonts::Font4);
    canvas.setTextDatum(middle_center);
    canvas.drawString("CYD GPS TRACKER", 160, 70);

    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setFont(&fonts::Font2);
    canvas.drawString("Initializing Hardware...", 160, 120);
    canvas.drawString("Connecting to GY-GPS6MV2...", 160, 150);
    canvas.drawString("Pins: RX=GPIO22 | TX=GPIO27", 160, 180);
    canvas.pushSprite(0, 0);
    Serial.println("[UI] initUiDisplay() COMPLETE");
}

// Read the onboard LDR and adjust backlight brightness.
// LDR on CYD: brighter ambient light = lower ADC reading, darker = higher reading.
// Maps ADC range to brightness 30..255 (never fully off).
void updateAutoBrightness() {
    int ldrRaw = analogRead(LDR_PIN);
    // ADC 12-bit: 0..4095. Low value = bright room, high value = dark room.
    // In bright light we want high brightness, in dark we want low brightness.
    uint8_t target = map(constrain(ldrRaw, 0, 4095), 0, 4095, 255, 30);

    // Smooth transitions to avoid flickering
    if (target > currentBrightness) {
        currentBrightness += min((uint8_t)5, (uint8_t)(target - currentBrightness));
    } else if (target < currentBrightness) {
        currentBrightness -= min((uint8_t)5, (uint8_t)(currentBrightness - target));
    }
    tft.setBrightness(currentBrightness);
}

static void renderGridTelemetryMode(const GpsTelemetryData& telemetry) {
    canvas.fillSprite(TFT_BLACK);

    // --- 1. Header Bar (Y: 0..28) ---
    uint16_t badgeColor = TFT_RED;
    uint16_t badgeTextColor = TFT_WHITE;
    const char* badgeText = "SEARCHING";

    if (telemetry.fixStatus == FIX_3D) {
        badgeColor = TFT_GREEN;
        badgeTextColor = TFT_DARKGREEN;
        badgeText = "3D LOCK";
        setRgbLed(false, true, false); // Green LED
    } else if (telemetry.fixStatus == FIX_2D) {
        badgeColor = TFT_YELLOW;
        badgeTextColor = TFT_BLACK;
        badgeText = "2D LOCK";
        setRgbLed(true, true, false); // Yellow (Red+Green)
    } else {
        setRgbLed(true, false, false); // Red LED
    }

    // Fix Badge (Green box with darker green text when locked)
    canvas.fillRoundRect(4, 3, 76, 20, 4, badgeColor);
    canvas.setTextColor(badgeTextColor, badgeColor);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.drawString(badgeText, 42, 13);

    // Satellites & HDOP
    char satBuf[32];
    snprintf(satBuf, sizeof(satBuf), "Sats: %u | HDOP: %.1f", (unsigned int)telemetry.satsLocked, telemetry.hdop);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_left);
    canvas.drawString(satBuf, 86, 13);

    // Mode Switch Button (Top Right: Green box with darker green text)
    canvas.fillRoundRect(248, 3, 68, 20, 4, TFT_GREEN);
    canvas.setTextColor(TFT_DARKGREEN, TFT_GREEN);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.drawString("DRIVE >", 282, 13);

    // --- 2. Hero Section: Maidenhead Grid Square (Y: 30..115) ---
    // Borderless, pure black background with green text
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.drawString(telemetry.gridLocator, 160, 71);

    // --- 3. Dual Clock Block (Zulu & Local Time) (Y: 116..155) ---
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
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_left);
    canvas.drawString(zuluBuf, 12, 135);
    canvas.drawString(localBuf, 165, 135);

    // --- 4. WGS84 Coordinates Block (Y: 158..195) ---
    char latBuf[32], lonBuf[32];
    if (telemetry.fixStatus != FIX_SEARCHING && telemetry.fixStatus != FIX_NONE) {
        snprintf(latBuf, sizeof(latBuf), "LAT: %10.6f%c", fabs(telemetry.latitude), (telemetry.latitude >= 0) ? 'N' : 'S');
        snprintf(lonBuf, sizeof(lonBuf), "LON: %10.6f%c", fabs(telemetry.longitude), (telemetry.longitude >= 0) ? 'E' : 'W');
    } else {
        snprintf(latBuf, sizeof(latBuf), "LAT: --.------");
        snprintf(lonBuf, sizeof(lonBuf), "LON: ---.------");
    }

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_left);
    canvas.drawString(latBuf, 12, 176);
    canvas.drawString(lonBuf, 165, 176);

    // --- 5. Altitude & Motion Footer (Y: 198..236) ---
    char altBuf[32], motionBuf[32];
    if (telemetry.fixStatus != FIX_SEARCHING && telemetry.fixStatus != FIX_NONE) {
        snprintf(altBuf, sizeof(altBuf), "ALT: %.0fft (%.0fm)", telemetry.altFeet, telemetry.altMeters);
        snprintf(motionBuf, sizeof(motionBuf), "SPD: %.1fmph | %s", telemetry.speedMph, telemetry.cardinalHeading);
    } else {
        snprintf(altBuf, sizeof(altBuf), "ALT: --- ft");
        snprintf(motionBuf, sizeof(motionBuf), "SPD: 0.0mph");
    }

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_left);
    canvas.drawString(altBuf, 12, 217);
    canvas.drawString(motionBuf, 165, 217);
}

static void renderDrivingDashboardMode(const GpsTelemetryData& telemetry) {
    canvas.fillSprite(TFT_BLACK);

    // --- Top Bar (Y: 0..28) ---
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_left);
    canvas.drawString("DRIVING DASHBOARD", 8, 13);

    // Mode Switch Button (Top Right)
    canvas.fillRoundRect(248, 3, 68, 20, 4, TFT_GREEN);
    canvas.setTextColor(TFT_DARKGREEN, TFT_GREEN);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.drawString("< GRID", 282, 13);

    // --- Left Box: Large Digital Speedometer (Y: 30..185, Width: 155) ---
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(top_center);
    canvas.drawString("SPEED (MPH)", 80, 36);

    char speedStr[16];
    snprintf(speedStr, sizeof(speedStr), "%.1f", telemetry.speedMph);
    canvas.setFont(&fonts::Font7); // Big digits
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.drawString(speedStr, 80, 105);

    char kphStr[32];
    snprintf(kphStr, sizeof(kphStr), "%.1f KPH", telemetry.speedKph);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(bottom_center);
    canvas.drawString(kphStr, 80, 178);

    // --- Right Box: Heading & Direction (Y: 30..185, Width: 155) ---
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(top_center);
    canvas.drawString("HEADING", 240, 36);

    // Cardinal Text (e.g. NW, NNE)
    canvas.setFont(&fonts::Font6);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.drawString(telemetry.cardinalHeading, 240, 95);

    // Exact degree
    char degStr[16];
    snprintf(degStr, sizeof(degStr), "%.0f deg", telemetry.headingDeg);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setTextDatum(bottom_center);
    canvas.drawString(degStr, 240, 178);

    // --- Bottom Bar: Elevation & Mini Grid (Y: 190..236) ---
    char altStr[32], gridStr[32];
    snprintf(altStr, sizeof(altStr), "ALT: %.0f ft", telemetry.altFeet);
    snprintf(gridStr, sizeof(gridStr), "GRID: %s", telemetry.gridLocator);

    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
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
    if (touchX <= 0 || touchY <= 0) return;

    // Top Right Mode Switch Button (X: 240..320, Y: 0..30)
    if (touchX >= 240 && touchY <= 30) {
        if (currentUiMode == UI_MODE_GRID) {
            currentUiMode = UI_MODE_DRIVING;
        } else {
            currentUiMode = UI_MODE_GRID;
        }
        return;
    }
}
