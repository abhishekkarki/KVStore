idf_component_register(SRCS  "main.c" "nvs_utils.c" "buffer.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash dht esp_wifi lwip esp_netif esp_event esp_timer esp_http_server)
                       