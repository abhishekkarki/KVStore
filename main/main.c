#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h> // For PRIu32 and PRIx32
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>    // For isspace()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"          // For UART types and functions
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "dht.h"

#include "nvs_utils.h"
#include "buffer.h"
#include "measurement.h"
#include "config.h"

// Wi-Fi and SNTP includes
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/apps/sntp.h"

// HTTP Server includes
#include "esp_http_server.h"

#define TAG "MAIN"

#define SENSOR_TYPE DHT_TYPE_DHT11
// DHT Sensor GPIO Pin
//#define DHT_GPIO_PIN CONFIG_DHT_GPIO_PIN

// Wi-Fi credentials
//#define WIFI_SSID CONFIG_WIFI_SSID
//#define WIFI_PASS CONFIG_WIFI_PASS

// Function prototypes
void data_collection_task(void *pvParameters);
bool query_measurement(uint32_t timestamp, Measurement *result);
void wifi_init(void);
void initialize_sntp(void);
void obtain_time(void);
void start_webserver(void);

// HTTP server handler
esp_err_t query_handler(httpd_req_t *req) {
    // Get the query string
    char*  buf;
    size_t buf_len;

    // Get URL query string length and allocate memory for length + 1, extra byte for null termination
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            // Get the value of the "timestamp" parameter
            if (httpd_query_key_value(buf, "timestamp", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => timestamp=%s", param);
                uint32_t timestamp = (uint32_t)strtoul(param, NULL, 10);
                Measurement result;

                if (query_measurement(timestamp, &result)) {
                    // Measurement found, send response
                    char resp_str[128];
                    snprintf(resp_str, sizeof(resp_str),
                             "{\"timestamp\": %" PRIu32 ", \"temperature\": %.1f, \"dirty_bit\": %u}",
                             result.timestamp, result.temperature, result.dirty_bit);
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
                } else {
                    // Measurement not found
                    httpd_resp_send_404(req);
                }
            } else {
                // Timestamp parameter not found
                httpd_resp_send_404(req);
            }
        }
        free(buf);
    } else {
        // No query string
        httpd_resp_send_404(req);
    }
    return ESP_OK;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handler
        httpd_uri_t query_uri = {
            .uri       = "/query",
            .method    = HTTP_GET,
            .handler   = query_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &query_uri);
    } else {
        ESP_LOGE(TAG, "Error starting server!");
    }
}

void data_collection_task(void *pvParameters) {
    while (1) {
        float temperature = 0.0;
        float humidity = 0.0;

        if (dht_read_float_data(SENSOR_TYPE, DHT_GPIO_PIN, &humidity, &temperature) == ESP_OK) {
            // Create a new measurement
            Measurement m;
            m.timestamp = (uint32_t)time(NULL); // Get current epoch time
            m.temperature = temperature;
            m.dirty_bit = DIRTY_BIT_BUFFER_ONLY; // Initial state: only in buffer

            // Add to buffer
            buffer_add_measurement(&m);
            ESP_LOGI(TAG, "Measurement added to buffer: Timestamp: %" PRIu32 ", Temperature: %.1f°C", m.timestamp, m.temperature);

            // Check if buffer is 90% full
            if (buffer_is_90_percent_full()) {
                ESP_LOGI(TAG, "Buffer is 90%% full. Pushing half of the entries to flash.");
                buffer_push_to_flash();
            }
        } else {
            ESP_LOGE(TAG, "Could not read data from sensor");
        }

        // Wait for 1 minute (60,000 milliseconds)
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

bool query_measurement(uint32_t timestamp, Measurement *result) {
    // 1. Check buffer
    if (buffer_find_measurement(timestamp, result)) {
        ESP_LOGI(TAG, "Measurement found in buffer");
        return true;
    }

    // 2. Check flash
    if (find_measurement_in_flash(timestamp, result)) {
        ESP_LOGI(TAG, "Measurement found in flash");
        // Bring back to buffer
        buffer_add_measurement(result);
        // No need to change dirty bit; it remains as DIRTY_BIT_IN_FLASH
        return true;
    }

    // 3. Edge device interaction (not implemented)
    ESP_LOGI(TAG, "Measurement not found locally");
    return false;
}

// Wi-Fi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from Wi-Fi, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected to Wi-Fi");
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
}

void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");

    // Set the SNTP operating mode to polling
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Set the SNTP server
    sntp_setservername(0, "pool.ntp.org");

    // Initialize the SNTP service
    sntp_init();
}

void obtain_time(void) {
    // Initialize Wi-Fi
    wifi_init();

    // Initialize SNTP
    initialize_sntp();

    // Wait for time to be set
    int retry = 0;
    const int max_retries = 15;
    time_t now = 0;
    struct tm timeinfo = {0};

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < max_retries) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, max_retries);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE(TAG, "Failed to get time over NTP.");
    } else {
        ESP_LOGI(TAG, "System time set to %s", asctime(&timeinfo));
    }
}

void app_main() {
    // Initialize NVS
    esp_err_t err = init_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return;
    }

    // Initialize buffer
    buffer_init();

    // Obtain current time via SNTP
    obtain_time();

    // Start the web server
    start_webserver();

    // Start data collection task
    xTaskCreate(data_collection_task, "data_collection_task", 4096, NULL, 5, NULL);
}