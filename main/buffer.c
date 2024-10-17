#include "buffer.h"
#include "nvs_utils.h"
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static Measurement buffer[BUFFER_SIZE];
static int buffer_head = 0; // Index for adding new measurements
static int buffer_tail = 0; // Index for removing measurements
static int buffer_count = 0; // Number of measurements in the buffer

static SemaphoreHandle_t buffer_mutex;

void buffer_init() {
    buffer_head = 0;
    buffer_tail = 0;
    buffer_count = 0;
    buffer_mutex = xSemaphoreCreateMutex();
}

void buffer_add_measurement(Measurement *m) {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    buffer[buffer_head] = *m;
    buffer_head = (buffer_head + 1) % BUFFER_SIZE;
    if (buffer_count < BUFFER_SIZE) {
        buffer_count++;
    } else {
        // Buffer is full; overwrite the oldest entry
        buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    }
    xSemaphoreGive(buffer_mutex);
}

bool buffer_is_full() {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    bool full = buffer_count >= BUFFER_SIZE;
    xSemaphoreGive(buffer_mutex);
    return full;
}

bool buffer_is_90_percent_full() {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    bool full = buffer_count >= (BUFFER_SIZE * 90 / 100);
    xSemaphoreGive(buffer_mutex);
    return full;
}

void buffer_push_to_flash() {
    int entries_to_push = BUFFER_SIZE / 2; // Push half of the entries
    int entries_pushed = 0;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count && entries_pushed < entries_to_push; i++) {
        int index = (buffer_tail + i) % BUFFER_SIZE;
        Measurement *m = &buffer[index];

        // Only push entries with dirty bit 0 (not in flash)
        if (m->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
            if (store_measurement_in_flash(m)) {
                m->dirty_bit = DIRTY_BIT_IN_FLASH; // Mark as in flash
                entries_pushed++;
            } else {
                // Handle storage error if needed
            }
        }
    }
    xSemaphoreGive(buffer_mutex);
}

bool buffer_find_measurement(uint32_t timestamp, Measurement *result) {
    bool found = false;
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count; i++) {
        int index = (buffer_tail + i) % BUFFER_SIZE;
        Measurement *m = &buffer[index];
        if (m->timestamp == timestamp) {
            *result = *m;
            found = true;
            break;
        }
    }
    xSemaphoreGive(buffer_mutex);
    return found;
}

void buffer_update_dirty_bit(uint32_t timestamp, uint8_t dirty_bit) {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count; i++) {
        int index = (buffer_tail + i) % BUFFER_SIZE;
        Measurement *m = &buffer[index];
        if (m->timestamp == timestamp) {
            m->dirty_bit = dirty_bit;
            break;
        }
    }
    xSemaphoreGive(buffer_mutex);
}