#include <inttypes.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "espnow_link.h"

#define TAG "CMD_ESPNOW"

static struct {
    struct arg_lit *status;
    struct arg_lit *start;
    struct arg_lit *add_peer;
    struct arg_lit *del_peer;
    struct arg_lit *send;
    struct arg_int *channel;
    struct arg_lit *long_range;
    struct arg_str *peer;
    struct arg_str *text;
    struct arg_lit *encrypt;
    struct arg_str *lmk;
    struct arg_end *end;
} espnow_args;

static int log_error(const char *command, esp_err_t err, const char *message)
{
    ESP_LOGW(TAG, "cmd=%s ok=0 err=%s msg=%s", command, esp_err_to_name(err), message);
    return 1;
}

static bool parse_hex_nibble(char ch, uint8_t *value)
{
    if (ch >= '0' && ch <= '9') {
        *value = (uint8_t)(ch - '0');
        return true;
    }
    ch = (char)tolower((unsigned char)ch);
    if (ch >= 'a' && ch <= 'f') {
        *value = (uint8_t)(ch - 'a' + 10);
        return true;
    }
    return false;
}

static esp_err_t parse_mac(const char *text, uint8_t mac[6])
{
    unsigned int values[6] = {0};

    if (!text || !mac) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < 6; ++i) {
        mac[i] = (uint8_t)values[i];
    }

    return ESP_OK;
}

static esp_err_t parse_key_16(const char *text, uint8_t out[16])
{
    size_t len = 0;

    if (!text || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    len = strlen(text);
    if (len != 32) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < 16; ++i) {
        uint8_t hi = 0;
        uint8_t lo = 0;
        if (!parse_hex_nibble(text[i * 2], &hi) || !parse_hex_nibble(text[i * 2 + 1], &lo)) {
            return ESP_ERR_INVALID_ARG;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return ESP_OK;
}

static int cmd_espnow_status(void)
{
    espnow_link_status_t status = {0};
    esp_err_t err = espnow_link_get_status(&status);
    if (err != ESP_OK) {
        return log_error("status", err, "status_query_failed");
    }

    ESP_LOGI(TAG, "cmd=status ok=1 initialized=%d channel=%u long_range=%d peers=%d version=%" PRIu32,
             status.initialized, status.channel, status.long_range, status.peer_count, status.version);
    return 0;
}

static int cmd_espnow_start(void)
{
    int channel = espnow_args.channel->count ? espnow_args.channel->ival[0] : 0;
    espnow_link_status_t status = {0};

    if (channel < 0 || channel > 14) {
        return log_error("start", ESP_ERR_INVALID_ARG, "invalid_channel");
    }

    esp_err_t err = espnow_link_start((uint8_t)channel, espnow_args.long_range->count > 0);
    if (err != ESP_OK) {
        return log_error("start", err, "start_failed");
    }

    err = espnow_link_get_status(&status);
    if (err != ESP_OK) {
        return log_error("start", err, "status_query_failed");
    }

    ESP_LOGI(TAG, "cmd=start ok=1 channel=%u long_range=%d", status.channel, status.long_range);
    return 0;
}

static int cmd_espnow_add_peer(void)
{
    uint8_t mac[6] = {0};
    uint8_t lmk[16] = {0};
    const uint8_t *lmk_ptr = NULL;
    esp_err_t err;

    if (!espnow_args.peer->count) {
        return log_error("add-peer", ESP_ERR_INVALID_ARG, "missing_peer");
    }

    err = parse_mac(espnow_args.peer->sval[0], mac);
    if (err != ESP_OK) {
        return log_error("add-peer", err, "invalid_peer_mac");
    }

    if (espnow_args.encrypt->count) {
        if (!espnow_args.lmk->count) {
            return log_error("add-peer", ESP_ERR_INVALID_ARG, "missing_lmk");
        }
        err = parse_key_16(espnow_args.lmk->sval[0], lmk);
        if (err != ESP_OK) {
            return log_error("add-peer", err, "invalid_lmk");
        }
        lmk_ptr = lmk;
    }

    err = espnow_link_add_peer(mac, espnow_args.encrypt->count > 0, lmk_ptr);
    if (err != ESP_OK) {
        return log_error("add-peer", err, "add_peer_failed");
    }

    ESP_LOGI(TAG, "cmd=add-peer ok=1 peer=" MACSTR " encrypt=%d", MAC2STR(mac), espnow_args.encrypt->count > 0);
    return 0;
}

static int cmd_espnow_del_peer(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err;

    if (!espnow_args.peer->count) {
        return log_error("del-peer", ESP_ERR_INVALID_ARG, "missing_peer");
    }

    err = parse_mac(espnow_args.peer->sval[0], mac);
    if (err != ESP_OK) {
        return log_error("del-peer", err, "invalid_peer_mac");
    }

    err = espnow_link_remove_peer(mac);
    if (err != ESP_OK) {
        return log_error("del-peer", err, "remove_peer_failed");
    }

    ESP_LOGI(TAG, "cmd=del-peer ok=1 peer=" MACSTR, MAC2STR(mac));
    return 0;
}

static int cmd_espnow_send(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err;

    if (!espnow_args.peer->count) {
        return log_error("send", ESP_ERR_INVALID_ARG, "missing_peer");
    }
    if (!espnow_args.text->count) {
        return log_error("send", ESP_ERR_INVALID_ARG, "missing_text");
    }

    err = parse_mac(espnow_args.peer->sval[0], mac);
    if (err != ESP_OK) {
        return log_error("send", err, "invalid_peer_mac");
    }

    err = espnow_link_send(mac, espnow_args.text->sval[0], strlen(espnow_args.text->sval[0]));
    if (err != ESP_OK) {
        return log_error("send", err, "send_failed");
    }

    ESP_LOGI(TAG, "cmd=send ok=1 peer=" MACSTR " len=%u text=%s",
             MAC2STR(mac),
             (unsigned int)strlen(espnow_args.text->sval[0]),
             espnow_args.text->sval[0]);
    return 0;
}

static int espnow_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&espnow_args);
    int op_count = espnow_args.status->count + espnow_args.start->count + espnow_args.add_peer->count +
                   espnow_args.del_peer->count + espnow_args.send->count;

    if (nerrors != 0) {
        return log_error("parse", ESP_ERR_INVALID_ARG, "invalid_arguments");
    }

    if (op_count != 1) {
        return log_error("parse", ESP_ERR_INVALID_ARG,
                         "exactly_one_of_status_start_add-peer_del-peer_send_required");
    }

    if (espnow_args.status->count) {
        return cmd_espnow_status();
    }
    if (espnow_args.start->count) {
        return cmd_espnow_start();
    }
    if (espnow_args.add_peer->count) {
        return cmd_espnow_add_peer();
    }
    if (espnow_args.del_peer->count) {
        return cmd_espnow_del_peer();
    }
    return cmd_espnow_send();
}

