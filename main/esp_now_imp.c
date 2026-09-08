/*
 * esp_now_imp.c — SAME CODE FOR BOTH BOARDS
 * Matches official ESP-IDF v6 ESP-NOW example exactly
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_now_imp.h"

static const char *TAG = "ESPNOW_HELLO";

#define WIFI_CHANNEL 1

static uint8_t broadcast_addr[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
    char message[32];
} data_packet_t;

static data_packet_t my_data;

/* ── Wi-Fi init (exact copy of official example) ──────────── */
void esp_wifi_initialization(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

/* ── Send callback ─────────────────────────────────────────── */
static void on_data_sent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "Sent to " MACSTR " — %s",
             MAC2STR(tx_info->des_addr),
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

/* ── Receive callback ─────────────────────────────────────── */
static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    ESP_LOGI(TAG, ">>> RAW RECV from " MACSTR " len=%d", MAC2STR(info->src_addr), len);

    if (data == NULL || len < (int)sizeof(data_packet_t)) {
        return;
    }

    const data_packet_t *pkt = (const data_packet_t *)data;
    ESP_LOGI(TAG, "Recv: %s", pkt->message);
}

/* ── ESP-NOW init (exact copy of official example) ────────── */
static void init_espnow(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // PMK — required by official example
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)"pmk1234567890123"));

    // Broadcast peer — channel MUST be explicit, NOT 0
    esp_now_peer_info_t broadcast_peer = {0};
    memcpy(broadcast_peer.peer_addr, broadcast_addr, ESP_NOW_ETH_ALEN);
    broadcast_peer.channel = WIFI_CHANNEL;   // ← WAS 0, MUST BE 1
    broadcast_peer.ifidx   = WIFI_IF_STA;
    broadcast_peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&broadcast_peer));
}

/* ── Main entry ───────────────────────────────────────────── */
void esp_now_initialization(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_wifi_initialization();
    init_espnow();

    uint8_t my_mac[ESP_NOW_ETH_ALEN];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    ESP_LOGI(TAG, "My MAC: " MACSTR, MAC2STR(my_mac));

    // 5-second delay (matches official example)
    ESP_LOGI(TAG, "Waiting 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "ESP-NOW ready. Broadcasting BEACON every second.");

    strcpy(my_data.message, "BEACON");

    while (1) {
        esp_err_t result = esp_now_send(broadcast_addr,
                                        (uint8_t *)&my_data,
                                        sizeof(data_packet_t));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Broadcast failed: 0x%x", result);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
