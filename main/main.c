// main.c

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "config.h"

#include "mqtt_client.h"
#include "dht.h"

#include "buffer.h"
#include "nvs_utils.h"
#include "measurement.h"
#include "mqtt_utils.h"
#include "timestamp_list.h"

// New parts
#include "query_handler.h"
#include "cJSON.h"

// ─────────────────────────────────────────────────────────────────────────────
static const char *TAG = "MAIN";

// Wi-Fi Configuration
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS

// ─────────────────────────────────────────────────────────────────────────────
// DHT Sensor Configuration
#define DHT_GPIO_PIN CONFIG_DHT_GPIO_PIN
#define SENSOR_TYPE DHT_TYPE_DHT11

// ─────────────────────────────────────────────────────────────────────────────
// Edge MQTT Configuration (Requires Authentication)
#define EDGE_MQTT_BROKER_URI CONFIG_EDGE_MQTT_BROKER_URI
#define EDGE_MQTT_USERNAME CONFIG_EDGE_MQTT_USERNAME
#define EDGE_MQTT_PASSWORD CONFIG_EDGE_MQTT_PASSWORD

// ─────────────────────────────────────────────────────────────────────────────
// Device MQTT Configuration (No Authentication)
#define DEVICE_MQTT_BROKER_URI CONFIG_DEVICE_MQTT_BROKER_URI

// Threshold for when to send data from flash to edge device
#define FLASH_USAGE_THRESHOLD_PERCENT 18 // Adjust as needed (this is in terms of percentage)

// Measurement Interval
#define MEASUREMENT_INTERVAL_MS 20000  // Collect measurement every x seconds (i.e. 5000 = 5 sec)

// ─────────────────────────────────────────────────────────────────────────────
// Global MQTT client handles
esp_mqtt_client_handle_t edge_mqtt_client = NULL;    // For edge MQTT broker
esp_mqtt_client_handle_t device_mqtt_client = NULL;  // For device MQTT broker

// ─────────────────────────────────────────────────────────────────────────────
// Function prototypes
void wifi_init_sta(void);
void obtain_time(void);
void mqtt_app_start(void);
void measurement_collection_task(void *pvParameters);
void flash_monitoring_task(void *pvParameters);
void send_flash_data_to_edge(void);
void time_sync_notification_cb(struct timeval *tv);
void buffer_init(void);

// ─────────────────────────────────────────────────────────────────────────────
// Wi-Fi Event Group
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// ─────────────────────────────────────────────────────────────────────────────
// MQTT Event Handlers
void edge_mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data);
void device_mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data);

