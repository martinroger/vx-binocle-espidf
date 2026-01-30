#pragma once
#ifndef ODOMETER_H
#define ODOMETER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "nvs_storage.h"

#ifdef __cplusplus
extern "C"
{
#endif

    static uint32_t odometer_m = 99; // Odometer value in meters
    static uint32_t trip_m = 99;     // Trip value in meters

    // Get odometer value in meters. If found != NULL it will be set to true when a value exists.
    uint32_t odometer_get(bool *found);
    // Set odometer value in meters (persists only if different to avoid unnecessary writes)
    esp_err_t odometer_set(uint32_t val);

    // Trip equivalent getters/setters
    uint32_t trip_get(bool *found);
    esp_err_t trip_set(uint32_t val);

#ifdef __cplusplus
}
#endif

#endif // ODOMETER_H
