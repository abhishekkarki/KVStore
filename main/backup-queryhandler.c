// query_handler.c

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

// // Function to send an error response
// void send_error_response(uint32_t timestamp, const char *response_topic) {
//     char payload[128];
//     snprintf(payload, sizeof(payload),
//              "{\"error\":\"Measurement not found\",\"timestamp\":%" PRIu32 "}", timestamp);

//     int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
//     if (msg_id != -1) {
//         ESP_LOGI(TAG, "Published error response to device MQTT broker, msg_id=%d", msg_id);
//     } else {
//         ESP_LOGE(TAG, "Failed to publish error response to device MQTT broker");
//     }
// }


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


// void process_query_message(const char *message) {
//     ESP_LOGI(TAG, "Processing query message: %s", message);
//     cJSON *json = cJSON_Parse(message);
//     if (!json) {
//         ESP_LOGE(TAG, "Failed to parse query message");
//         return;
//     }

//     const cJSON *action = cJSON_GetObjectItem(json, "action");
//     if (!action || !cJSON_IsString(action)) {
//         ESP_LOGE(TAG, "Missing or invalid 'action'");
//         cJSON_Delete(json);
//         return;
//     }

//     if (strcmp(action->valuestring, "get_data_range") == 0) {
//         const cJSON *start_ts = cJSON_GetObjectItem(json, "start_timestamp");
//         const cJSON *end_ts   = cJSON_GetObjectItem(json, "end_timestamp");
//         const cJSON *response_topic = cJSON_GetObjectItem(json, "response_topic");

//         const char *resp_topic = "esp32/response";
//         if (cJSON_IsString(response_topic)) {
//             resp_topic = response_topic->valuestring;
//         }

//         if (cJSON_IsNumber(start_ts) && cJSON_IsNumber(end_ts)) {
//             uint32_t start_timestamp = (uint32_t)start_ts->valuedouble;
//             uint32_t end_timestamp   = (uint32_t)end_ts->valuedouble;
//             ESP_LOGI(TAG, "Handling range query: [%"PRIu32", %"PRIu32"]", start_timestamp, end_timestamp);

//             size_t max_measurements = BUFFER_CAPACITY_MACRO * 2; // allow some extra
//             Measurement *measurements = malloc(sizeof(Measurement) * max_measurements);
//             if (!measurements) {
//                 ESP_LOGE(TAG, "Failed to allocate memory for measurements array");
//                 cJSON_Delete(json);
//                 return;
//             }

//             // 1) Buffer
//             int count_buf = get_measurements_from_buffer(start_timestamp, end_timestamp, measurements, max_measurements);
//             if (count_buf < 0) {
//                 count_buf = 0; // treat error as no data
//             }
//             // 2) Flash if needed
//             int count_flash = 0;
//             if (count_buf < (int)max_measurements) {
//                 Measurement *flashSlot = &measurements[count_buf];
//                 size_t flash_cap = max_measurements - count_buf;
//                 count_flash = get_measurements_from_flash(start_timestamp, end_timestamp, flashSlot, flash_cap);
//                 if (count_flash < 0) count_flash = 0;
//             }
//             int total = count_buf + count_flash;
//             ESP_LOGI(TAG, "Retrieved %d measurements total (buffer+flash).", total);

//             if (total > 0) {
//                 // Optionally bring flash portion into buffer
//                 if (count_flash > 0) {
//                     // e.g. if small enough
//                     if (count_flash <= (int)(BUFFER_CAPACITY_MACRO * 0.8f)) {
//                         // bring them in
//                         Measurement *flashSlot = &measurements[count_buf];
//                         for (int i = 0; i < count_flash; i++) {
//                             buffer_add_measurement(&flashSlot[i]);
//                         }
//                         ESP_LOGI(TAG, "Brought %d flash items into buffer", count_flash);
//                     } else {
//                         ESP_LOGW(TAG, "Flash portion too large => skipping buffer load");
//                     }
//                 }
//                 // Send them all
//                 send_measurements_response(measurements, total, resp_topic);
//             } else {
//                 // Not found locally => Edge
//                 ESP_LOGI(TAG, "Data not in local device => retrieve from edge or respond error");
//                 // For now, do error
//                 send_error_response_range(start_timestamp, end_timestamp, resp_topic);
//             }
//             free(measurements);
//         } else {
//             ESP_LOGE(TAG, "Missing or invalid 'start_timestamp'/'end_timestamp'");
//         }
//     } else {
//         ESP_LOGE(TAG, "Invalid action in query message: %s", action->valuestring);
//     }

//     cJSON_Delete(json);
// }

