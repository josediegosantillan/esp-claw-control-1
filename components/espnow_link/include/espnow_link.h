#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESPNOW_LINK_MAX_PAYLOAD 250

typedef struct {
    bool initialized;
    uint8_t channel;
    bool long_range;
    int peer_count;
    uint32_t version;
} espnow_link_status_t;

esp_err_t espnow_link_init(void);
esp_err_t espnow_link_start(uint8_t channel, bool long_range);
esp_err_t espnow_link_add_peer(const uint8_t mac[6], bool encrypt, const uint8_t lmk[16]);
esp_err_t espnow_link_remove_peer(const uint8_t mac[6]);
bool espnow_link_is_peer_exist(const uint8_t mac[6]);
esp_err_t espnow_link_send(const uint8_t mac[6], const void *data, size_t len);
esp_err_t espnow_link_get_status(espnow_link_status_t *status);
esp_err_t espnow_link_set_pmk(const uint8_t pmk[16]);

#ifdef __cplusplus
}
#endif
