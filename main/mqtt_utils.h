// mqtt_utils.h

#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H

#include "measurement.h"
#include "mqtt_client.h"

extern esp_mqtt_client_handle_t edge_mqtt_client;    // For edge MQTT broker
extern esp_mqtt_client_handle_t device_mqtt_client;  // For device MQTT broker

void publish_to_edge(Measurement *m);
void send_measurement_response(Measurement *m, const char *response_topic);

#endif // MQTT_UTILS_H