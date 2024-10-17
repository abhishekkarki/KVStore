#include "nvs_utils.h"
#include "esp_log.h"

#define TAG "NVS_UTILS"

const char *NVS_NAMESPACE = "storage";

esp_err_t init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

bool store_measurement_in_flash(Measurement *m) {
    if (is_measurement_in_flash(m->timestamp)) {
        // Measurement already in flash; skip writing
        return true;
    }

    char key[11]; // Max NVS key length is 15; timestamp in hex is 8 chars
    snprintf(key, sizeof(key), "%08" PRIx32, m->timestamp);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        size_t size = sizeof(Measurement);
        err = nvs_set_blob(handle, key, m, size);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
        if (err == ESP_OK) {
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
    return false;
}

bool find_measurement_in_flash(uint32_t timestamp, Measurement *result) {
    char key[11];
    snprintf(key, sizeof(key), "%08" PRIx32, timestamp);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t size = sizeof(Measurement);
        err = nvs_get_blob(handle, key, result, &size);
        nvs_close(handle);
        if (err == ESP_OK) {
            return true;
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            // Measurement not found
            return false;
        } else {
            ESP_LOGE(TAG, "Failed to get blob from NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
    return false;
}

bool is_measurement_in_flash(uint32_t timestamp) {
    char key[11];
    snprintf(key, sizeof(key), "%08" PRIx32, timestamp);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t size;
        err = nvs_get_blob(handle, key, NULL, &size);
        nvs_close(handle);
        return err == ESP_OK;
    }
    return false;
}