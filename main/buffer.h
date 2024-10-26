#ifndef BUFFER_H
#define BUFFER_H

#include "measurement.h"
#include <stdbool.h>

#define BUFFER_SIZE 10 // Reduced buffer size for testing
#define BUFFER_THRESHOLD_PERCENT 20 // Adjusted threshold for testing

void buffer_init(void);
void buffer_add_measurement(Measurement *m);
bool buffer_is_threshold_full(void);
void buffer_push_to_flash(void);

#endif // BUFFER_H