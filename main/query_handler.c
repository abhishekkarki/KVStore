#include "query_handler.h"
#include "mqtt_utils.h"
#include "buffer.h"
#include "nvs_utils.h"
#include "cJSON.h"
#include "esp_log.h"
#include <inttypes.h>
// ─────────────────────────────────────────────────────────────────────────────
static const char *TAG = "QUERY_HANDLER";
// ─────────────────────────────────────────────────────────────────────────────
#define MAX_MEASUREMENTS 50  // Arbitrary max array size for single query
// You can define this if your measurement interval is exactly 20 seconds:
#define MEASUREMENT_INTERVAL_SEC 20


// ─────────────────────────────────────────────────────────────────────────────
// Function to send an error response for a single timestamp
void send_error_response(uint32_t timestamp, const char *response_topic) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"error\":\"Measurement not found\",\"timestamp\":%" PRIu32 "}", 
             timestamp);

    int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published error response for timestamp %"PRIu32" to device broker, msg_id=%d", timestamp, msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish error response for timestamp %"PRIu32, timestamp);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Function to send an error response for a timestamp range
void send_error_response_range(uint32_t start_timestamp, uint32_t end_timestamp, const char *response_topic) {
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"error\":\"Measurements not found in the range\",\"start_timestamp\":%" PRIu32 ",\"end_timestamp\":%" PRIu32 "}",
             start_timestamp, end_timestamp);

    int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published error response to device broker, msg_id=%d for range [%"PRIu32",%"PRIu32"]",
                 msg_id, start_timestamp, end_timestamp);
    } else {
        ESP_LOGE(TAG, "Failed to publish error response for range [%"PRIu32",%"PRIu32"]", start_timestamp, end_timestamp);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper to send an array of measurements
void send_measurements_response(Measurement *measurements, int count, const char *response_topic) {
    cJSON *json_array = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "timestamp",   measurements[i].timestamp);
        cJSON_AddNumberToObject(item, "temperature", measurements[i].temperature);
        // Optionally add the dirty_bit if you want debugging info
        cJSON_AddNumberToObject(item, "dirty_bit",   measurements[i].dirty_bit);

        cJSON_AddItemToArray(json_array, item);
    }

    char *payload = cJSON_PrintUnformatted(json_array);
    cJSON_Delete(json_array);

    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON payload for measurement array");
        return;
    }

    int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published %d measurements to device broker, msg_id=%d", count, msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish measurements to device broker");
    }

    free(payload);
}

