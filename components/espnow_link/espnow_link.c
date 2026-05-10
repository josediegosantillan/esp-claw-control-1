#include "espnow_link.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"

static const char *TAG = "espnow_link";

static bool s_initialized;
static uint8_t s_channel;
static bool s_long_range;

static void espnow_link_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (!tx_info) {
        ESP_LOGW(TAG, "Send callback without tx_info");
        return;
    }

    ESP_LOGI(TAG, "tx peer=" MACSTR " status=%s",
             MAC2STR(tx_info->des_addr),
             status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

static void espnow_link_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (!recv_info || !recv_info->src_addr || !data || data_len <= 0) {
        ESP_LOGW(TAG, "Receive callback with invalid arguments");
        return;
    }

    int rssi = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : 0;
    ESP_LOGI(TAG, "rx peer=" MACSTR " len=%d rssi=%d data=%.*s",
             MAC2STR(recv_info->src_addr),
             data_len,
             rssi,
             data_len,
             (const char *)data);
}

esp_err_t espnow_link_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init failed");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(espnow_link_send_cb), TAG, "register send cb failed");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(espnow_link_recv_cb), TAG, "register recv cb failed");

    uint32_t version = 0;
    esp_err_t version_err = esp_now_get_version(&version);
    if (version_err == ESP_OK) {
        ESP_LOGI(TAG, "ESP-NOW initialized version=%" PRIu32, version);
    } else {
        ESP_LOGW(TAG, "ESP-NOW initialized but version query failed: %s", esp_err_to_name(version_err));
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t espnow_link_start(uint8_t channel, bool long_range)
{
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;

    ESP_RETURN_ON_ERROR(espnow_link_init(), TAG, "ESP-NOW init failed");

    if (channel == 0) {
        ESP_RETURN_ON_ERROR(esp_wifi_get_channel(&channel, &second), TAG, "read Wi-Fi channel failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE), TAG, "set Wi-Fi channel failed");
    }

    if (long_range) {
        ESP_RETURN_ON_ERROR(
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR),
            TAG,
            "enable LR on STA failed");
    }

    s_channel = channel;
    s_long_range = long_range;

    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "ESP-NOW ready channel=%u lr=%d self=" MACSTR, s_channel, s_long_range, MAC2STR(mac));
    } else {
        ESP_LOGI(TAG, "ESP-NOW ready channel=%u lr=%d", s_channel, s_long_range);
    }

    return ESP_OK;
}

esp_err_t espnow_link_set_pmk(const uint8_t pmk[16])
{
    if (!pmk) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(espnow_link_init(), TAG, "ESP-NOW init failed");
    return esp_now_set_pmk(pmk);
}

esp_err_t espnow_link_add_peer(const uint8_t mac[6], bool encrypt, const uint8_t lmk[16])
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(espnow_link_init(), TAG, "ESP-NOW init failed");

    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = s_channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = encrypt;

    if (encrypt) {
        if (!lmk) {
            return ESP_ERR_INVALID_ARG;
        }
        memcpy(peer.lmk, lmk, 16);
    }

    return esp_now_add_peer(&peer);
}

esp_err_t espnow_link_remove_peer(const uint8_t mac[6])
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(espnow_link_init(), TAG, "ESP-NOW init failed");
    return esp_now_del_peer(mac);
}

bool espnow_link_is_peer_exist(const uint8_t mac[6])
{
    if (!mac || !s_initialized) {
        return false;
    }

    return esp_now_is_peer_exist(mac);
}

esp_err_t espnow_link_send(const uint8_t mac[6], const void *data, size_t len)
{
    if (!mac || !data || len == 0 || len > ESPNOW_LINK_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(espnow_link_init(), TAG, "ESP-NOW init failed");
    return esp_now_send(mac, data, len);
}

esp_err_t espnow_link_get_status(espnow_link_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(*status));
    status->initialized = s_initialized;
    status->channel = s_channel;
    status->long_range = s_long_range;

    if (s_initialized) {
        esp_now_peer_num_t peer_num = {0};
        ESP_RETURN_ON_ERROR(esp_now_get_peer_num(&peer_num), TAG, "peer count query failed");
        status->peer_count = peer_num.total_num;
        ESP_RETURN_ON_ERROR(esp_now_get_version(&status->version), TAG, "version query failed");
    }

    return ESP_OK;
}
