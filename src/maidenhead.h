#ifndef MAIDENHEAD_H
#define MAIDENHEAD_H

#include <Arduino.h>

/**
 * @brief Encodes WGS84 geographic coordinates into Maidenhead grid locators.
 * 
 * @param lat Input Latitude (-90.0 to +90.0 degrees)
 * @param lon Input Longitude (-180.0 to +180.0 degrees)
 * @param outStr Output buffer (must contain at least length + 1 bytes)
 * @param length Target character precision: 6, 8, or 10 characters.
 */
void getMaidenheadLocator(double lat, double lon, char* outStr, uint8_t length);

#endif // MAIDENHEAD_H
