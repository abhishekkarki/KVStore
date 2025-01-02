// mqtt_utils.c

#include "mqtt_utils.h"
#include "esp_log.h"
#include <inttypes.h>

#include "freertos/semphr.h"
#include "cJSON.h"
#include "nvs_utils.h" 

#include "config.h"
#include "mqtt_topics.h"
// ─────────────────────────────────────────────────────────────────────────────
static const char *TAG = "MQTT_UTILS";
// ─────────────────────────────────────────────────────────────────────────────
// Extern MQTT clients
extern esp_mqtt_client_handle_t edge_mqtt_client;
extern esp_mqtt_client_handle_t device_mqtt_client;
// ─────────────────────────────────────────────────────────────────────────────

SemaphoreHandle_t edge_response_semaphore = NULL;
Measurement edge_received_measurement;
bool edge_response_received = false;
uint32_t expected_request_id = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Publish Measurement to Edge Broker
void publish_to_edge(Measurement *m) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%" PRIu32 ",\"temperature\":%.1f}",
             m->timestamp, m->temperature);

    int msg_id = esp_mqtt_client_publish(edge_mqtt_client, EDGE_PUBLISH_TOPIC, payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published measurement to edge MQTT broker, msg_id=%d", msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish measurement to edge MQTT broker");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Send Measurement Response to Device Broker
void send_measurement_response(Measurement *m, const char *response_topic) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%" PRIu32 ",\"temperature\":%.1f}",
             m->timestamp, m->temperature);

    int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published measurement to device MQTT broker, msg_id=%d", msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish measurement to device MQTT broker");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Function to process the response from the edge device
void process_edge_response(const char *data) {
    ESP_LOGI(TAG, "Processing edge response: %s", data);

    // Parse the JSON data
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse edge response");
        return;
    }

    // Extract request_id
    cJSON *request_id_json = cJSON_GetObjectItem(json, "request_id");
    if (!cJSON_IsNumber(request_id_json)) {
        ESP_LOGE(TAG, "Invalid or missing 'request_id' in edge response");
        cJSON_Delete(json);
        return;
    }
    uint32_t response_request_id = (uint32_t)request_id_json->valuedouble;

    // Check if the response matches the expected request ID
    if (response_request_id != expected_request_id) {
        ESP_LOGW(TAG, "Received response with unexpected request_id: %" PRIu32, response_request_id);
        cJSON_Delete(json);
        return;
    }

    // Extract timestamp and temperature
    cJSON *timestamp_json = cJSON_GetObjectItem(json, "timestamp");
    cJSON *temperature_json = cJSON_GetObjectItem(json, "temperature");

    if (!cJSON_IsNumber(timestamp_json) || !cJSON_IsNumber(temperature_json)) {
        ESP_LOGE(TAG, "Invalid data in edge response");
        cJSON_Delete(json);
        return;
    }

    // Populate the Measurement structure
    edge_received_measurement.timestamp = (uint32_t)timestamp_json->valuedouble;
    edge_received_measurement.temperature = (float)temperature_json->valuedouble;

    cJSON_Delete(json);

    // Signal that the response has been received
    edge_response_received = true;
    xSemaphoreGive(edge_response_semaphore);
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT event handler for the edge MQTT client
static void edge_mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Edge MQTT Connected");
            // Subscribe to the response topic
            esp_mqtt_client_subscribe(edge_mqtt_client, ESP32_RESPONSE_TOPIC, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Edge MQTT Disconnected");
            break;

        case MQTT_EVENT_DATA: {
            char *topic = strndup(event->topic, event->topic_len);
            char *data = strndup(event->data, event->data_len);

            if (strcmp(topic, ESP32_RESPONSE_TOPIC) == 0) {
                process_edge_response(data);
            }

            free(topic);
            free(data);
            break;
        }

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Function to initialize and start the edge MQTT client
void edge_mqtt_start(void) {
    // Edge MQTT Client Configuration
    esp_mqtt_client_config_t edge_mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.0.105:1883",  // Replace with your edge MQTT broker address
        .credentials = {
            .username = "edge_device",
            .authentication = {
                .password = "pass101",
            },
        },
    };

    edge_mqtt_client = esp_mqtt_client_init(&edge_mqtt_cfg);

    // Register the event handler
    esp_mqtt_client_register_event(edge_mqtt_client, ESP_EVENT_ANY_ID, edge_mqtt_event_handler_cb, NULL);

    esp_mqtt_client_start(edge_mqtt_client);
}