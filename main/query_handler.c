// query_handler.c

#include "query_handler.h"
#include "mqtt_utils.h"
#include "buffer.h"
#include "nvs_utils.h"
#include "cJSON.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "QUERY_HANDLER";

void process_query_message(const char *message) {
    ESP_LOGI(TAG, "Processing query message: %s", message);
    cJSON *json = cJSON_Parse(message);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse query message");
        return;
    }

    const cJSON *action = cJSON_GetObjectItem(json, "action");
    if (cJSON_IsString(action) && (strcmp(action->valuestring, "get_data") == 0)) {
        const cJSON *timestamp = cJSON_GetObjectItem(json, "timestamp");
        const cJSON *response_topic = cJSON_GetObjectItem(json, "response_topic");

        const char *resp_topic = "esp32/response"; // Default response topic
        if (cJSON_IsString(response_topic)) {
            resp_topic = response_topic->valuestring;
        }

        if (cJSON_IsNumber(timestamp)) {
            uint32_t ts = (uint32_t)timestamp->valuedouble;
            Measurement m;
            if (find_measurement_in_buffer(ts, &m)) {
                // Send response
                send_measurement_response(&m, resp_topic);
            } else if (retrieve_measurement_from_flash(ts, &m)) {
                // Bring it into the buffer
                m.dirty_bit = DIRTY_BIT_SENT_TO_EDGE; // Update dirty_bit as needed
                buffer_add_measurement(&m);

                // Send response
                send_measurement_response(&m, resp_topic);
            } else if (retrieve_measurement_from_edge(ts, &m)) {
                // Store in flash and buffer if desired
                m.dirty_bit = DIRTY_BIT_SENT_TO_EDGE;
                store_measurement_in_flash(&m);
                buffer_add_measurement(&m);

                // Send response
                send_measurement_response(&m, resp_topic);
            } else {
                ESP_LOGE(TAG, "Measurement not found for timestamp %" PRIu32, ts);
                // Optionally, send an error response
            }
        } else {
            ESP_LOGE(TAG, "Invalid or missing 'timestamp' in query");
        }
    } else {
        ESP_LOGE(TAG, "Invalid action in query message");
    }

    cJSON_Delete(json);
}