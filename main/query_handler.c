// query_handler.c

#include "query_handler.h"
#include "mqtt_utils.h"
#include "buffer.h"
#include "nvs_utils.h"
#include "cJSON.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "QUERY_HANDLER";

// Function to send an error response
void send_error_response(uint32_t timestamp, const char *response_topic) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"error\":\"Measurement not found\",\"timestamp\":%" PRIu32 "}", timestamp);

    int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published error response to device MQTT broker, msg_id=%d", msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish error response to device MQTT broker");
    }
}

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
                ESP_LOGI(TAG, "Measurement found in buffer");
                // Send response
                send_measurement_response(&m, resp_topic);
            } else if (retrieve_measurement_from_flash(ts, &m)) {
                ESP_LOGI(TAG, "Measurement found in flash");
                // Bring it into the buffer
                m.dirty_bit = DIRTY_BIT_SENT_TO_EDGE; // Update dirty_bit as needed
                buffer_add_measurement(&m);

                // Send response
                send_measurement_response(&m, resp_topic);
            } else if (retrieve_measurement_from_edge(ts, &m)) {
                ESP_LOGI(TAG, "Measurement retrived from edge device");
                // Store in flash and buffer if desired
                m.dirty_bit = DIRTY_BIT_SENT_TO_EDGE;
                store_measurement_in_flash(&m);
                buffer_add_measurement(&m);

                // Send response
                send_measurement_response(&m, resp_topic);
            } else {
                ESP_LOGE(TAG, "Measurement not found for timestamp %" PRIu32, ts);
                // Optionally, send an error response
                send_error_response(ts, resp_topic);
            }
        } else {
            ESP_LOGE(TAG, "Invalid or missing 'timestamp' in query");
        }
    } else {
        ESP_LOGE(TAG, "Invalid action in query message");
    }

    cJSON_Delete(json);
}

