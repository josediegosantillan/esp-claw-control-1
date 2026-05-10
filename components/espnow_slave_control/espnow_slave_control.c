#include "espnow_slave_control.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "claw_cap.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "espnow_link.h"

static const char *TAG = "espnow_slave_ctrl";

#define ESPNOW_SLAVE_SEND_TEXT_SCHEMA \
    "{""type"":""object""," \
    "\"properties\":{\"peer_mac\":{\"type\":\"string\",\"description\":\"Peer MAC in AA:BB:CC:DD:EE:FF format.\"}," \
    "\"text\":{\"type\":\"string\",\"description\":\"UTF-8 payload to send by ESP-NOW.\"}}," \
    "\"required\":[\"peer_mac\",\"text\"]}"

static esp_err_t parse_mac_text(const char *text, uint8_t mac[6])
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

static esp_err_t espnow_slave_send_text_execute(const char *input_json,
                                                const claw_cap_call_context_t *ctx,
                                                char *output,
                                                size_t output_size)
{
    cJSON *root = NULL;
    const char *peer_mac_text = NULL;
    const char *text = NULL;
    uint8_t peer_mac[6] = {0};
    esp_err_t err;

    (void)ctx;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    output[0] = '\0';

    root = cJSON_Parse(input_json ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    peer_mac_text = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "peer_mac"));
    text = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "text"));
    if (!peer_mac_text || !peer_mac_text[0] || !text || !text[0]) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: peer_mac and text are required");
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(text) > ESPNOW_LINK_MAX_PAYLOAD) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: text too long (%u max)", (unsigned)ESPNOW_LINK_MAX_PAYLOAD);
        return ESP_ERR_INVALID_SIZE;
    }

    err = parse_mac_text(peer_mac_text, peer_mac);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: invalid peer_mac");
        return err;
    }

    if (!espnow_link_is_peer_exist(peer_mac)) {
        err = espnow_link_add_peer(peer_mac, false, NULL);
        if (err != ESP_OK) {
            cJSON_Delete(root);
            snprintf(output, output_size, "Error: add_peer failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    err = espnow_link_send(peer_mac, text, strlen(text));
    if (err != ESP_OK) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: send failed: %s", esp_err_to_name(err));
        return err;
    }

    snprintf(output, output_size, "OK peer=" MACSTR " text=%s", MAC2STR(peer_mac), text);
    cJSON_Delete(root);
    return ESP_OK;
}

static const claw_cap_descriptor_t s_espnow_slave_descriptors[] = {
    {
        .id = "espnow_send_text",
        .name = "ESP-NOW Send Text",
        .family = "espnow",
        .description = "Send a text payload to one ESP-NOW peer, auto-adding the peer if needed.",
        .kind = CLAW_CAP_KIND_CALLABLE,
        .cap_flags = 0,
        .input_schema_json = ESPNOW_SLAVE_SEND_TEXT_SCHEMA,
        .execute = espnow_slave_send_text_execute,
    },
};

static const claw_cap_group_t s_espnow_slave_group = {
    .group_id = "cap_espnow_slave_control",
    .plugin_name = "ESP-NOW Slave Control",
    .version = "1",
    .descriptors = s_espnow_slave_descriptors,
    .descriptor_count = sizeof(s_espnow_slave_descriptors) / sizeof(s_espnow_slave_descriptors[0]),
};

esp_err_t espnow_slave_control_register_group(void)
{
    if (claw_cap_group_exists(s_espnow_slave_group.group_id)) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Registering ESP-NOW slave control capability");
    return claw_cap_register_group(&s_espnow_slave_group);
}
