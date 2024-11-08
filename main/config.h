#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi credentials
#define CONFIG_WIFI_SSID "TP-Link_BA88"
#define CONFIG_WIFI_PASS "91037695"

// DHT Sensor GPIO Pin
#define CONFIG_DHT_GPIO_PIN GPIO_NUM_22 // Replace with your GPIO number

// MQTT Data
//#define CONFIG_MQTT_BROKER_URI "mqtt://192.168.0.104:1883"
//#define CONFIG_MQTT_USERNAME "edge_device"
//#define CONFIG_MQTT_PASSWORD "pass101"


// EDGE part
#define CONFIG_EDGE_MQTT_BROKER_URI "mqtt://192.168.0.105:1883"
#define CONFIG_EDGE_MQTT_USERNAME "edge_device"
#define CONFIG_EDGE_MQTT_PASSWORD "pass101"

// Device part
#define CONFIG_DEVICE_MQTT_BROKER_URI "mqtt://192.168.0.103:1883"
#define CONFIG_DEVICE_MQTT_USERNAME ""
#define CONFIG_DEVICE_MQTT_PASSWORD ""


#endif // CONFIG_H