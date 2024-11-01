// buffer.h
#ifndef BUFFER_H
#define BUFFER_H

#include "measurement.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BUFFER_CAPACITY_MACRO 10
#define BUFFER_THRESHOLD_PERCENT 20

extern Measurement buffer[];
extern int buffer_head;
extern int buffer_tail;
extern int buffer_count;
extern SemaphoreHandle_t buffer_mutex;
extern const int BUFFER_CAPACITY;

void buffer_init(void);
void buffer_add_measurement(Measurement *m);
bool buffer_is_threshold_full(void);
void buffer_push_to_flash(void);
bool find_measurement_in_buffer(uint32_t timestamp, Measurement *result);

#endif // BUFFER_H