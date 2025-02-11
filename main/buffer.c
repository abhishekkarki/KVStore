#include "buffer.h"
#include "nvs_utils.h"
#include "esp_log.h"
#include <string.h>
#include "timestamp_list.h"
// ─────────────────────────────────────────────────────────────────────────────
static const char *TAG = "BUFFER";
// ─────────────────────────────────────────────────────────────────────────────
const int BUFFER_CAPACITY = BUFFER_CAPACITY_MACRO; // Match the macro value
// ─────────────────────────────────────────────────────────────────────────────
Measurement buffer[BUFFER_CAPACITY_MACRO];
// ─────────────────────────────────────────────────────────────────────────────
int buffer_head = 0;
int buffer_tail = 0;
int buffer_count = 0;
SemaphoreHandle_t buffer_mutex;
// ─────────────────────────────────────────────────────────────────────────────
/* track earliest & latest timestamps in buffer */
uint32_t buffer_earliest_ts = 0;
uint32_t buffer_latest_ts  = 0;
// ─────────────────────────────────────────────────────────────────────────────
void buffer_init() {
    buffer_head = 0;
    buffer_tail = 0;
    buffer_count = 0;
    buffer_mutex = xSemaphoreCreateMutex();

    /* Initialize earliest & latest to some neutral values */
    buffer_earliest_ts = 0;
    buffer_latest_ts   = 0;
}
// ─────────────────────────────────────────────────────────────────────────────
void update_buffer_earliest(void)
{
    // We'll find the minimum timestamp in the current buffer.
    // This is O(buffer_count), typically small enough for embedded scenarios.
    if (buffer_count == 0) {
        buffer_earliest_ts = 0;
        return;
    }

    uint32_t min_ts = UINT32_MAX;
    for (int i = 0; i < buffer_count; i++) {
        int idx = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
        if (buffer[idx].timestamp < min_ts) {
            min_ts = buffer[idx].timestamp;
        }
    }
    buffer_earliest_ts = min_ts;
}
// ─────────────────────────────────────────────────────────────────────────────
void buffer_add_measurement(Measurement *m) {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return;
    }

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    // Check if buffer is full
    if (buffer_count >= BUFFER_CAPACITY_MACRO) {
        // Buffer is full, need to make space
        // Evict the oldest entry (buffer_tail)
        Measurement *oldest = &buffer[buffer_tail];

        ESP_LOGI(TAG, "Evicting oldest entry timestamp=%" PRIu32 " from buffer", oldest->timestamp);

        if (oldest->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
            // Push to flash before eviction
            if (store_measurement_in_flash(oldest)) {
                append_timestamp_to_list(oldest->timestamp);
                oldest->dirty_bit = DIRTY_BIT_IN_FLASH;
                ESP_LOGI(TAG, "Evicted entry stored to flash, timestamp=%" PRIu32, oldest->timestamp);
            } else {
                ESP_LOGE(TAG, "Failed to store measurement in flash during eviction");
                // Handle error if necessary
            }
        }
        // Move buffer_tail forward
        buffer_tail = (buffer_tail + 1) % BUFFER_CAPACITY_MACRO;
        buffer_count--;

        // We removed the oldest; re-scan to update buffer_earliest_ts
        update_buffer_earliest();
    }

    // Add new measurement
    buffer[buffer_head] = *m;
    buffer_head = (buffer_head + 1) % BUFFER_CAPACITY_MACRO;
    buffer_count++;

    // If this is the only entry, set earliest_ts = latest_ts = current
    if (buffer_count == 1) {
        buffer_earliest_ts = m->timestamp;
        buffer_latest_ts   = m->timestamp;
    } else {
        // update earliest & latest if needed
        if (m->timestamp < buffer_earliest_ts) {
            buffer_earliest_ts = m->timestamp;
        }
        if (m->timestamp > buffer_latest_ts) {
            buffer_latest_ts   = m->timestamp;
        }
    }

    xSemaphoreGive(buffer_mutex);
    ESP_LOGI(TAG, "Added measurement timestamp=%" PRIu32 ", dirty_bit=%u to buffer (count=%d)",
             m->timestamp, m->dirty_bit, buffer_count);
}
// ─────────────────────────────────────────────────────────────────────────────
bool buffer_is_threshold_full() {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return false;
    }

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    bool result = (buffer_count >= (BUFFER_CAPACITY_MACRO * BUFFER_THRESHOLD_PERCENT / 100));
    xSemaphoreGive(buffer_mutex);
    // Before returning result checking the threshold
    if (result) {
        ESP_LOGI(TAG, "Buffer threshold reached. Current count = %d, capacity = %d", 
                 buffer_count, BUFFER_CAPACITY);
    }
    return result;
}
// ─────────────────────────────────────────────────────────────────────────────
void buffer_push_to_flash() {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return;
    }
    ESP_LOGI(TAG, "Buffer threshold triggered. Pushing half of buffer entries to flash...");
    int entries_to_push = BUFFER_CAPACITY_MACRO / 2; // Push half of the entries
    int entries_pushed = 0;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count && entries_pushed < entries_to_push; i++) {
        int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
        Measurement *m = &buffer[index];

        // Only process entries with dirty bit DIRTY_BIT_BUFFER_ONLY (0)
        if (m->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
            // Update dirty bit to DIRTY_BIT_IN_FLASH
            m->dirty_bit = DIRTY_BIT_IN_FLASH;

            // Store measurement in flash
            if (store_measurement_in_flash(m)) {
                // Append timestamp to FIFO list
                append_timestamp_to_list(m->timestamp);
                entries_pushed++;
                ESP_LOGI(TAG, "Pushed timestamp=%" PRIu32 " from buffer to flash", m->timestamp);
            } else {
                ESP_LOGE(TAG, "Failed to store measurement in flash");
            }
        }
    }
    xSemaphoreGive(buffer_mutex);
}
// ─────────────────────────────────────────────────────────────────────────────
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
// ─────────────────────────────────────────────────────────────────────────────
// Retrieves measurements within the specified timestamp range from the buffer
int get_measurements_from_buffer(uint32_t start_timestamp, uint32_t end_timestamp,
                                 Measurement *measurements, size_t max_measurements) {
    if (buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Buffer mutex not initialized");
        return -1;
    }

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    int count = 0;
    for (int i = 0; i < buffer_count; i++) {
        int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
        Measurement *m = &buffer[index];

        if (m->timestamp >= start_timestamp && m->timestamp <= end_timestamp) {
            if (count < (int)max_measurements) {
                measurements[count++] = *m;
            } else {
                // Exceeded the maximum measurements that can be stored in the provided array
                ESP_LOGW(TAG, "get_measurements_from_buffer: reached max_measurements limit=%u", 
                         (unsigned)max_measurements);
                break;
            }
        }
    }
    xSemaphoreGive(buffer_mutex);

    if (count > 0) {
        ESP_LOGI(TAG, "get_measurements_from_buffer: found %d matching entries in buffer", count);
    }
    return count; // Return the number of measurements found
}