void register_espnow_command(void)
{
    espnow_args.status = arg_lit0(NULL, "status", "Print ESP-NOW runtime status");
    espnow_args.start = arg_lit0(NULL, "start", "Initialize ESP-NOW and set Wi-Fi channel");
    espnow_args.add_peer = arg_lit0(NULL, "add-peer", "Add an ESP-NOW peer");
    espnow_args.del_peer = arg_lit0(NULL, "del-peer", "Remove an ESP-NOW peer");
    espnow_args.send = arg_lit0(NULL, "send", "Send text payload to a peer");
    espnow_args.channel = arg_int0(NULL, "channel", "<1-14>", "Wi-Fi channel used by ESP-NOW");
    espnow_args.long_range = arg_lit0(NULL, "long-range", "Enable 802.11 LR on STA");
    espnow_args.peer = arg_str0(NULL, "peer", "<mac>", "Peer MAC, format AA:BB:CC:DD:EE:FF");
    espnow_args.text = arg_str0(NULL, "text", "<payload>", "UTF-8 payload to send");
    espnow_args.encrypt = arg_lit0(NULL, "encrypt", "Add peer with encryption enabled");
    espnow_args.lmk = arg_str0(NULL, "lmk", "<32-hex>", "16-byte LMK in hex");
    espnow_args.end = arg_end(12);

    const esp_console_cmd_t espnow_cmd = {
        .command = "espnow",
        .help = "ESP-NOW operations.\n"
                "Examples:\n"
                " espnow --start\n"
                " espnow --start --channel 6\n"
                " espnow --add-peer --peer 24:6F:28:AA:BB:CC\n"
                " espnow --send --peer 24:6F:28:AA:BB:CC --text hola\n"
                " espnow --status\n",
        .func = espnow_func,
        .argtable = &espnow_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&espnow_cmd));
}
