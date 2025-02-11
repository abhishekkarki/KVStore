// mqtt_topics.h

#ifndef MQTT_TOPICS_H
#define MQTT_TOPICS_H
// ─────────────────────────────────────────────────────────────────────────────
#define EDGE_REQUEST_TOPIC "edge/measurement/request"
#define ESP32_RESPONSE_TOPIC "esp32/measurement/response"
#define EDGE_PUBLISH_TOPIC "esp32/temperature"  // Topic to publish measurements to edge
#define DEVICE_RESPONSE_TOPIC "esp32/response"  // Response topic for device broker
// ─────────────────────────────────────────────────────────────────────────────
#endif // MQTT_TOPICS_H