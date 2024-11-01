#include "nvs_utils.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <inttypes.h>

static const char *TAG = "NVS_UTILS";

esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
        err == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGW(TAG, "NVS partition was truncated or not initialized. Erasing and re-initializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

bool store_measurement_in_flash(Measurement *m) {
    if (m == NULL) {
        ESP_LOGE(TAG, "Measurement is NULL");
        return false;
    }

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
    ESP_LOGI(TAG, "Stored measurement in flash with key %s", key);

    nvs_close(handle);

    return true;
}

bool find_measurement_in_flash(uint32_t timestamp, Measurement *result) {
    if (result == NULL) {
        ESP_LOGE(TAG, "Result pointer is NULL");
        return false;
    }

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

void clear_flash_storage(void) {
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

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
}

uint32_t get_flash_usage_percent() {
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(err));
        return 0;
    }

    size_t used_entries = nvs_stats.used_entries;
    size_t total_entries = nvs_stats.total_entries;

    if (total_entries == 0) {
        return 0;
    }

    uint32_t usage_percent = (used_entries * 100) / total_entries;
    return usage_percent;
}