// ─────────────────────────────────────────────────────────────────────────────
void process_query_message(const char *message) {
    ESP_LOGI(TAG, "Processing query message: %s", message);
    cJSON *json = cJSON_Parse(message);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse query message");
        return;
    }

    const cJSON *action = cJSON_GetObjectItem(json, "action");
    if (action == NULL || !cJSON_IsString(action)) {
        ESP_LOGE(TAG, "Missing or invalid 'action' in query message");
        cJSON_Delete(json);
        return;
    }

    if (strcmp(action->valuestring, "get_data_range") == 0) {
        const cJSON *start_ts = cJSON_GetObjectItem(json, "start_timestamp");
        const cJSON *end_ts   = cJSON_GetObjectItem(json,   "end_timestamp");
        const cJSON *response_topic = cJSON_GetObjectItem(json, "response_topic");

        const char *resp_topic = "esp32/response"; // Default response topic
        if (cJSON_IsString(response_topic)) {
            resp_topic = response_topic->valuestring;
        }

        if (cJSON_IsNumber(start_ts) && cJSON_IsNumber(end_ts)) {
            uint32_t start_timestamp = (uint32_t)start_ts->valuedouble;
            uint32_t end_timestamp   = (uint32_t)end_ts->valuedouble;
            if (end_timestamp < start_timestamp) {
                ESP_LOGW(TAG, "Invalid range: end < start");
                send_error_response_range(start_timestamp, end_timestamp, resp_topic);
                cJSON_Delete(json);
                return;
            }

            // Estimate how many we expect in that range, given 20s interval
            uint32_t dt = end_timestamp - start_timestamp;
            uint32_t expectedCount = (dt / MEASUREMENT_INTERVAL_SEC) + 1;

            size_t max_measurements = BUFFER_CAPACITY_MACRO * 3; // or some bigger number
            Measurement *measurements = malloc(sizeof(Measurement) * max_measurements);
            if (!measurements) {
                ESP_LOGE(TAG, "Failed to allocate memory for measurements (size=%u)",
                         (unsigned)(sizeof(Measurement) * max_measurements));
                cJSON_Delete(json);
                return;
            }

            ESP_LOGI(TAG, "Handling range query: [%"PRIu32", %"PRIu32"], expectedCount=%"PRIu32,
                     start_timestamp, end_timestamp, expectedCount);

            /*--------------------------------------------------------
             * 1) Check buffer coverage
             *-------------------------------------------------------*/
            // If the entire range is within buffer_earliest_ts..buffer_latest_ts
            // and expectedCount isn't huge, we might skip flash entirely.
            bool entireRangeInBuffer = false;
            if (start_timestamp >= buffer_earliest_ts && end_timestamp <= buffer_latest_ts) {
                entireRangeInBuffer = true;
            }

            int found_in_buffer = get_measurements_from_buffer(start_timestamp, end_timestamp,
                                                               measurements, max_measurements);

            ESP_LOGI(TAG, "get_measurements_from_buffer found=%d", found_in_buffer);

            if (entireRangeInBuffer && (uint32_t)found_in_buffer == expectedCount) {
                ESP_LOGI(TAG, "All %d data points are already in buffer. Skipping flash.", found_in_buffer);
                send_measurements_response(measurements, found_in_buffer, resp_topic);
                free(measurements);
                cJSON_Delete(json);
                return;
            }

            /*--------------------------------------------------------
             * 2) If partial or none in buffer => also check flash
             *-------------------------------------------------------*/
            int total_found = found_in_buffer; 
            if ((uint32_t)found_in_buffer < expectedCount) {
                // Attempt to get the remainder from flash
                // We'll store them into measurements array after the existing ones
                ESP_LOGI(TAG, "Measurements not fully in buffer => checking flash");
                int found_in_flash = get_measurements_from_flash(start_timestamp, end_timestamp,
                                                                 &measurements[found_in_buffer],
                                                                 max_measurements - found_in_buffer);
                if (found_in_flash < 0) {
                    // Error reading flash
                    ESP_LOGE(TAG, "Error reading from flash, aborting query");
                    free(measurements);
                    cJSON_Delete(json);
                    return;
                }
                if (found_in_flash > 0) {
                    ESP_LOGI(TAG, "Found %d from flash", found_in_flash);
                }
                total_found += found_in_flash;
            }

            ESP_LOGI(TAG, "Retrieved %d measurements total (buffer+flash).", total_found);

            if (total_found == 0) {
                // Data not available locally, or truly none found
                ESP_LOGI(TAG, "Data not available locally. Retrieving from edge device or error...");
                // For now, just send an error
                send_error_response_range(start_timestamp, end_timestamp, resp_topic);
                free(measurements);
                cJSON_Delete(json);
                return;
            }

            /*--------------------------------------------------------
             * 3) Optionally bring some or all from flash => buffer
             *-------------------------------------------------------*/
            if (total_found <= (int)(BUFFER_CAPACITY_MACRO * 0.8)) {
                // Bring them all to the buffer, for caching
                for (int i = found_in_buffer; i < total_found; i++) {
                    buffer_add_measurement(&measurements[i]);
                }
            } else {
                ESP_LOGI(TAG, "Data too large to bring fully into buffer => skipping buffer load");
            }

            /*--------------------------------------------------------
             * 4) Finally respond
             *-------------------------------------------------------*/
            send_measurements_response(measurements, total_found, resp_topic);
            free(measurements);

        } else {
            ESP_LOGE(TAG, "Invalid or missing 'start_timestamp' or 'end_timestamp' in query");
        }
    } else {
        ESP_LOGE(TAG, "Invalid action in query message: %s", action->valuestring);
    }

    cJSON_Delete(json);
}

// ─────────────────────────────────────────────────────────────────────────────

