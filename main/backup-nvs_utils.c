// #include "nvs_utils.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "nvs.h"
// #include <inttypes.h>

// #include "mqtt_utils.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"
// #include "cJSON.h"
// #include "esp_timer.h"
// #include "esp_random.h"
// #include "esp_system.h"  // For esp_random()
// #include "mqtt_topics.h"

// #include "timestamp_list.h"


// static const char *TAG = "NVS_UTILS";

// esp_err_t init_nvs(void) {
//     esp_err_t err = nvs_flash_init();
//     if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
//         err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
//         err == ESP_ERR_NVS_NOT_INITIALIZED) {
//         ESP_LOGW(TAG, "NVS partition was truncated or not initialized. Erasing and re-initializing...");
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         err = nvs_flash_init();
//     }
//     return err;
// }

// bool store_measurement_in_flash(Measurement *m) {
//     if (m == NULL) {
//         ESP_LOGE(TAG, "Measurement is NULL");
//         return false;
//     }

//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return false;
//     }

//     char key[16];
//     snprintf(key, sizeof(key), "%" PRIu32, m->timestamp);

//     // Check if entry already exists
//     size_t required_size = 0;
//     err = nvs_get_blob(handle, key, NULL, &required_size);
//     if (err == ESP_OK) {
//         // Entry already exists, no need to overwrite
//         nvs_close(handle);
//         return true;
//     }

//     // Store the measurement
//     err = nvs_set_blob(handle, key, m, sizeof(Measurement));
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to set blob in NVS: %s", esp_err_to_name(err));
//         nvs_close(handle);
//         return false;
//     }

//     err = nvs_commit(handle);
//     nvs_close(handle);

//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
//         return false;
//     }

//     //ESP_LOGI(TAG, "Stored measurement in flash with key %s", key);
//     ESP_LOGI(TAG, "Stored measurement timestamp = %" PRIu32 " into flash with key %s", m->timestamp, key);
//     return true;
// }

// bool find_measurement_in_flash(uint32_t timestamp, Measurement *result) {
//     if (result == NULL) {
//         ESP_LOGE(TAG, "Result pointer is NULL");
//         return false;
//     }

//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return false;
//     }

//     char key[11];
//     snprintf(key, sizeof(key), "%08" PRIX32, timestamp);  // Use PRIX32

//     size_t required_size = sizeof(Measurement);
//     err = nvs_get_blob(handle, key, result, &required_size);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get blob from NVS: %s", esp_err_to_name(err));
//         nvs_close(handle);
//         return false;
//     }

//     nvs_close(handle);
//     return true;
// }

// void clear_flash_storage(void) {
//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return;
//     }

//     err = nvs_erase_all(handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to erase NVS storage: %s", esp_err_to_name(err));
//     } else {
//         ESP_LOGI(TAG, "Flash storage cleared.");
//     }

//     err = nvs_commit(handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
//     }

//     nvs_close(handle);
// }

// uint32_t get_flash_usage_percent() {
//     nvs_stats_t nvs_stats;
//     esp_err_t err = nvs_get_stats("nvs", &nvs_stats);  // Specify the partition name
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(err));
//         return 0;
//     }

//     size_t used_entries = nvs_stats.used_entries;
//     size_t total_entries = nvs_stats.total_entries;

//     if (total_entries == 0) {
//         return 0;
//     }

//     uint32_t usage_percent = (used_entries * 100) / total_entries;
//     return usage_percent;
// }

// bool retrieve_measurement_from_flash(uint32_t timestamp, Measurement *m) {
//     if (m == NULL) {
//         ESP_LOGE(TAG, "Measurement pointer is NULL");
//         return false;
//     }

//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return false;
//     }

//     char key[16];
//     snprintf(key, sizeof(key), "%" PRIu32, timestamp);

//     size_t required_size = sizeof(Measurement);
//     err = nvs_get_blob(handle, key, m, &required_size);
//     nvs_close(handle);

