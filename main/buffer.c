#include "buffer.h"
#include "nvs_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "BUFFER";

static Measurement buffer[BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;
static int buffer_count = 0;
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
        // Overwrite oldest data
        buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    }

    xSemaphoreGive(buffer_mutex);
}

bool buffer_is_threshold_full() {
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    bool result = (buffer_count >= (BUFFER_SIZE * BUFFER_THRESHOLD_PERCENT / 100));
    xSemaphoreGive(buffer_mutex);
    return result;
}

void buffer_push_to_flash() {
    int entries_to_push = BUFFER_SIZE / 2; // Push half of the entries
    int entries_pushed = 0;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < buffer_count && entries_pushed < entries_to_push; i++) {
        int index = (buffer_tail + i) % BUFFER_SIZE;
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