#include "gps_engine.h"
#include "maidenhead.h"
#include <string.h>

static HardwareSerial SerialGPS(2);
static TinyGPSPlus tinyGps;

GpsTelemetryData currentTelemetry;
SemaphoreHandle_t telemetryMutex = NULL;

static const char* CARDINALS[] = {
    "N", "NNE", "NE", "ENE",
    "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW",
    "W", "WNW", "NW", "NNW"
};

const char* getCardinalDirection(double degrees) {
    if (degrees < 0.0 || degrees >= 360.0) {
        degrees = fmod(degrees, 360.0);
        if (degrees < 0.0) degrees += 360.0;
    }
    int index = static_cast<int>((degrees + 11.25) / 22.5) % 16;
    return CARDINALS[index];
}

static void updateLocalTime(GpsTelemetryData& data) {
    int hour = data.utcHour + data.timeZoneOffsetHours;
    int day = data.utcDay;
    int month = data.utcMonth;
    int year = data.utcYear;

    if (hour < 0) {
        hour += 24;
        day--;
        if (day < 1) {
            month--;
            if (month < 1) {
                month = 12;
                year--;
            }
            // Simple month length fallback
            day = (month == 2) ? 28 : ((month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31);
        }
    } else if (hour >= 24) {
        hour -= 24;
        day++;
        int maxDays = (month == 2) ? 28 : ((month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31);
        if (day > maxDays) {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
        }
    }

    data.localHour = static_cast<uint8_t>(hour);
    data.localMinute = data.utcMinute;
    data.localSecond = data.utcSecond;
    data.localDay = static_cast<uint8_t>(day);
    data.localMonth = static_cast<uint8_t>(month);
    data.localYear = static_cast<uint16_t>(year);
}

void initGpsEngine(int rxPin, int txPin, int baud) {
    telemetryMutex = xSemaphoreCreateMutex();

    memset(&currentTelemetry, 0, sizeof(GpsTelemetryData));
    currentTelemetry.fixStatus = FIX_SEARCHING;
    currentTelemetry.gridPrecision = 8; // Default 8-digit precision
    currentTelemetry.timeZoneOffsetHours = -7; // Default PDT (UTC-7)
    strcpy(currentTelemetry.gridLocator, "Searching");
    strcpy(currentTelemetry.cardinalHeading, "N/A");

    SerialGPS.begin(baud, SERIAL_8N1, rxPin, txPin);
}

static void gpsTaskLoop(void* arg) {
    (void)arg;
    for (;;) {
        while (SerialGPS.available() > 0) {
            char c = SerialGPS.read();
            tinyGps.encode(c);
        }

        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (tinyGps.satellites.isValid()) {
                currentTelemetry.satsLocked = tinyGps.satellites.value();
            } else {
                currentTelemetry.satsLocked = 0;
            }

            if (tinyGps.hdop.isValid()) {
                currentTelemetry.hdop = tinyGps.hdop.hdop();
            }

            if (tinyGps.location.isValid()) {
                currentTelemetry.latitude = tinyGps.location.lat();
                currentTelemetry.longitude = tinyGps.location.lng();

                if (currentTelemetry.satsLocked >= 4) {
                    currentTelemetry.fixStatus = FIX_3D;
                } else if (currentTelemetry.satsLocked >= 3) {
                    currentTelemetry.fixStatus = FIX_2D;
                } else {
                    currentTelemetry.fixStatus = FIX_SEARCHING;
                }

                // Compute Maidenhead Locator
                getMaidenheadLocator(currentTelemetry.latitude, 
                                    currentTelemetry.longitude, 
                                    currentTelemetry.gridLocator, 
                                    currentTelemetry.gridPrecision);
            } else {
                currentTelemetry.fixStatus = FIX_SEARCHING;
                strcpy(currentTelemetry.gridLocator, "Searching");
            }

            if (tinyGps.altitude.isValid()) {
                currentTelemetry.altMeters = tinyGps.altitude.meters();
                currentTelemetry.altFeet = tinyGps.altitude.feet();
            }

            if (tinyGps.speed.isValid()) {
                currentTelemetry.speedMph = tinyGps.speed.mph();
                currentTelemetry.speedKph = tinyGps.speed.kmph();
            }

            if (tinyGps.course.isValid()) {
                currentTelemetry.headingDeg = tinyGps.course.deg();
                strncpy(currentTelemetry.cardinalHeading, getCardinalDirection(currentTelemetry.headingDeg), 4);
                currentTelemetry.cardinalHeading[4] = '\0';
            }

            if (tinyGps.time.isValid() && tinyGps.date.isValid()) {
                currentTelemetry.utcYear = tinyGps.date.year();
                currentTelemetry.utcMonth = tinyGps.date.month();
                currentTelemetry.utcDay = tinyGps.date.day();
                currentTelemetry.utcHour = tinyGps.time.hour();
                currentTelemetry.utcMinute = tinyGps.time.minute();
                currentTelemetry.utcSecond = tinyGps.time.second();

                updateLocalTime(currentTelemetry);
            }

            xSemaphoreGive(telemetryMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Service GPS task at ~10Hz
    }
}

void startGpsTask() {
    xTaskCreatePinnedToCore(
        gpsTaskLoop,
        "GpsTask",
        4096,
        NULL,
        5, // High priority
        NULL,
        0  // Dedicated to Core 0
    );
}

void getTelemetrySnapshot(GpsTelemetryData& dest) {
    if (telemetryMutex != NULL && xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&dest, &currentTelemetry, sizeof(GpsTelemetryData));
        xSemaphoreGive(telemetryMutex);
    }
}

void setGridPrecision(uint8_t precision) {
    if (precision == 6 || precision == 8 || precision == 10) {
        if (telemetryMutex != NULL && xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            currentTelemetry.gridPrecision = precision;
            if (currentTelemetry.fixStatus != FIX_SEARCHING && currentTelemetry.fixStatus != FIX_NONE) {
                getMaidenheadLocator(currentTelemetry.latitude, 
                                    currentTelemetry.longitude, 
                                    currentTelemetry.gridLocator, 
                                    currentTelemetry.gridPrecision);
            }
            xSemaphoreGive(telemetryMutex);
        }
    }
}

void cycleGridPrecision() {
    uint8_t nextPrecision = 6;
    if (currentTelemetry.gridPrecision == 6) nextPrecision = 8;
    else if (currentTelemetry.gridPrecision == 8) nextPrecision = 10;
    else nextPrecision = 6;

    setGridPrecision(nextPrecision);
}

void setTimeZoneOffset(int8_t offsetHours) {
    if (telemetryMutex != NULL && xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentTelemetry.timeZoneOffsetHours = offsetHours;
        updateLocalTime(currentTelemetry);
        xSemaphoreGive(telemetryMutex);
    }
}