// ─────────────────────────────────────────────────────────────────────────────
void process_query_message(const char *message)
{
    ESP_LOGI(TAG, "Processing query message: %s", message);
    cJSON *json = cJSON_Parse(message);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse query message");
        return;
    }

    const cJSON *action = cJSON_GetObjectItem(json, "action");
    if (!action || !cJSON_IsString(action)) {
        ESP_LOGE(TAG, "Missing or invalid 'action' in query");
        cJSON_Delete(json);
        return;
    }

    // Checking if we have a "get_data_range" action
    if (strcmp(action->valuestring, "get_data_range") == 0) {
        const cJSON *start_ts = cJSON_GetObjectItem(json, "start_timestamp");
        const cJSON *end_ts   = cJSON_GetObjectItem(json, "end_timestamp");
        const cJSON *response_topic = cJSON_GetObjectItem(json, "response_topic");

        const char *resp_topic = "esp32/response"; // default
        if (cJSON_IsString(response_topic)) {
            resp_topic = response_topic->valuestring;
        }

        // Validate range
        if (cJSON_IsNumber(start_ts) && cJSON_IsNumber(end_ts)) {
            uint32_t start_timestamp = (uint32_t)start_ts->valuedouble;
            uint32_t end_timestamp   = (uint32_t)end_ts->valuedouble;

            ESP_LOGI(TAG, "Handling range query: [%"PRIu32", %"PRIu32"] -> response_topic=%s",
                     start_timestamp, end_timestamp, resp_topic);

            // Allocate array for combined results
            Measurement *measurements = malloc(sizeof(Measurement) * MAX_MEASUREMENTS);
            if (!measurements) {
                ESP_LOGE(TAG, "Failed to allocate memory for measurements array");
                cJSON_Delete(json);
                return;
            }

            // 1) Retrieve from buffer
            int buf_count = get_measurements_from_buffer(start_timestamp, end_timestamp, measurements, MAX_MEASUREMENTS);
            if (buf_count < 0) buf_count = 0; // treat negative as zero

            ESP_LOGI(TAG, "Buffer returned %d measurement(s) in range [%"PRIu32",%"PRIu32"]", 
                     buf_count, start_timestamp, end_timestamp);

            // If buffer is enough, respond immediately
            // But let's also check flash for additional data
            int flash_count = 0;
            if (buf_count < MAX_MEASUREMENTS) {
                // We have space left in the array to store from flash
                Measurement *flashSlot = &measurements[buf_count];
                size_t flashCapacity = MAX_MEASUREMENTS - buf_count;

                flash_count = get_measurements_from_flash(start_timestamp, end_timestamp, flashSlot, flashCapacity);
                if (flash_count < 0) flash_count = 0; // treat negative as zero

                ESP_LOGI(TAG, "Flash returned %d measurement(s) in range [%"PRIu32",%"PRIu32"]", 
                         flash_count, start_timestamp, end_timestamp);
            }

            int total_count = buf_count + flash_count;
            ESP_LOGI(TAG, "Total found locally: %d (buf=%d + flash=%d)", total_count, buf_count, flash_count);

            if (total_count > 0) {
                // 2) Optionally bring flash portion into buffer
                if (flash_count > 0) {
                    // Decide if we want to push them into buffer
                    if (flash_count <= (BUFFER_CAPACITY_MACRO * 0.8f)) {
                        // bring them in
                        Measurement *flashSlot = &measurements[buf_count];
                        for (int i = 0; i < flash_count; i++) {
                            // LOG: 
                            ESP_LOGI(TAG, "Bringing TS=%"PRIu32" from flash to buffer", flashSlot[i].timestamp);
                            buffer_add_measurement(&flashSlot[i]);
                        }
                        ESP_LOGI(TAG, "Brought %d flash items into buffer", flash_count);
                    } else {
                        ESP_LOGW(TAG, "Flash portion too big to store in buffer => skipping buffer load");
                    }
                }

                // 3) Return all measurements found
                ESP_LOGI(TAG, "Returning %d total measurement(s) from local device", total_count);
                send_measurements_response(measurements, total_count, resp_topic);
            } else {
                // 4) Not found locally => retrieve from edge or respond error
                ESP_LOGI(TAG, "Data not in buffer or flash => retrieving from edge or error");
                send_error_response_range(start_timestamp, end_timestamp, resp_topic);
            }

            free(measurements);
        } else {
            ESP_LOGE(TAG, "Invalid or missing start_timestamp/end_timestamp in query");
        }
    } 
    else {
        // if not get_data_range => log
        ESP_LOGE(TAG, "Invalid action: %s", action->valuestring);
    }

    cJSON_Delete(json);
}



// ─────────────────────────────────────────────────────────────────────────────

