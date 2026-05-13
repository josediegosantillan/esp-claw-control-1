#include "espnow_link.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "claw_event_publisher.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"

static const char *TAG = "espnow_link";
static const char *RADIO_TAG = "radio";
static const char *PORTON_LAST_CHAT_PATH = "/fatfs/porton_last_chat.txt";
static const char *PORTON_STATE_PATH = "/fatfs/porton_state.txt";

#define ESPNOW_LINK_CONFIRM_QUEUE_LEN 4
#define ESPNOW_LINK_CONFIRM_TEXT_LEN 32
#define ESPNOW_LINK_CHAT_ID_LEN 32

typedef struct {
    uint8_t src_mac[6];
    int rssi;
    char text[ESPNOW_LINK_CONFIRM_TEXT_LEN];
} espnow_link_confirm_event_t;

typedef enum {
    PORTON_RX_KIND_NONE = 0,
    PORTON_RX_KIND_CONFIRM,
    PORTON_RX_KIND_REMOTE_EVENT,
} porton_rx_kind_t;

static bool s_initialized;
static uint8_t s_channel;
static bool s_long_range;
static QueueHandle_t s_confirm_queue;
static TaskHandle_t s_confirm_task;

static void trim_ascii(char *text)
{
    if (!text) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start++;
    }

    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

static bool parse_mac_string(const char *text, uint8_t mac[6])
{
    if (!text || !mac) {
        return false;
    }

    unsigned int values[6] = {0};
    if (sscanf(text,
               "%2x:%2x:%2x:%2x:%2x:%2x",
               &values[0],
               &values[1],
               &values[2],
               &values[3],
               &values[4],
               &values[5]) != 6) {
        return false;
    }

    for (size_t i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }

    return true;
}

static bool parse_porton_confirm_text(const char *text, const char **state_label)
{
    if (!text || !state_label) {
        return false;
    }

    if (strcmp(text, "ok relay=on") == 0) {
        *state_label = "ENCENDIDA";
        return true;
    }

    if (strcmp(text, "ok relay=off") == 0) {
        *state_label = "APAGADA";
        return true;
    }

    return false;
}

static bool parse_porton_remote_event_text(const char *text, const char **state_label)
{
    if (!text || !state_label) {
        return false;
    }

    if (strcmp(text, "event relay=on") == 0) {
        *state_label = "ENCENDIDA";
        return true;
    }

    if (strcmp(text, "event relay=off") == 0) {
        *state_label = "APAGADA";
        return true;
    }

    return false;
}

static void log_wifi_radio_state(const char *label)
{
    uint8_t self_mac[6] = {0};
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    int8_t tx_power = 0;

    esp_err_t mac_err = esp_wifi_get_mac(WIFI_IF_STA, self_mac);
    esp_err_t channel_err = esp_wifi_get_channel(&primary, &second);
    esp_err_t tx_err = esp_wifi_get_max_tx_power(&tx_power);

    if (mac_err == ESP_OK && channel_err == ESP_OK && tx_err == ESP_OK) {
        ESP_LOGI(RADIO_TAG,
                 "%s self=" MACSTR " channel=%u tx_power_raw=%d approx_dbm=%d",
                 label,
                 MAC2STR(self_mac),
                 primary,
                 tx_power,
                 tx_power / 4);
        return;
    }

    ESP_LOGW(RADIO_TAG,
             "%s mac_err=%s channel_err=%s tx_err=%s",
             label,
             esp_err_to_name(mac_err),
             esp_err_to_name(channel_err),
             esp_err_to_name(tx_err));
}

static const char *porton_rssi_quality_label(int rssi)
{
    if (rssi >= -55) {
        return "EXCELENTE";
    }

    if (rssi >= -67) {
        return "BUENA";
    }

    if (rssi >= -75) {
        return "REGULAR";
    }

    return "MALA";
}

static const char *porton_confirm_event_key(const char *text)
{
    if (!text) {
        return NULL;
    }

    if (strcmp(text, "ok relay=on") == 0) {
        return "on";
    }

    if (strcmp(text, "ok relay=off") == 0) {
        return "off";
    }

    return NULL;
}

static const char *porton_remote_event_key(const char *text)
{
    if (!text) {
        return NULL;
    }

    if (strcmp(text, "event relay=on") == 0) {
        return "on";
    }

    if (strcmp(text, "event relay=off") == 0) {
        return "off";
    }

    return NULL;
}

