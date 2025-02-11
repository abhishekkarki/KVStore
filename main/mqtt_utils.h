// mqtt_utils.h

#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H
// ─────────────────────────────────────────────────────────────────────────────
#include "measurement.h"
#include "mqtt_client.h"
#include "config.h"
// ─────────────────────────────────────────────────────────────────────────────
extern esp_mqtt_client_handle_t edge_mqtt_client;    // For edge MQTT broker
extern esp_mqtt_client_handle_t device_mqtt_client;  // For device MQTT broker
// ─────────────────────────────────────────────────────────────────────────────
extern SemaphoreHandle_t edge_response_semaphore;
extern Measurement edge_received_measurement;
extern bool edge_response_received;
extern uint32_t expected_request_id;
// ─────────────────────────────────────────────────────────────────────────────
void publish_to_edge(Measurement *m);
void send_measurement_response(Measurement *m, const char *response_topic);
// ─────────────────────────────────────────────────────────────────────────────
// Function prototypes
void publish_measurement_to_edge(const Measurement *measurement);
void publish_measurement_to_device(const Measurement *measurement);
// ─────────────────────────────────────────────────────────────────────────────
// Timeout for waiting for a response from the edge device (in milliseconds)
#define EDGE_REQUEST_TIMEOUT_MS 10000  // Adjust as needed
// ─────────────────────────────────────────────────────────────────────────────
// Extern declarations for the edge MQTT client
extern esp_mqtt_client_handle_t edge_mqtt_client;
// ─────────────────────────────────────────────────────────────────────────────
// Function to initialize the edge MQTT client
void edge_mqtt_start(void);
// ─────────────────────────────────────────────────────────────────────────────
void process_edge_response(const char *data);
// ─────────────────────────────────────────────────────────────────────────────
#endif // MQTT_UTILS_H