// // The main unify method
// static int unify_and_respond(uint32_t start_timestamp, uint32_t end_timestamp, const char *resp_topic)
// {
//     // 1) Retrieve data from the buffer
//     size_t max_buf = BUFFER_CAPACITY_MACRO;
//     Measurement *buffer_results = malloc(sizeof(Measurement) * max_buf);
//     if (!buffer_results) {
//         ESP_LOGE(TAG, "Failed to allocate memory for buffer_results");
//         return 0;
//     }
//     int buffer_count = get_measurements_from_buffer(start_timestamp, end_timestamp, buffer_results, max_buf);

//     // 2) If we might have partial data, let's see which timestamps are still missing
//     //    We'll only look up flash for what's missing. 
//     //    But to do that, we actually need to figure out *which* timestamps are missing, or we do a simpler approach:
//     //    We just retrieve from flash *any* measurement in [start, end], then unify.
//     //    Then we deduplicate. 
//     size_t max_flash = BUFFER_CAPACITY_MACRO; // Or another number you prefer
//     Measurement *flash_results = malloc(sizeof(Measurement) * max_flash);
//     if (!flash_results) {
//         ESP_LOGE(TAG, "Failed to allocate memory for flash_results");
//         free(buffer_results);
//         return 0;
//     }
//     int flash_count = get_measurements_from_flash(start_timestamp, end_timestamp, flash_results, max_flash);

//     // 3) Combine them into one array
//     Measurement *combined = malloc(sizeof(Measurement) * (buffer_count + flash_count));
//     if (!combined) {
//         ESP_LOGE(TAG, "Failed to allocate memory for combined results");
//         free(buffer_results);
//         free(flash_results);
//         return 0;
//     }

//     // copy buffer portion
//     int combined_index = 0;
//     for (int i = 0; i < buffer_count; i++) {
//         combined[combined_index++] = buffer_results[i];
//     }
//     // copy flash portion
//     for (int j = 0; j < flash_count; j++) {
//         // If you want to skip duplicates, do a check here. For simplicity, we do none.
//         combined[combined_index++] = flash_results[j];
//     }

//     int total_found = combined_index;

//     // free local arrays
//     free(buffer_results);
//     free(flash_results);

//     if (total_found == 0) {
//         // no data found in buffer or flash
//         free(combined);
//         return 0;
//     }

//     ESP_LOGI(TAG, "Retrieved %d measurements total (buffer+flash).", total_found);

//     // 4) Optionally, bring flash portion to the buffer if it fits
//     //    Let’s say if flash_count <=  (BUFFER_CAPACITY_MACRO * 0.8)
//     //    This is a quick approach: we only do it if the entire flash result is small enough
//     if (flash_count > 0 && flash_count <= (BUFFER_CAPACITY_MACRO * 0.8)) {
//         ESP_LOGI(TAG, "Storing flash portion back into buffer for future queries...");
//         for (int i = buffer_count; i < total_found; i++) {
//             // i in [buffer_count..(total_found-1)] is the flash portion
//             buffer_add_measurement(&combined[i]);
//         }
//     }

//     // 5) Send it back
//     send_measurements_response(combined, total_found, resp_topic);

//     free(combined);
//     return total_found;
// }


// void send_measurements_response(Measurement *measurements, int count, const char *response_topic) {
//     cJSON *json_array = cJSON_CreateArray();
//     for (int i = 0; i < count; i++) {
//         cJSON *item = cJSON_CreateObject();
//         cJSON_AddNumberToObject(item, "timestamp", measurements[i].timestamp);
//         cJSON_AddNumberToObject(item, "temperature", measurements[i].temperature);
//         cJSON_AddItemToArray(json_array, item);
//     }

//     char *payload = cJSON_PrintUnformatted(json_array);
//     cJSON_Delete(json_array);

//     if (payload == NULL) {
//         ESP_LOGE(TAG, "Failed to create JSON payload");
//         return;
//     }

//     int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
//     if (msg_id != -1) {
//         ESP_LOGI(TAG, "Published measurements response to device MQTT broker, msg_id=%d", msg_id);
//     } else {
//         ESP_LOGE(TAG, "Failed to publish measurements response to device MQTT broker");
//     }

//     free(payload);
// }

// void send_error_response_range(uint32_t start_timestamp, uint32_t end_timestamp, const char *response_topic) {
//     char payload[256];
//     snprintf(payload, sizeof(payload),
//              "{\"error\":\"Measurements not found in the range\",\"start_timestamp\":%" PRIu32 ",\"end_timestamp\":%" PRIu32 "}",
//              start_timestamp, end_timestamp);

//     int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
//     if (msg_id != -1) {
//         ESP_LOGI(TAG, "Published error response to device MQTT broker, msg_id=%d", msg_id);
//     } else {
//         ESP_LOGE(TAG, "Failed to publish error response to device MQTT broker");
//     }
// }
