#include "buffer.h"
#include "nvs_utils.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BUFFER";

const int BUFFER_CAPACITY = BUFFER_CAPACITY_MACRO; // Match the macro value

Measurement buffer[BUFFER_CAPACITY_MACRO];

int buffer_head = 0;
int buffer_tail = 0;
int buffer_count = 0;
SemaphoreHandle_t buffer_mutex;

void buffer_init() {
    buffer_head = 0;
    buffer_tail = 0;
    buffer_count = 0;
    buffer_mutex = xSemaphoreCreateMutex();
}

void buffer_add_measurement(Measurement *m) {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return;
    }

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    buffer[buffer_head] = *m;
    buffer_head = (buffer_head + 1) % BUFFER_CAPACITY_MACRO;

    if (buffer_count < BUFFER_CAPACITY_MACRO) {
        buffer_count++;
    } else {
        // Overwrite oldest data
        buffer_tail = (buffer_tail + 1) % BUFFER_CAPACITY_MACRO;
    }

    xSemaphoreGive(buffer_mutex);
}

bool buffer_is_threshold_full() {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return false;
    }

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    bool result = (buffer_count >= (BUFFER_CAPACITY_MACRO * BUFFER_THRESHOLD_PERCENT / 100));
    xSemaphoreGive(buffer_mutex);
    return result;
}

void buffer_push_to_flash() {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return;
    }

    int entries_to_push = BUFFER_CAPACITY_MACRO / 2; // Push half of the entries
    int entries_pushed = 0;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count && entries_pushed < entries_to_push; i++) {
        int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
        Measurement *m = &buffer[index];

        // Only process entries with dirty bit DIRTY_BIT_BUFFER_ONLY (0)
        if (m->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
            if (store_measurement_in_flash(m)) {
                m->dirty_bit = DIRTY_BIT_IN_FLASH; // Mark as in flash
                entries_pushed++;
            } else {
                ESP_LOGE(TAG, "Failed to store measurement in flash");
                // Handle storage error if needed
            }
        }
    }
    xSemaphoreGive(buffer_mutex);
}

bool find_measurement_in_buffer(uint32_t timestamp, Measurement *result) {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return false;
    }

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count; i++) {
        int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
        if (buffer[index].timestamp == timestamp) {
            *result = buffer[index];
            xSemaphoreGive(buffer_mutex);
            return true;
        }
    }
    xSemaphoreGive(buffer_mutex);
    return false;
}