//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get measurement from flash for key %s: %s", key, esp_err_to_name(err));
//         return false;
//     }
//     ESP_LOGI(TAG, "Retrieved measurement timestamp = %" PRIu32 " from flash", timestamp);
//     return true;
// }

// // Function to initialize the semaphore
// static void init_edge_response_semaphore() {
//     if (edge_response_semaphore == NULL) {
//         edge_response_semaphore = xSemaphoreCreateBinary();
//         if (edge_response_semaphore == NULL) {
//             ESP_LOGE(TAG, "Failed to create semaphore");
//         }
//     }
// }

// bool retrieve_measurement_from_edge(uint32_t timestamp, Measurement *m) {
//     if (edge_mqtt_client == NULL) {
//         ESP_LOGE(TAG, "Edge MQTT client is not initialized");
//         return false;
//     }

//     init_edge_response_semaphore();
//     if (edge_response_semaphore == NULL) {
//         return false;
//     }

//     // Reset the response flag
//     edge_response_received = false;

//     // Generate a unique request ID
//     expected_request_id = esp_random();

//     // Construct the request JSON
//     cJSON *request_json = cJSON_CreateObject();
//     cJSON_AddStringToObject(request_json, "action", "get_measurement");
//     cJSON_AddNumberToObject(request_json, "timestamp", timestamp);
//     cJSON_AddNumberToObject(request_json, "request_id", expected_request_id);

//     char *request_payload = cJSON_PrintUnformatted(request_json);
//     cJSON_Delete(request_json);

//     if (request_payload == NULL) {
//         ESP_LOGE(TAG, "Failed to create request payload");
//         return false;
//     }

//     ESP_LOGI(TAG, "Requesting measurement timestamp=%" PRIu32 " from edge", timestamp);
//     // Publish the request to the edge device
//     int msg_id = esp_mqtt_client_publish(edge_mqtt_client, EDGE_REQUEST_TOPIC,
//                                          request_payload, 0, 1, 0);

//     free(request_payload);

//     if (msg_id == -1) {
//         ESP_LOGE(TAG, "Failed to publish request to edge device");
//         return false;
//     }

//     ESP_LOGI(TAG, "Published measurement request to edge device, msg_id=%d", msg_id);

//     // Wait for the response with a timeout
//     if (xSemaphoreTake(edge_response_semaphore, pdMS_TO_TICKS(EDGE_REQUEST_TIMEOUT_MS)) == pdTRUE) {
//         if (edge_response_received) {
//             // Copy the received measurement to the output parameter
//             *m = edge_received_measurement;
//             return true;
//         } else {
//             ESP_LOGE(TAG, "Edge response received but flag not set");
//             return false;
//         }
//     } else {
//         ESP_LOGE(TAG, "Timeout waiting for response from edge device");
//         return false;
//     }

//     // Ensure that the function returns a value in all code paths
//     return false;
// }

// // Retrieves measurements within the specified timestamp range from flash
// int get_measurements_from_flash(uint32_t start_timestamp, uint32_t end_timestamp, Measurement *measurements, size_t max_measurements) {
//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return -1;
//     }

//     // Retrieve the timestamp list
//     size_t list_size = 0;
//     err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, NULL, &list_size);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get timestamp list size: %s", esp_err_to_name(err));
//         nvs_close(handle);
//         return -1;
//     }

//     size_t total_entries = list_size / sizeof(uint32_t);
//     uint32_t *timestamp_list = malloc(list_size);
//     if (timestamp_list == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for timestamp list");
//         nvs_close(handle);
//         return -1;
//     }

//     err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, &list_size);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get timestamp list: %s", esp_err_to_name(err));
//         free(timestamp_list);
//         nvs_close(handle);
//         return -1;
//     }

//     int count = 0;
//     for (size_t i = 0; i < total_entries && count < max_measurements; i++) {
//         uint32_t ts = timestamp_list[i];
//         if (ts >= start_timestamp && ts <= end_timestamp) {
//             Measurement m;
//             if (retrieve_measurement_from_flash(ts, &m)) {
//                 measurements[count++] = m;
//             } else {
//                 ESP_LOGE(TAG, "Failed to retrieve measurement for timestamp %" PRIu32, ts);
//             }
//         }
//     }