// ─────────────────────────────────────────────────────────────────────────────
// Measurement Collection Task
void measurement_collection_task(void *pvParameters) {
    while (1) {
        Measurement m;
        m.timestamp = (uint32_t)time(NULL);

        float humidity = 0.0f;
        float temperature = 0.0f;

        if (dht_read_float_data(SENSOR_TYPE, DHT_GPIO_PIN, &humidity, &temperature) == ESP_OK) {
            m.temperature = temperature;
            m.dirty_bit = DIRTY_BIT_BUFFER_ONLY;

            // Add to buffer
            buffer_add_measurement(&m);

            ESP_LOGI(TAG, "Measurement collected: Timestamp: %" PRIu32 ", Temperature: %.1f°C", m.timestamp, m.temperature);

            // Check if buffer is at threshold
            if (buffer_is_threshold_full()) {
                ESP_LOGI(TAG, "Buffer is %d%% full. Pushing half of the entries to flash.", BUFFER_THRESHOLD_PERCENT);
                buffer_push_to_flash();
            }
        } else {
            ESP_LOGE(TAG, "Could not read data from sensor");
        }

        // Wait for the next measurement
        vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Flash Monitoring Task
void flash_monitoring_task(void *pvParameters) {
    while (1) {
        uint32_t flash_usage = get_flash_usage_percent();
        ESP_LOGI(TAG, "Flash usage: %" PRIu32 "%%", flash_usage);

        if (flash_usage >= FLASH_USAGE_THRESHOLD_PERCENT) {
            ESP_LOGI(TAG, "Flash usage threshold reached (%u%%). Sending data to edge device.", FLASH_USAGE_THRESHOLD_PERCENT);
            send_flash_data_to_edge();
        }

        // Check every minute (adjust as needed)
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// send flash data to edge
void send_flash_data_to_edge(void) {
    ESP_LOGI(TAG, "Sending data from flash to edge device over MQTT");

    size_t entries_to_send = 10; // Adjust as needed
    uint32_t timestamps[entries_to_send];
    size_t actual_entries = 0;

    // Get the first 'entries_to_send' timestamps from the list
    get_timestamps_from_list(entries_to_send, timestamps, &actual_entries);

    if (actual_entries == 0) {
        ESP_LOGI(TAG, "No entries to send");
        return;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    for (size_t i = 0; i < actual_entries; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%" PRIu32, timestamps[i]);

        // Retrieve the measurement
        size_t required_size = sizeof(Measurement);
        Measurement m;
        err = nvs_get_blob(handle, key, &m, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get measurement for key %s: %s", key, esp_err_to_name(err));
            continue;
        }

        // Send the measurement to the edge device
        publish_to_edge(&m);

        // Update dirty_bit to DIRTY_BIT_SENT_TO_EDGE
        m.dirty_bit = DIRTY_BIT_SENT_TO_EDGE;
        err = nvs_set_blob(handle, key, &m, sizeof(Measurement));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update measurement for key %s: %s", key, esp_err_to_name(err));
            continue;
        }

        // Delete the entry from flash
        err = nvs_erase_key(handle, key);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase key %s: %s", key, esp_err_to_name(err));
        }
    }

    nvs_commit(handle);
    nvs_close(handle);

    // Remove the timestamps from the list
    remove_timestamps_from_list(actual_entries);
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT Event Handlers
void edge_mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    // Handle events related to the edge MQTT client
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Edge MQTT Connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Edge MQTT Disconnected");
            break;
        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void device_mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    // Handle events related to the device MQTT client
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Device MQTT Connected");
            esp_mqtt_client_subscribe(device_mqtt_client, "esp32/query", 0);
            ESP_LOGI(TAG, "Subscribed to esp32/query");
            break;
        case MQTT_EVENT_DATA: {
            char *topic_str = strndup(event->topic, event->topic_len);
            char *data_str = strndup(event->data, event->data_len);

            ESP_LOGI(TAG, "Received message on topic: %s", topic_str);

            if (strcmp(topic_str, "esp32/query") == 0) {
                // Process the query message
                process_query_message(data_str);
            }

            free(topic_str);
            free(data_str);
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Device MQTT Disconnected");
            break;
        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT Initialization
void mqtt_app_start(void) {
    /*
    // Edge MQTT Client Configuration (Requires Authentication)
    esp_mqtt_client_config_t edge_mqtt_cfg = {
        .broker.address.uri = EDGE_MQTT_BROKER_URI,
        .credentials = {
            .username = EDGE_MQTT_USERNAME,
            .authentication = {
                .password = EDGE_MQTT_PASSWORD,
            },
        },
    };

    edge_mqtt_client = esp_mqtt_client_init(&edge_mqtt_cfg);
    esp_mqtt_client_register_event(edge_mqtt_client, ESP_EVENT_ANY_ID, edge_mqtt_event_handler, NULL);
    esp_mqtt_client_start(edge_mqtt_client);
    */

    // Device MQTT Client Configuration (No Authentication)
    esp_mqtt_client_config_t device_mqtt_cfg = {
        .broker.address.uri = DEVICE_MQTT_BROKER_URI,
        .credentials = {
            .username = NULL,
            .authentication = {
                .password = NULL,
            },
        },
    };

    device_mqtt_client = esp_mqtt_client_init(&device_mqtt_cfg);
    esp_mqtt_client_register_event(device_mqtt_client, ESP_EVENT_ANY_ID, device_mqtt_event_handler, NULL);
    esp_mqtt_client_start(device_mqtt_client);
}

// ─────────────────────────────────────────────────────────────────────────────
// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected to SSID:%s", WIFI_SSID);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Wi-Fi Initialization
void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    // Create default Wi-Fi station
    esp_netif_create_default_wifi_sta();

    // Initialize Wi-Fi with default configurations
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Set Wi-Fi mode and configuration
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            // Prevents connecting to AP with weaker security
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Station mode
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SNTP Time Synchronization
void obtain_time(void) {
    ESP_LOGI(TAG, "Initializing SNTP");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry >= retry_count) {
        ESP_LOGE(TAG, "Failed to obtain time");
    } else {
        ESP_LOGI(TAG, "System time is set");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Time Synchronization Notification Callback
void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronization event");
}

// ─────────────────────────────────────────────────────────────────────────────
void app_main(void) {
    // Initialize NVS
    esp_err_t err = init_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return;
    }

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize buffer
    buffer_init();

    // Initialize Wi-Fi
    wifi_init_sta();

    // Wait for Wi-Fi to be connected
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    // Synchronize time
    obtain_time();

    // Initialize MQTT clients
    mqtt_app_start();       // Start the device MQTT client
    edge_mqtt_start();     // Start the edge MQTT client


    // Start measurement collection task
    xTaskCreate(measurement_collection_task, "measurement_collection_task", 4096, NULL, 5, NULL);

    // Start flash monitoring task
    xTaskCreate(flash_monitoring_task, "flash_monitoring_task", 4096, NULL, 5, NULL);
}
// ─────────────────────────────────────────────────────────────────────────────