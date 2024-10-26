// nvs_utils.c

#include "nvs_utils.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <inttypes.h>  // Include this header

static const char *TAG = "NVS_UTILS";

static uint32_t flash_measurement_count = 0;
static const uint32_t FLASH_MAX_MEASUREMENTS = 1000; // Adjust based on your storage capacity

esp_err_t init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

bool store_measurement_in_flash(Measurement *m) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    char key[11]; // Enough to hold 8 hex digits + null terminator
    snprintf(key, sizeof(key), "%08" PRIX32, m->timestamp);  // Use PRIX32

    err = nvs_set_blob(handle, key, m, sizeof(Measurement));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set blob in NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);

    flash_measurement_count++;

    return true;
}

bool find_measurement_in_flash(uint32_t timestamp, Measurement *result) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    char key[11];
    snprintf(key, sizeof(key), "%08" PRIX32, timestamp);  // Use PRIX32

    size_t required_size = sizeof(Measurement);
    err = nvs_get_blob(handle, key, result, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get blob from NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

void clear_flash_storage() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS storage: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Flash storage cleared.");
    }

    nvs_close(handle);
    flash_measurement_count = 0;
}

uint32_t get_flash_usage_percent() {
    return (flash_measurement_count * 100) / FLASH_MAX_MEASUREMENTS;
}