//     free(timestamp_list);
//     nvs_close(handle);
//     return count;
// }


// this is my current query_handler.c file 


// // query_handler.c

// #include "query_handler.h"
// #include "mqtt_utils.h"
// #include "buffer.h"
// #include "nvs_utils.h"
// #include "cJSON.h"
// #include "esp_log.h"
// #include <inttypes.h>

// static const char *TAG = "QUERY_HANDLER";

// // Function to send an error response
// void send_error_response(uint32_t timestamp, const char *response_topic) {
//     char payload[128];
//     snprintf(payload, sizeof(payload),
//              "{\"error\":\"Measurement not found\",\"timestamp\":%" PRIu32 "}", timestamp);

//     int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
//     if (msg_id != -1) {
//         ESP_LOGI(TAG, "Published error response to device MQTT broker, msg_id=%d", msg_id);
//     } else {
//         ESP_LOGE(TAG, "Failed to publish error response to device MQTT broker");
//     }
// }

// void process_query_message(const char *message) {
//     ESP_LOGI(TAG, "Processing query message: %s", message);
//     cJSON *json = cJSON_Parse(message);
//     if (json == NULL) {
//         ESP_LOGE(TAG, "Failed to parse query message");
//         return;
//     }

//     const cJSON *action = cJSON_GetObjectItem(json, "action");
//     if (strcmp(action->valuestring, "get_data_range") == 0) {
//         const cJSON *start_ts = cJSON_GetObjectItem(json, "start_timestamp");
//         const cJSON *end_ts = cJSON_GetObjectItem(json, "end_timestamp");
//         const cJSON *response_topic = cJSON_GetObjectItem(json, "response_topic");

//         const char *resp_topic = "esp32/response"; // Default response topic
//         if (cJSON_IsString(response_topic)) {
//             resp_topic = response_topic->valuestring;
//         }

//         if (cJSON_IsNumber(start_ts) && cJSON_IsNumber(end_ts)) {
//             uint32_t start_timestamp = (uint32_t)start_ts->valuedouble;
//             uint32_t end_timestamp = (uint32_t)end_ts->valuedouble;

//             size_t max_measurements = BUFFER_CAPACITY_MACRO; // Adjust as needed
//             Measurement *measurements = malloc(sizeof(Measurement) * max_measurements);
//             if (measurements == NULL) {
//                 ESP_LOGE(TAG, "Failed to allocate memory for measurements");
//                 cJSON_Delete(json);
//                 return;
//             }

//             int count = get_measurements_from_buffer(start_timestamp, end_timestamp, measurements, max_measurements);
//             if (count > 0) {
//                 ESP_LOGI(TAG, "Measurements found in buffer");
//                 ESP_LOGI(TAG, "Retrieved %d measurements from buffer", count);
//                 // Send measurements
//                 send_measurements_response(measurements, count, resp_topic);
//                 free(measurements);
//             } else {
//                 ESP_LOGI(TAG, "Measurements not found in buffer, getting them from flash");
//                 // Try to get data from flash
//                 count = get_measurements_from_flash(start_timestamp, end_timestamp, measurements, max_measurements);
//                 if (count > 0) {
//                     ESP_LOGI(TAG, "Retrieved %d measurements from flash", count);

//                     // Decide whether to bring data into the buffer
//                     if (count <= (BUFFER_CAPACITY_MACRO * 0.8)) {
//                         // Bring data into the buffer
//                         for (int i = 0; i < count; i++) {
//                             buffer_add_measurement(&measurements[i]);
//                         }
//                         // Send measurements
//                         send_measurements_response(measurements, count, resp_topic);
//                     } else {
//                         // Data too large to bring into buffer, read directly from flash
//                         ESP_LOGI(TAG, "Data too large to bring into buffer. Sending directly from flash.");
//                         send_measurements_response(measurements, count, resp_topic);
//                     }
//                     free(measurements);
//                 } else {
//                     // Data not available locally, retrieve from edge
//                     ESP_LOGI(TAG, "Data not available locally. Retrieving from edge device.");
//                     // Implement retrieval from edge device here
//                     // For now, send an error response
//                     send_error_response_range(start_timestamp, end_timestamp, resp_topic);
//                     free(measurements);
//                 }
//             }
//         } else {
//             ESP_LOGE(TAG, "Invalid or missing 'start_timestamp' or 'end_timestamp' in query");
//         }
//     } else {
//         ESP_LOGE(TAG, "Invalid action in query message");
//     }

