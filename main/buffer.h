#ifndef BUFFER_H
#define BUFFER_H

#include "measurement.h"
#include <stdbool.h>

#define BUFFER_SIZE 500 // Adjust as needed

void buffer_init();
void buffer_add_measurement(Measurement *m);
bool buffer_is_full();
bool buffer_is_90_percent_full();
void buffer_push_to_flash();
bool buffer_find_measurement(uint32_t timestamp, Measurement *result);
void buffer_update_dirty_bit(uint32_t timestamp, uint8_t dirty_bit);

#endif // BUFFER_H