// #include "buffer.h"
// #include "nvs_utils.h"
// #include "esp_log.h"
// #include <string.h>
// #include "timestamp_list.h"

// static const char *TAG = "BUFFER";

// const int BUFFER_CAPACITY = BUFFER_CAPACITY_MACRO; // Match the macro value

// Measurement buffer[BUFFER_CAPACITY_MACRO];

// int buffer_head = 0;
// int buffer_tail = 0;
// int buffer_count = 0;
// SemaphoreHandle_t buffer_mutex;

// void buffer_init() {
//     buffer_head = 0;
//     buffer_tail = 0;
//     buffer_count = 0;
//     buffer_mutex = xSemaphoreCreateMutex();
// }

// void buffer_add_measurement(Measurement *m) {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return;
//     }

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);

//     // Check if buffer is full
//     if (buffer_count >= BUFFER_CAPACITY_MACRO) {
//         // Buffer is full, need to make space
//         // Evict the oldest entry (buffer_tail)
//         Measurement *oldest = &buffer[buffer_tail];
//         if (oldest->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
//             // Push to flash before eviction
//             if (store_measurement_in_flash(oldest)) {
//                 append_timestamp_to_list(oldest->timestamp);
//                 oldest->dirty_bit = DIRTY_BIT_IN_FLASH;
//             } else {
//                 ESP_LOGE(TAG, "Failed to store measurement in flash during eviction");
//                 // Handle error if necessary
//             }
//         }
//         // Move buffer_tail forward
//         buffer_tail = (buffer_tail + 1) % BUFFER_CAPACITY_MACRO;
//         buffer_count--;
//     }

//     // Add new measurement
//     buffer[buffer_head] = *m;
//     buffer_head = (buffer_head + 1) % BUFFER_CAPACITY_MACRO;
//     buffer_count++;

//     xSemaphoreGive(buffer_mutex);
//     ESP_LOGI(TAG, "Added measurement timestamp = %" PRIu32 ", dirty_bit = %u to buffer", m->timestamp, m->dirty_bit);
// }

// bool buffer_is_threshold_full() {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return false;
//     }

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     bool result = (buffer_count >= (BUFFER_CAPACITY_MACRO * BUFFER_THRESHOLD_PERCENT / 100));
//     xSemaphoreGive(buffer_mutex);
//     // Before returning result checking the threshold
//     if (result) {
//         ESP_LOGI(TAG, "Buffer threshold reached. Current count = %d Capacity = %d", buffer_count, BUFFER_CAPACITY);
//     }
//     return result;
// }

// void buffer_push_to_flash() {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return;
//     }
//     ESP_LOGI(TAG, "Buffer threshold triggered. Pushing half of buffer entries to flash...");
//     int entries_to_push = BUFFER_CAPACITY_MACRO / 2; // Push half of the entries
//     int entries_pushed = 0;

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     for (int i = 0; i < buffer_count && entries_pushed < entries_to_push; i++) {
//         int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
//         Measurement *m = &buffer[index];

//         // Only process entries with dirty bit DIRTY_BIT_BUFFER_ONLY (0)
//         if (m->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
//             // Update dirty bit to DIRTY_BIT_IN_FLASH
//             m->dirty_bit = DIRTY_BIT_IN_FLASH;

//             // Store measurement in flash
//             if (store_measurement_in_flash(m)) {
//                 // Append timestamp to FIFO list
//                 append_timestamp_to_list(m->timestamp);
//                 entries_pushed++;
//                 ESP_LOGI(TAG, "Pushed timestamp = %" PRIu32 " from buffer to flash", m->timestamp);
//             } else {
//                 ESP_LOGE(TAG, "Failed to store measurement in flash");
//                 // Handle storage error if needed
//             }
//         }
//     }
//     xSemaphoreGive(buffer_mutex);
// }

// bool find_measurement_in_buffer(uint32_t timestamp, Measurement *result) {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return false;
//     }

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     for (int i = 0; i < buffer_count; i++) {
//         int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
//         if (buffer[index].timestamp == timestamp) {
//             *result = buffer[index];
//             xSemaphoreGive(buffer_mutex);
//             return true;
//         }
//     }
//     xSemaphoreGive(buffer_mutex);
//     return false;
// }

// // Retrieves measurements within the specified timestamp range from the buffer
// int get_measurements_from_buffer(uint32_t start_timestamp, uint32_t end_timestamp, Measurement *measurements, size_t max_measurements) {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return -1;
//     }

//     int count = 0;

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     for (int i = 0; i < buffer_count; i++) {
//         int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
//         Measurement *m = &buffer[index];

//         if (m->timestamp >= start_timestamp && m->timestamp <= end_timestamp) {
//             if (count < max_measurements) {
//                 measurements[count++] = *m;
//             } else {
//                 // Exceeded the maximum measurements that can be stored in the provided array
//                 break;
//             }
//         }
//     }
//     xSemaphoreGive(buffer_mutex);

//     return count; // Return the number of measurements found
// }