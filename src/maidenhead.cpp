#include "maidenhead.h"

void getMaidenheadLocator(double lat, double lon, char* outStr, uint8_t length) {
    if (length != 6 && length != 8 && length != 10) {
        length = 8;
    }

    // Boundary constraints to avoid out-of-range indexing
    if (lat >= 90.0)   lat = 89.9999999;
    if (lat < -90.0)   lat = -90.0;
    if (lon >= 180.0)  lon = 179.9999999;
    if (lon < -180.0)  lon = -180.0;

    double adjLon = lon + 180.0;
    double adjLat = lat + 90.0;

    // --- Pair 1: Field Level (20° Lon x 10° Lat) ---
    int fLon = static_cast<int>(adjLon / 20.0);
    int fLat = static_cast<int>(adjLat / 10.0);
    double rLon = adjLon - (fLon * 20.0);
    double rLat = adjLat - (fLat * 10.0);

    outStr[0] = static_cast<char>('A' + fLon);
    outStr[1] = static_cast<char>('A' + fLat);

    // --- Pair 2: Square Level (2° Lon x 1° Lat) ---
    int sqLon = static_cast<int>(rLon / 2.0);
    int sqLat = static_cast<int>(rLat / 1.0);
    rLon -= (sqLon * 2.0);
    rLat -= (sqLat * 1.0);

    outStr[2] = static_cast<char>('0' + sqLon);
    outStr[3] = static_cast<char>('0' + sqLat);

    // --- Pair 3: Subsquare Level (5' Lon x 2.5' Lat) ---
    int subLon = static_cast<int>(rLon / (2.0 / 24.0));
    int subLat = static_cast<int>(rLat / (1.0 / 24.0));
    rLon -= (subLon * (2.0 / 24.0));
    rLat -= (subLat * (1.0 / 24.0));

    outStr[4] = static_cast<char>('a' + subLon);
    outStr[5] = static_cast<char>('a' + subLat);

    if (length >= 8) {
        // --- Pair 4: Extended Square Level (30" Lon x 15" Lat) ---
        int extSqLon = static_cast<int>(rLon / (2.0 / 240.0));
        int extSqLat = static_cast<int>(rLat / (1.0 / 240.0));
        rLon -= (extSqLon * (2.0 / 240.0));
        rLat -= (extSqLat * (1.0 / 240.0));

        if (extSqLon > 9) extSqLon = 9;
        if (extSqLat > 9) extSqLat = 9;

        outStr[6] = static_cast<char>('0' + extSqLon);
        outStr[7] = static_cast<char>('0' + extSqLat);
    }

    if (length == 10) {
        // --- Pair 5: Extended Subsquare Level (1.25" Lon x 0.625" Lat) ---
        int extSubLon = static_cast<int>(rLon / (2.0 / 5760.0));
        int extSubLat = static_cast<int>(rLat / (1.0 / 5760.0));

        if (extSubLon > 23) extSubLon = 23;
        if (extSubLat > 23) extSubLat = 23;

        outStr[8] = static_cast<char>('a' + extSubLon);
        outStr[9] = static_cast<char>('a' + extSubLat);
    }

    outStr[length] = '\0';
}
