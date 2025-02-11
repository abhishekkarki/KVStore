#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi credentials
#define CONFIG_WIFI_SSID "XXXXX"
#define CONFIG_WIFI_PASS "XXXXX"

// DHT Sensor GPIO Pin
#define CONFIG_DHT_GPIO_PIN GPIO_NUM_22 // Replace with your GPIO number

// EDGE part
#define CONFIG_EDGE_MQTT_BROKER_URI "mqtt://192.X.X.X:1883" 
#define CONFIG_EDGE_MQTT_USERNAME "edge_device"
#define CONFIG_EDGE_MQTT_PASSWORD "pass101"

// Device part
#define CONFIG_DEVICE_MQTT_BROKER_URI "mqtt://192.X.X.X:1883"
#define CONFIG_DEVICE_MQTT_USERNAME ""
#define CONFIG_DEVICE_MQTT_PASSWORD ""

// Define MQTT topics for communication with the edge device
//#define EDGE_REQUEST_TOPIC "edge/request"
//#define ESP32_RESPONSE_TOPIC "esp32/response"

#endif // CONFIG_H
