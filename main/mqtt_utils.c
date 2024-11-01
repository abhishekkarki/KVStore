// mqtt_utils.c

#include "mqtt_utils.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "MQTT_UTILS";

// Extern MQTT clients
extern esp_mqtt_client_handle_t edge_mqtt_client;
extern esp_mqtt_client_handle_t device_mqtt_client;

// Publish Measurement to Edge Broker
void publish_to_edge(Measurement *m) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%" PRIu32 ",\"temperature\":%.1f}",
             m->timestamp, m->temperature);

    int msg_id = esp_mqtt_client_publish(edge_mqtt_client, "esp32/temperature", payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published measurement to edge MQTT broker, msg_id=%d", msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish measurement to edge MQTT broker");
    }
}

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