#ifndef GPS_ENGINE_H
#define GPS_ENGINE_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

enum GpsFixStatus {
    FIX_NONE = 0,
    FIX_SEARCHING,
    FIX_2D,
    FIX_3D
};

struct GpsTelemetryData {
    GpsFixStatus fixStatus;
    uint32_t satsLocked;
    double hdop;

    double latitude;
    double longitude;
    double altMeters;
    double altFeet;

    double speedMph;
    double speedKph;
    double headingDeg;
    char cardinalHeading[5]; // e.g. "N", "NNE", "NE", etc.

    // Time Data
    uint16_t utcYear;
    uint8_t utcMonth;
    uint8_t utcDay;
    uint8_t utcHour;
    uint8_t utcMinute;
    uint8_t utcSecond;

    uint16_t localYear;
    uint8_t localMonth;
    uint8_t localDay;
    uint8_t localHour;
    uint8_t localMinute;
    uint8_t localSecond;

    int8_t timeZoneOffsetHours;

    char gridLocator[11];
    uint8_t gridPrecision; // 6, 8, or 10
};

extern GpsTelemetryData currentTelemetry;
extern SemaphoreHandle_t telemetryMutex;

void initGpsEngine(int rxPin = 22, int txPin = 27, int baud = 9600);
void startGpsTask();

void getTelemetrySnapshot(GpsTelemetryData& dest);
void setGridPrecision(uint8_t precision);
void cycleGridPrecision();
void setTimeZoneOffset(int8_t offsetHours);

const char* getCardinalDirection(double degrees);

#endif // GPS_ENGINE_H
