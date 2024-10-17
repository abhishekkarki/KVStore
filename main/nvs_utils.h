#ifndef NVS_UTILS_H
#define NVS_UTILS_H

#include <stdio.h>
#include <string.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "measurement.h"
#include <stdbool.h>

extern const char *NVS_NAMESPACE;

esp_err_t init_nvs();
bool store_measurement_in_flash(Measurement *m);
bool find_measurement_in_flash(uint32_t timestamp, Measurement *result);
bool is_measurement_in_flash(uint32_t timestamp);

#endif // NVS_UTILS_H