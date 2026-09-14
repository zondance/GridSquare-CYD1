#include <Arduino.h>
#include "cyd_config.h"
#include "gps_engine.h"
#include "ui_display.h"

static unsigned long lastUiUpdate = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESP32 CYD GPS Grid Square Locator & Dashboard ---");

    // 1. Initialize Display & UI
    initUiDisplay();
    Serial.println("[System] LovyanGFX Display & Touch initialized.");

    // 2. Initialize GPS Engine (Hardware UART2 on CN1: RX=GPIO 22, TX=GPIO 27)
    initGpsEngine(22, 27, 9600);
    Serial.println("[System] Hardware UART2 initialized on Pins 22(RX) and 27(TX).");

    // 3. Start FreeRTOS GPS Task on Core 0
    startGpsTask();
    Serial.println("[System] FreeRTOS Core 0 GPS telemetry task launched.");
}

void loop() {
    // 1. Handle Touch Input (Core 1)
    uint16_t touchX = 0, touchY = 0;
    if (tft.getTouch(&touchX, &touchY)) {
        handleTouchInput(static_cast<int16_t>(touchX), static_cast<int16_t>(touchY));
        delay(150); // Debounce touch
    }

    // 2. Refresh UI at ~20 FPS (50ms interval)
    unsigned long now = millis();
    if (now - lastUiUpdate >= 50) {
        lastUiUpdate = now;

        GpsTelemetryData telemetry;
        getTelemetrySnapshot(telemetry);
        updateUiDisplay(telemetry);
    }
}
