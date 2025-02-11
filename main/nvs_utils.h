#ifndef NVS_UTILS_H
#define NVS_UTILS_H
// ─────────────────────────────────────────────────────────────────────────────
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "measurement.h"
// ─────────────────────────────────────────────────────────────────────────────
esp_err_t init_nvs(void);
bool store_measurement_in_flash(Measurement *m);
bool find_measurement_in_flash(uint32_t timestamp, Measurement *result);
void clear_flash_storage(void);
uint32_t get_flash_usage_percent(void);
bool retrieve_measurement_from_flash(uint32_t timestamp, Measurement *m);
bool retrieve_measurement_from_edge(uint32_t timestamp, Measurement *m); // Declaration only
// ─────────────────────────────────────────────────────────────────────────────
int get_measurements_from_flash(uint32_t start_timestamp, uint32_t end_timestamp, Measurement *measurements, size_t max_measurements);
// ─────────────────────────────────────────────────────────────────────────────
#endif // NVS_UTILS_H