static bool is_porton_confirm_payload(const uint8_t *data, size_t len)
{
    if (!data) {
        return false;
    }

    return (len == strlen("ok relay=on") && memcmp(data, "ok relay=on", len) == 0) ||
           (len == strlen("ok relay=off") && memcmp(data, "ok relay=off", len) == 0);
}

static bool is_porton_remote_event_payload(const uint8_t *data, size_t len)
{
    if (!data) {
        return false;
    }

    return (len == strlen("event relay=on") && memcmp(data, "event relay=on", len) == 0) ||
           (len == strlen("event relay=off") && memcmp(data, "event relay=off", len) == 0);
}

static porton_rx_kind_t detect_porton_rx_kind(const uint8_t *data, size_t len)
{
    if (is_porton_confirm_payload(data, len)) {
        return PORTON_RX_KIND_CONFIRM;
    }

    if (is_porton_remote_event_payload(data, len)) {
        return PORTON_RX_KIND_REMOTE_EVENT;
    }

    return PORTON_RX_KIND_NONE;
}

static void persist_porton_confirmed_state(const char *event_key)
{
    if (!event_key) {
        return;
    }

    const char *state = NULL;
    if (strcmp(event_key, "on") == 0) {
        state = "on";
    } else if (strcmp(event_key, "off") == 0) {
        state = "off";
    } else {
        return;
    }

    FILE *file = fopen(PORTON_STATE_PATH, "w");
    if (!file) {
        ESP_LOGW(TAG, "porton confirm state persist failed: fopen(%s)", PORTON_STATE_PATH);
        return;
    }

    fprintf(file, "%s\n", state);
    fclose(file);
}

