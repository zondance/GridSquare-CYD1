#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

#include "cyd_config.h"
#include "gps_engine.h"

enum UiDisplayMode {
    UI_MODE_GRID = 0,
    UI_MODE_DRIVING
};

extern LGFX_ESP32_CYD tft;
extern UiDisplayMode currentUiMode;

void initUiDisplay();
void updateUiDisplay(const GpsTelemetryData& telemetry);
void updateAutoBrightness();
void handleTouchInput(int16_t touchX, int16_t touchY);

#endif // UI_DISPLAY_H
