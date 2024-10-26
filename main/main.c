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

static const char *TAG = "MAIN";

// Wi-Fi Configuration
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS
#define MAXIMUM_RETRY 5

// DHT Sensor Configuration
#define DHT_GPIO_PIN CONFIG_DHT_GPIO_PIN 
#define SENSOR_TYPE DHT_TYPE_DHT11

// MQTT Configuration
#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD CONFIG_MQTT_PASSWORD

// Threshold for when to send data from flash to edge device
#define FLASH_USAGE_THRESHOLD_PERCENT 5 // Adjust as needed for testing

esp_mqtt_client_handle_t mqtt_client = NULL;

// Wi-Fi Event Group and Retry Counter
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Function prototypes
void wifi_init_sta(void);
void obtain_time(void);
void mqtt_app_start(void);
void data_collection_task(void *pvParameters);
void flash_monitoring_task(void *pvParameters);
void publish_measurement_to_mqtt(Measurement *m);
void send_flash_data_to_edge(void);
void time_sync_notification_cb(struct timeval *tv);
void buffer_init(void);

// MQTT Publish Function
void publish_measurement_to_mqtt(Measurement *m) {
    char topic[64];
    char payload[128];

    // Define your MQTT topic
    snprintf(topic, sizeof(topic), "esp32/temperature");

    // Prepare the payload
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%" PRIu32 ",\"temperature\":%.1f}",
             m->timestamp, m->temperature);

    // Publish to MQTT
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published measurement to MQTT topic %s, msg_id=%d", topic, msg_id);
}

// MQTT Event Handler
void mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;  // Obtain the client handle
    int msg_id;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            // Subscribe to a topic if needed
            msg_id = esp_mqtt_client_subscribe(client, "esp32/temperature", 0);
            ESP_LOGI(TAG, "Subscribed to esp32/temperature, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
                ESP_LOGE(TAG, "TLS Error: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "TLS Stack Error: 0x%x", event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused, reason code: 0x%x", event->error_handle->connect_return_code);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Transport error: errno=%d", event->error_handle->esp_transport_sock_errno);
            } else {
                ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id: %" PRIi32, event_id);
            break;
    }
}

void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials = {
            .username = MQTT_USERNAME,
            .authentication = {
                .password = MQTT_PASSWORD,
            },
        },
        // If using TLS, include the certificate
        //.cert_pem = (const char *)mqtt_broker_cert_pem_start,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
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

// Time Synchronization Notification Callback
void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronization event");
}

// main.c

void data_collection_task(void *pvParameters) {
    while (1) {
        float temperature = 0.0;
        float humidity = 0.0;

        if (dht_read_float_data(SENSOR_TYPE, DHT_GPIO_PIN, &humidity, &temperature) == ESP_OK) {
            Measurement m;
            m.timestamp = (uint32_t)time(NULL);
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

        // Wait for 10 seconds
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void flash_monitoring_task(void *pvParameters) {
    while (1) {
        uint32_t flash_usage = get_flash_usage_percent();
        //ESP_LOGI(TAG, "Flash usage: %u%%", flash_usage);
        ESP_LOGI(TAG, "Flash usage: %" PRIu32 "%%", flash_usage);

        if (flash_usage >= FLASH_USAGE_THRESHOLD_PERCENT) {
            ESP_LOGI(TAG, "Flash usage threshold reached (%u%%). Sending data to edge device.", FLASH_USAGE_THRESHOLD_PERCENT);
            send_flash_data_to_edge();

            // Clear flash storage after sending
            clear_flash_storage();
        }

        // Check every minute (adjust as needed)
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void send_flash_data_to_edge() {
    ESP_LOGI(TAG, "Sending data from flash to edge device over MQTT");

    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find("nvs", "storage", NVS_TYPE_BLOB, &it);
    while (err == ESP_OK && it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        // Read the measurement
        Measurement m;
        uint32_t timestamp = strtoul(info.key, NULL, 16);
        if (find_measurement_in_flash(timestamp, &m)) {
            // Publish measurement over MQTT
            publish_measurement_to_mqtt(&m);
        } else {
            ESP_LOGE(TAG, "Failed to read measurement from flash for key %s", info.key);
        }

        // Move to the next entry
        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
}

void app_main() {
    // Initialize NVS (Non-Volatile Storage)
    ESP_ERROR_CHECK(init_nvs());

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi
    wifi_init_sta();

    // Wait for Wi-Fi to be connected
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    // Synchronize time
    obtain_time();

    // Initialize buffer
    buffer_init();

    // Initialize MQTT
    mqtt_app_start();

    // Start data collection task
    xTaskCreate(data_collection_task, "data_collection_task", 4096, NULL, 5, NULL);

    // Start flash monitoring task
    xTaskCreate(flash_monitoring_task, "flash_monitoring_task", 4096, NULL, 5, NULL);
}