static esp_err_t load_porton_chat_route(char *chat_id, size_t chat_id_size, uint8_t peer_mac[6])
{
    if (!chat_id || chat_id_size == 0 || !peer_mac) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(PORTON_LAST_CHAT_PATH, "r");
    if (!file) {
        return ESP_ERR_NOT_FOUND;
    }

    char chat_line[ESPNOW_LINK_CHAT_ID_LEN] = {0};
    char mac_line[32] = {0};

    char *chat_ok = fgets(chat_line, sizeof(chat_line), file);
    char *mac_ok = fgets(mac_line, sizeof(mac_line), file);
    fclose(file);

    if (!chat_ok || !mac_ok) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    trim_ascii(chat_line);
    trim_ascii(mac_line);

    if (chat_line[0] == '\0' || !parse_mac_string(mac_line, peer_mac)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    snprintf(chat_id, chat_id_size, "%s", chat_line);
    return ESP_OK;
}

static void espnow_link_confirm_task_fn(void *arg)
{
    (void)arg;

    espnow_link_confirm_event_t event = {0};
    for (;;) {
        if (xQueueReceive(s_confirm_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const char *state_label = NULL;
        const char *event_key = NULL;
        const char *rssi_quality = NULL;
        porton_rx_kind_t rx_kind = detect_porton_rx_kind((const uint8_t *)event.text, strlen(event.text));

        if (rx_kind == PORTON_RX_KIND_CONFIRM) {
            if (!parse_porton_confirm_text(event.text, &state_label)) {
                continue;
            }
            event_key = porton_confirm_event_key(event.text);
        } else if (rx_kind == PORTON_RX_KIND_REMOTE_EVENT) {
            if (!parse_porton_remote_event_text(event.text, &state_label)) {
                continue;
            }
            event_key = porton_remote_event_key(event.text);
        } else {
            continue;
        }

        if (!event_key) {
            continue;
        }
        rssi_quality = porton_rssi_quality_label(event.rssi);

        persist_porton_confirmed_state(event_key);

        if (rx_kind == PORTON_RX_KIND_CONFIRM) {
            char chat_id[ESPNOW_LINK_CHAT_ID_LEN] = {0};
            uint8_t expected_mac[6] = {0};
            esp_err_t route_err = load_porton_chat_route(chat_id, sizeof(chat_id), expected_mac);
            if (route_err != ESP_OK) {
                ESP_LOGD(TAG, "porton confirm ignored: chat route unavailable (%s)", esp_err_to_name(route_err));
                continue;
            }

            if (memcmp(expected_mac, event.src_mac, sizeof(expected_mac)) != 0) {
                ESP_LOGD(TAG, "porton confirm ignored: unexpected peer " MACSTR, MAC2STR(event.src_mac));
                continue;
            }

            char payload[192] = {0};
            snprintf(payload,
                     sizeof(payload),
                     "{\"chat_id\":\"%s\",\"state\":\"%s\",\"rssi\":%d,\"rssi_quality\":\"%s\"}",
                     chat_id,
                     state_label,
                     event.rssi,
                     rssi_quality);
            esp_err_t send_err = claw_event_router_publish_trigger("espnow_link",
                                                                   "espnow_porton_confirm",
                                                                   event_key,
                                                                   payload);
            if (send_err == ESP_OK) {
                ESP_LOGI(TAG, "porton confirm published chat=%s peer=" MACSTR " state=%s rssi=%d quality=%s",
                         chat_id, MAC2STR(event.src_mac), state_label, event.rssi, rssi_quality);
            } else {
                ESP_LOGW(TAG, "porton confirm publish failed chat=%s: %s", chat_id, esp_err_to_name(send_err));
            }
            continue;
        }

        if (rx_kind == PORTON_RX_KIND_REMOTE_EVENT) {
            char payload[160] = {0};
            snprintf(payload,
                     sizeof(payload),
                     "{\"state\":\"%s\",\"rssi\":%d,\"rssi_quality\":\"%s\",\"peer_mac\":\"" MACSTR "\"}",
                     state_label,
                     event.rssi,
                     rssi_quality,
                     MAC2STR(event.src_mac));
            esp_err_t send_err = claw_event_router_publish_trigger("espnow_link",
                                                                   "espnow_porton_remote_event",
                                                                   event_key,
                                                                   payload);
            if (send_err == ESP_OK) {
                ESP_LOGI(TAG, "porton remote event published peer=" MACSTR " state=%s rssi=%d quality=%s",
                         MAC2STR(event.src_mac), state_label, event.rssi, rssi_quality);
            } else {
                ESP_LOGW(TAG, "porton remote event publish failed peer=" MACSTR ": %s",
                         MAC2STR(event.src_mac), esp_err_to_name(send_err));
            }
        }
    }
}

static esp_err_t ensure_confirm_task(void)
{
    if (s_confirm_queue && s_confirm_task) {
        return ESP_OK;
    }

    s_confirm_queue = xQueueCreate(ESPNOW_LINK_CONFIRM_QUEUE_LEN, sizeof(espnow_link_confirm_event_t));
    ESP_RETURN_ON_FALSE(s_confirm_queue != NULL, ESP_ERR_NO_MEM, TAG, "confirm queue alloc failed");

    BaseType_t task_ok = xTaskCreate(espnow_link_confirm_task_fn,
                                     "espnow_confirm",
                                     4096,
                                     NULL,
                                     5,
                                     &s_confirm_task);
    if (task_ok != pdPASS) {
        vQueueDelete(s_confirm_queue);
        s_confirm_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

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
    ESP_LOGI("espnow", "rx from " MACSTR " len=%d rssi=%d text=%.*s",
             MAC2STR(recv_info->src_addr), data_len, rssi, data_len, (const char *)data);

    if (!s_confirm_queue) {
        return;
    }

    if (detect_porton_rx_kind(data, (size_t)data_len) == PORTON_RX_KIND_NONE) {
        return;
    }

    espnow_link_confirm_event_t event = {0};
    memcpy(event.src_mac, recv_info->src_addr, sizeof(event.src_mac));
    event.rssi = rssi;

    size_t copy_len = (size_t)data_len;
    if (copy_len >= sizeof(event.text)) {
        copy_len = sizeof(event.text) - 1;
    }
    memcpy(event.text, data, copy_len);
    event.text[copy_len] = '\0';

    if (xQueueSend(s_confirm_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "confirm queue full, dropping rx peer=" MACSTR, MAC2STR(recv_info->src_addr));
    }
}

esp_err_t espnow_link_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init failed");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(espnow_link_send_cb), TAG, "register send cb failed");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(espnow_link_recv_cb), TAG, "register recv cb failed");
    ESP_RETURN_ON_ERROR(ensure_confirm_task(), TAG, "confirm task init failed");

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
    ESP_RETURN_ON_ERROR(esp_wifi_set_max_tx_power(80), TAG, "set STA max TX power failed");

    s_channel = channel;
    s_long_range = long_range;

    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "ESP-NOW ready channel=%u lr=%d self=" MACSTR, s_channel, s_long_range, MAC2STR(mac));
    } else {
        ESP_LOGI(TAG, "ESP-NOW ready channel=%u lr=%d", s_channel, s_long_range);
    }
    log_wifi_radio_state("espnow ready");

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
