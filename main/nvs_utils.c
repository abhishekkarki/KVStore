#include "nvs_utils.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <inttypes.h>

#include "mqtt_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"  // For esp_random()
#include "mqtt_topics.h"
#include "timestamp_list.h"  // [FIX] ensure we have TIMESTAMP_LIST_KEY
// ─────────────────────────────────────────────────────────────────────────────
static const char *TAG = "NVS_UTILS";
// ─────────────────────────────────────────────────────────────────────────────
esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
        err == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGW(TAG, "NVS partition truncated or not init; erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}
// ─────────────────────────────────────────────────────────────────────────────
bool store_measurement_in_flash(Measurement *m) {
    if (!m) {
        ESP_LOGE(TAG, "Measurement is NULL");
        return false;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    char key[16]; // [FIX] unify key size
    snprintf(key, sizeof(key), "%" PRIu32, m->timestamp);

    // see if entry already exists
    size_t sz = 0;
    err = nvs_get_blob(handle, key, NULL, &sz);
    if (err == ESP_OK) {
        // entry already exists
        nvs_close(handle);
        return true;
    }

    err = nvs_set_blob(handle, key, m, sizeof(Measurement));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set blob in NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Stored measurement timestamp=%" PRIu32 " in flash key=%s", m->timestamp, key);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool find_measurement_in_flash(uint32_t timestamp, Measurement *result) {
    if (!result) {
        ESP_LOGE(TAG, "Result pointer is NULL");
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    char key[16]; // [FIX]
    snprintf(key, sizeof(key), "%" PRIu32, timestamp);

    size_t sz = sizeof(Measurement);
    err = nvs_get_blob(handle, key, result, &sz);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed get blob for key=%s: %s", key, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "Found measurement timestamp=%" PRIu32 " in flash (key=%s)", timestamp, key);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void clear_flash_storage(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed erase NVS storage: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Flash storage cleared.");
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed commit after erase: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
}

// ─────────────────────────────────────────────────────────────────────────────
uint32_t get_flash_usage_percent(void) {
    nvs_stats_t st;
    esp_err_t err = nvs_get_stats("nvs", &st);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(err));
        return 0;
    }
    if (st.total_entries == 0) {
        return 0;
    }
    return (st.used_entries * 100U) / st.total_entries;
}

// ─────────────────────────────────────────────────────────────────────────────
bool retrieve_measurement_from_flash(uint32_t timestamp, Measurement *m) {
    if (!m) {
        ESP_LOGE(TAG, "Measurement pointer is NULL");
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    char key[16];
    snprintf(key, sizeof(key), "%" PRIu32, timestamp);

    size_t sz = sizeof(Measurement);
    err = nvs_get_blob(handle, key, m, &sz);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed get measurement for key=%s: %s", key, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "Retrieved measurement timestamp=%" PRIu32 " from flash key=%s", timestamp, key);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// -------------- Edge retrieve logic truncated for brevity... --------------

// Range retrieval from flash
int get_measurements_from_flash(uint32_t start_timestamp, uint32_t end_timestamp,
                                Measurement *measurements, size_t max_measurements)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed nvs_open: %s", esp_err_to_name(err));
        return -1;
    }

    // We assume TIMESTAMP_LIST_KEY is defined in "timestamp_list.h"
    size_t list_size = 0;
    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, NULL, &list_size);
    if (err != ESP_OK || list_size == 0) {
        ESP_LOGE(TAG, "Failed to get timestamp list or empty. err=%s", esp_err_to_name(err));
        nvs_close(handle);
        return -1;
    }

    size_t total_entries = list_size / sizeof(uint32_t);
    uint32_t *timestamp_list = malloc(list_size);
    if (!timestamp_list) {
        ESP_LOGE(TAG, "Malloc fail for timestamp_list");
        nvs_close(handle);
        return -1;
    }

    err = nvs_get_blob(handle, TIMESTAMP_LIST_KEY, timestamp_list, &list_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed get blob for TS list: %s", esp_err_to_name(err));
        free(timestamp_list);
        nvs_close(handle);
        return -1;
    }
    nvs_close(handle);

    int count = 0;
    for (size_t i = 0; i < total_entries && (int)count < (int)max_measurements; i++) {
        uint32_t ts = timestamp_list[i];
        if (ts >= start_timestamp && ts <= end_timestamp) {
            Measurement tmp;
            if (retrieve_measurement_from_flash(ts, &tmp)) {
                measurements[count++] = tmp;
            } else {
                ESP_LOGW(TAG, "Could not retrieve flash item for TS=%" PRIu32, ts);
            }
        }
    }
    ESP_LOGI(TAG, "Found %d measurements in flash range [%"PRIu32", %"PRIu32"]", count, start_timestamp, end_timestamp);

    free(timestamp_list);
    return count;
}