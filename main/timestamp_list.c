#include "timestamp_list.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
// ─────────────────────────────────────────────────────────────────────────────
static const char *TAG = "TIMESTAMP_LIST";
#define TIMESTAMP_LIST_KEY "timestamp_list"
#define NAMESPACE "storage"

// ─────────────────────────────────────────────────────────────────────────────
void append_timestamp_to_list(uint32_t timestamp) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    size_t list_size = 0;
    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, NULL, &list_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error getting timestamp list size: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    size_t new_list_size = list_size + sizeof(uint32_t);
    uint32_t *timestamp_list = malloc(new_list_size);
    if (timestamp_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for timestamp list");
        nvs_close(handle);
        return;
    }

    if (list_size > 0) {
        err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, &list_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error getting timestamp list: %s", esp_err_to_name(err));
            free(timestamp_list);
            nvs_close(handle);
            return;
        }
    }

    timestamp_list[list_size / sizeof(uint32_t)] = timestamp;

    err = nvs_set_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, new_list_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting timestamp list: %s", esp_err_to_name(err));
    } else {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Appended timestamp %" PRIu32 " to FIFO list", timestamp);
    }

    free(timestamp_list);
    nvs_close(handle);
}

// ─────────────────────────────────────────────────────────────────────────────
void remove_timestamps_from_list(size_t count) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    size_t list_size = 0;
    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, NULL, &list_size);
    if (err != ESP_OK || list_size == 0) {
        ESP_LOGW(TAG, "Timestamp list is empty or error occurred: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    uint32_t *timestamp_list = malloc(list_size);
    if (timestamp_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for timestamp list");
        nvs_close(handle);
        return;
    }

    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, &list_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting timestamp list: %s", esp_err_to_name(err));
        free(timestamp_list);
        nvs_close(handle);
        return;
    }

    size_t total_entries = list_size / sizeof(uint32_t);
    if (count > total_entries) {
        count = total_entries;
    }

    size_t new_list_size = (total_entries - count) * sizeof(uint32_t);
    if (new_list_size > 0) {
        memmove(timestamp_list, &timestamp_list[count], new_list_size);
        err = nvs_set_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, new_list_size);
    } else {
        err = nvs_erase_key(handle, TIMESTAMP_LIST_KEY);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error updating timestamp list: %s", esp_err_to_name(err));
    } else {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Removed %" PRIu32 " timestamps from FIFO list", (uint32_t)count);
    }

    free(timestamp_list);
    nvs_close(handle);
}

// ─────────────────────────────────────────────────────────────────────────────
void get_timestamps_from_list(size_t count, uint32_t *timestamps, size_t *out_count) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        *out_count = 0;
        return;
    }

    size_t list_size = 0;
    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, NULL, &list_size);
    if (err != ESP_OK || list_size == 0) {
        ESP_LOGW(TAG, "Timestamp list is empty or error occurred: %s", esp_err_to_name(err));
        *out_count = 0;
        nvs_close(handle);
        return;
    }

    uint32_t *timestamp_list = malloc(list_size);
    if (timestamp_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for timestamp list");
        *out_count = 0;
        nvs_close(handle);
        return;
    }

    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, &list_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting timestamp list: %s", esp_err_to_name(err));
        free(timestamp_list);
        *out_count = 0;
        nvs_close(handle);
        return;
    }

    size_t total_entries = list_size / sizeof(uint32_t);
    if (count > total_entries) {
        count = total_entries;
    }

    memcpy(timestamps, timestamp_list, count * sizeof(uint32_t));
    *out_count = count;
    ESP_LOGI(TAG, "Retrieved %" PRIu32 " timestamps from FIFO list", (uint32_t)*out_count);

    free(timestamp_list);
    nvs_close(handle);
}