//     cJSON_Delete(json);
// }

// void send_measurements_response(Measurement *measurements, int count, const char *response_topic) {
//     cJSON *json_array = cJSON_CreateArray();
//     for (int i = 0; i < count; i++) {
//         cJSON *item = cJSON_CreateObject();
//         cJSON_AddNumberToObject(item, "timestamp", measurements[i].timestamp);
//         cJSON_AddNumberToObject(item, "temperature", measurements[i].temperature);
//         cJSON_AddItemToArray(json_array, item);
//     }

//     char *payload = cJSON_PrintUnformatted(json_array);
//     cJSON_Delete(json_array);

//     if (payload == NULL) {
//         ESP_LOGE(TAG, "Failed to create JSON payload");
//         return;
//     }

//     int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
//     if (msg_id != -1) {
//         ESP_LOGI(TAG, "Published measurements response to device MQTT broker, msg_id=%d", msg_id);
//     } else {
//         ESP_LOGE(TAG, "Failed to publish measurements response to device MQTT broker");
//     }

//     free(payload);
// }

// void send_error_response_range(uint32_t start_timestamp, uint32_t end_timestamp, const char *response_topic) {
//     char payload[256];
//     snprintf(payload, sizeof(payload),
//              "{\"error\":\"Measurements not found in the range\",\"start_timestamp\":%" PRIu32 ",\"end_timestamp\":%" PRIu32 "}",
//              start_timestamp, end_timestamp);

//     int msg_id = esp_mqtt_client_publish(device_mqtt_client, response_topic, payload, 0, 1, 0);
//     if (msg_id != -1) {
//         ESP_LOGI(TAG, "Published error response to device MQTT broker, msg_id=%d", msg_id);
//     } else {
//         ESP_LOGE(TAG, "Failed to publish error response to device MQTT broker");
//     }
// }




// and this is the nvs_utils.c file 

// #include "nvs_utils.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "nvs.h"
// #include <inttypes.h>

// #include "mqtt_utils.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"
// #include "cJSON.h"
// #include "esp_timer.h"
// #include "esp_random.h"
// #include "esp_system.h"  // For esp_random()
// #include "mqtt_topics.h"

// #include "timestamp_list.h"


// static const char *TAG = "NVS_UTILS";

// esp_err_t init_nvs(void) {
//     esp_err_t err = nvs_flash_init();
//     if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
//         err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
//         err == ESP_ERR_NVS_NOT_INITIALIZED) {
//         ESP_LOGW(TAG, "NVS partition was truncated or not initialized. Erasing and re-initializing...");
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         err = nvs_flash_init();
//     }
//     return err;
// }

// bool store_measurement_in_flash(Measurement *m) {
//     if (m == NULL) {
//         ESP_LOGE(TAG, "Measurement is NULL");
//         return false;
//     }

//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return false;
//     }

//     char key[16];
//     snprintf(key, sizeof(key), "%" PRIu32, m->timestamp);

//     // Check if entry already exists
//     size_t required_size = 0;
//     err = nvs_get_blob(handle, key, NULL, &required_size);
//     if (err == ESP_OK) {
//         // Entry already exists, no need to overwrite
//         nvs_close(handle);
//         return true;
//     }

//     // Store the measurement
//     err = nvs_set_blob(handle, key, m, sizeof(Measurement));
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to set blob in NVS: %s", esp_err_to_name(err));
//         nvs_close(handle);
//         return false;
//     }

