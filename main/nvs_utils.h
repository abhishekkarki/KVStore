// nvs_utils.h

#ifndef NVS_UTILS_H
#define NVS_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"  // Add this line to include esp_err_t

#include "measurement.h"

esp_err_t init_nvs();
bool store_measurement_in_flash(Measurement *m);
bool find_measurement_in_flash(uint32_t timestamp, Measurement *result);
void clear_flash_storage();
uint32_t get_flash_usage_percent();

#endif // NVS_UTILS_H