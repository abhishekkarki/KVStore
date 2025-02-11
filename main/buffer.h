#ifndef BUFFER_H
#define BUFFER_H
// ─────────────────────────────────────────────────────────────────────────────
#include "measurement.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// ─────────────────────────────────────────────────────────────────────────────
#define BUFFER_CAPACITY_MACRO 10
#define BUFFER_THRESHOLD_PERCENT 80
// ─────────────────────────────────────────────────────────────────────────────
extern Measurement buffer[];
extern int buffer_head;
extern int buffer_tail;
extern int buffer_count;
extern SemaphoreHandle_t buffer_mutex;
extern const int BUFFER_CAPACITY;
// ─────────────────────────────────────────────────────────────────────────────
/* Track earliest & latest timestamps in the buffer */
extern uint32_t buffer_earliest_ts;
extern uint32_t buffer_latest_ts;
// ─────────────────────────────────────────────────────────────────────────────
void buffer_init(void);
void buffer_add_measurement(Measurement *m);
bool buffer_is_threshold_full(void);
void buffer_push_to_flash(void);
bool find_measurement_in_buffer(uint32_t timestamp, Measurement *result);
// ─────────────────────────────────────────────────────────────────────────────
/* Helper to re-scan the buffer and find a new earliest_ts if we evict oldest */
void update_buffer_earliest(void);
// ─────────────────────────────────────────────────────────────────────────────
/*  Retrieve all measurements in the range from the buffer into `measurements` array */
int get_measurements_from_buffer(uint32_t start_timestamp, uint32_t end_timestamp,
                                 Measurement *measurements, size_t max_measurements);
// ─────────────────────────────────────────────────────────────────────────────
#endif // BUFFER_H