//     err = nvs_commit(handle);
//     nvs_close(handle);

//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
//         return false;
//     }

//     //ESP_LOGI(TAG, "Stored measurement in flash with key %s", key);
//     ESP_LOGI(TAG, "Stored measurement timestamp = %" PRIu32 " into flash with key %s", m->timestamp, key);
//     return true;
// }

// bool find_measurement_in_flash(uint32_t timestamp, Measurement *result) {
//     if (result == NULL) {
//         ESP_LOGE(TAG, "Result pointer is NULL");
//         return false;
//     }

//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return false;
//     }

//     char key[11];
//     snprintf(key, sizeof(key), "%08" PRIX32, timestamp);  // Use PRIX32

//     size_t required_size = sizeof(Measurement);
//     err = nvs_get_blob(handle, key, result, &required_size);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get blob from NVS: %s", esp_err_to_name(err));
//         nvs_close(handle);
//         return false;
//     }

//     nvs_close(handle);
//     return true;
// }

// void clear_flash_storage(void) {
//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return;
//     }

//     err = nvs_erase_all(handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to erase NVS storage: %s", esp_err_to_name(err));
//     } else {
//         ESP_LOGI(TAG, "Flash storage cleared.");
//     }

//     err = nvs_commit(handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
//     }

//     nvs_close(handle);
// }

// uint32_t get_flash_usage_percent() {
//     nvs_stats_t nvs_stats;
//     esp_err_t err = nvs_get_stats("nvs", &nvs_stats);  // Specify the partition name
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(err));
//         return 0;
//     }

//     size_t used_entries = nvs_stats.used_entries;
//     size_t total_entries = nvs_stats.total_entries;

//     if (total_entries == 0) {
//         return 0;
//     }

//     uint32_t usage_percent = (used_entries * 100) / total_entries;
//     return usage_percent;
// }

// bool retrieve_measurement_from_flash(uint32_t timestamp, Measurement *m) {
//     if (m == NULL) {
//         ESP_LOGE(TAG, "Measurement pointer is NULL");
//         return false;
//     }

//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return false;
//     }

//     char key[16];
//     snprintf(key, sizeof(key), "%" PRIu32, timestamp);

//     size_t required_size = sizeof(Measurement);
//     err = nvs_get_blob(handle, key, m, &required_size);
//     nvs_close(handle);

//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get measurement from flash for key %s: %s", key, esp_err_to_name(err));
//         return false;
//     }
//     ESP_LOGI(TAG, "Retrieved measurement timestamp = %" PRIu32 " from flash", timestamp);
//     return true;
// }

// // Function to initialize the semaphore
// static void init_edge_response_semaphore() {
//     if (edge_response_semaphore == NULL) {
//         edge_response_semaphore = xSemaphoreCreateBinary();
//         if (edge_response_semaphore == NULL) {
//             ESP_LOGE(TAG, "Failed to create semaphore");
//         }
//     }
// }

// bool retrieve_measurement_from_edge(uint32_t timestamp, Measurement *m) {
//     if (edge_mqtt_client == NULL) {
//         ESP_LOGE(TAG, "Edge MQTT client is not initialized");
//         return false;
//     }

//     init_edge_response_semaphore();
//     if (edge_response_semaphore == NULL) {
//         return false;
//     }

//     // Reset the response flag
//     edge_response_received = false;

//     // Generate a unique request ID
//     expected_request_id = esp_random();

//     // Construct the request JSON
//     cJSON *request_json = cJSON_CreateObject();
//     cJSON_AddStringToObject(request_json, "action", "get_measurement");
//     cJSON_AddNumberToObject(request_json, "timestamp", timestamp);
//     cJSON_AddNumberToObject(request_json, "request_id", expected_request_id);

//     char *request_payload = cJSON_PrintUnformatted(request_json);
//     cJSON_Delete(request_json);

//     if (request_payload == NULL) {
//         ESP_LOGE(TAG, "Failed to create request payload");
//         return false;
//     }

//     ESP_LOGI(TAG, "Requesting measurement timestamp=%" PRIu32 " from edge", timestamp);
//     // Publish the request to the edge device
//     int msg_id = esp_mqtt_client_publish(edge_mqtt_client, EDGE_REQUEST_TOPIC,
//                                          request_payload, 0, 1, 0);

//     free(request_payload);

//     if (msg_id == -1) {
//         ESP_LOGE(TAG, "Failed to publish request to edge device");
//         return false;
//     }

//     ESP_LOGI(TAG, "Published measurement request to edge device, msg_id=%d", msg_id);

//     // Wait for the response with a timeout
//     if (xSemaphoreTake(edge_response_semaphore, pdMS_TO_TICKS(EDGE_REQUEST_TIMEOUT_MS)) == pdTRUE) {
//         if (edge_response_received) {
//             // Copy the received measurement to the output parameter
//             *m = edge_received_measurement;
//             return true;
//         } else {
//             ESP_LOGE(TAG, "Edge response received but flag not set");
//             return false;
//         }
//     } else {
//         ESP_LOGE(TAG, "Timeout waiting for response from edge device");
//         return false;
//     }

//     // Ensure that the function returns a value in all code paths
//     return false;
// }

// // Retrieves measurements within the specified timestamp range from flash
// int get_measurements_from_flash(uint32_t start_timestamp, uint32_t end_timestamp, Measurement *measurements, size_t max_measurements) {
//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
//         return -1;
//     }

//     // Retrieve the timestamp list
//     size_t list_size = 0;
//     err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, NULL, &list_size);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get timestamp list size: %s", esp_err_to_name(err));
//         nvs_close(handle);
//         return -1;
//     }

//     size_t total_entries = list_size / sizeof(uint32_t);
//     uint32_t *timestamp_list = malloc(list_size);
//     if (timestamp_list == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for timestamp list");
//         nvs_close(handle);
//         return -1;
//     }

//     err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, &list_size);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get timestamp list: %s", esp_err_to_name(err));
//         free(timestamp_list);
//         nvs_close(handle);
//         return -1;
//     }

//     int count = 0;
//     for (size_t i = 0; i < total_entries && count < max_measurements; i++) {
//         uint32_t ts = timestamp_list[i];
//         if (ts >= start_timestamp && ts <= end_timestamp) {
//             Measurement m;
//             if (retrieve_measurement_from_flash(ts, &m)) {
//                 measurements[count++] = m;
//             } else {
//                 ESP_LOGE(TAG, "Failed to retrieve measurement for timestamp %" PRIu32, ts);
//             }
//         }
//     }

//     free(timestamp_list);
//     nvs_close(handle);
//     return count;
// } 




// and this is the buffer.c file  

// #include "buffer.h"
// #include "nvs_utils.h"
// #include "esp_log.h"
// #include <string.h>
// #include "timestamp_list.h"

// static const char *TAG = "BUFFER";

// const int BUFFER_CAPACITY = BUFFER_CAPACITY_MACRO; // Match the macro value

// Measurement buffer[BUFFER_CAPACITY_MACRO];

// int buffer_head = 0;
// int buffer_tail = 0;
// int buffer_count = 0;
// SemaphoreHandle_t buffer_mutex;

// void buffer_init() {
//     buffer_head = 0;
//     buffer_tail = 0;
//     buffer_count = 0;
//     buffer_mutex = xSemaphoreCreateMutex();
// }

// void buffer_add_measurement(Measurement *m) {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return;
//     }

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);

//     // Check if buffer is full
//     if (buffer_count >= BUFFER_CAPACITY_MACRO) {
//         // Buffer is full, need to make space
//         // Evict the oldest entry (buffer_tail)
//         Measurement *oldest = &buffer[buffer_tail];
//         if (oldest->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
//             // Push to flash before eviction
//             if (store_measurement_in_flash(oldest)) {
//                 append_timestamp_to_list(oldest->timestamp);
//                 oldest->dirty_bit = DIRTY_BIT_IN_FLASH;
//             } else {
//                 ESP_LOGE(TAG, "Failed to store measurement in flash during eviction");
//                 // Handle error if necessary
//             }
//         }
//         // Move buffer_tail forward
//         buffer_tail = (buffer_tail + 1) % BUFFER_CAPACITY_MACRO;
//         buffer_count--;
//     }

//     // Add new measurement
//     buffer[buffer_head] = *m;
//     buffer_head = (buffer_head + 1) % BUFFER_CAPACITY_MACRO;
//     buffer_count++;

//     xSemaphoreGive(buffer_mutex);
//     ESP_LOGI(TAG, "Added measurement timestamp = %" PRIu32 ", dirty_bit = %u to buffer", m->timestamp, m->dirty_bit);
// }

// bool buffer_is_threshold_full() {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return false;
//     }

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     bool result = (buffer_count >= (BUFFER_CAPACITY_MACRO * BUFFER_THRESHOLD_PERCENT / 100));
//     xSemaphoreGive(buffer_mutex);
//     // Before returning result checking the threshold
//     if (result) {
//         ESP_LOGI(TAG, "Buffer threshold reached. Current count = %d Capacity = %d", buffer_count, BUFFER_CAPACITY);
//     }
//     return result;
// }

// void buffer_push_to_flash() {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return;
//     }
//     ESP_LOGI(TAG, "Buffer threshold triggered. Pushing half of buffer entries to flash...");
//     int entries_to_push = BUFFER_CAPACITY_MACRO / 2; // Push half of the entries
//     int entries_pushed = 0;

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     for (int i = 0; i < buffer_count && entries_pushed < entries_to_push; i++) {
//         int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
//         Measurement *m = &buffer[index];

//         // Only process entries with dirty bit DIRTY_BIT_BUFFER_ONLY (0)
//         if (m->dirty_bit == DIRTY_BIT_BUFFER_ONLY) {
//             // Update dirty bit to DIRTY_BIT_IN_FLASH
//             m->dirty_bit = DIRTY_BIT_IN_FLASH;

//             // Store measurement in flash
//             if (store_measurement_in_flash(m)) {
//                 // Append timestamp to FIFO list
//                 append_timestamp_to_list(m->timestamp);
//                 entries_pushed++;
//                 ESP_LOGI(TAG, "Pushed timestamp = %" PRIu32 " from buffer to flash", m->timestamp);
//             } else {
//                 ESP_LOGE(TAG, "Failed to store measurement in flash");
//                 // Handle storage error if needed
//             }
//         }
//     }
//     xSemaphoreGive(buffer_mutex);
// }

// bool find_measurement_in_buffer(uint32_t timestamp, Measurement *result) {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return false;
//     }

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     for (int i = 0; i < buffer_count; i++) {
//         int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
//         if (buffer[index].timestamp == timestamp) {
//             *result = buffer[index];
//             xSemaphoreGive(buffer_mutex);
//             return true;
//         }
//     }
//     xSemaphoreGive(buffer_mutex);
//     return false;
// }

// // Retrieves measurements within the specified timestamp range from the buffer
// int get_measurements_from_buffer(uint32_t start_timestamp, uint32_t end_timestamp, Measurement *measurements, size_t max_measurements) {
//     if (buffer_mutex == NULL) {
//         ESP_LOGE(TAG, "Buffer mutex not initialized");
//         return -1;
//     }

//     int count = 0;

//     xSemaphoreTake(buffer_mutex, portMAX_DELAY);
//     for (int i = 0; i < buffer_count; i++) {
//         int index = (buffer_tail + i) % BUFFER_CAPACITY_MACRO;
//         Measurement *m = &buffer[index];

//         if (m->timestamp >= start_timestamp && m->timestamp <= end_timestamp) {
//             if (count < max_measurements) {
//                 measurements[count++] = *m;
//             } else {
//                 // Exceeded the maximum measurements that can be stored in the provided array
//                 break;
//             }
//         }
//     }
//     xSemaphoreGive(buffer_mutex);

//     return count; // Return the number of measurements found
// }