#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nimble/ble.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"  // ADD THIS

static const char *TAG = "BLE_CLIENT";

static const char *TARGET_DEVICE_NAME = "ESP BLE";
static uint16_t conn_handle;
static uint8_t  g_own_addr_type = 0;
static uint16_t tx_handle = 0;
static uint16_t rx_handle = 0;

/* Stored target address — filled when discovered, consumed on DISC_COMPLETE */
static ble_addr_t target_addr;
static bool       target_found = false;

static void ble_client_scan(void);
static int  ble_client_gap_event(struct ble_gap_event *event, void *arg);

/* ---- GATT read callback ---- */
static int on_read_complete(uint16_t conn_h, const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr, void *arg) {
    if (error->status != 0) {
        ESP_LOGE(TAG, "Read error: %d", error->status);
        return error->status;
    }
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    ESP_LOGI(TAG, "Read %d bytes: %.*s", len, len, attr->om->om_data);
    return 0;
}

/* ---- GATT write callback ---- */
static int on_write_complete(uint16_t conn_h, const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr, void *arg) {
    if (error->status != 0) {
        ESP_LOGE(TAG, "Write error: %d", error->status);
        return error->status;
    }
    ESP_LOGI(TAG, "Write OK");
    return 0;
}

/* ---- Characteristic discovery callback ---- */
static int on_disc_chr(uint16_t conn_h, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0) {
        ESP_LOGI(TAG, "CHR found, val_handle=%d, props=0x%02x",
                 chr->val_handle, chr->properties);

        if (chr->properties & BLE_GATT_CHR_F_WRITE) {
            tx_handle = chr->val_handle;
            ESP_LOGI(TAG, "WRITE handle saved: %d", tx_handle);

            /* Send a test message */
            const char *msg = "Hello Server!";
            ble_gattc_write_no_rsp_flat(conn_handle, tx_handle,
                                 msg, strlen(msg));
        }

        if (chr->properties & BLE_GATT_CHR_F_READ) {
            rx_handle = chr->val_handle;
            ESP_LOGI(TAG, "READ handle saved: %d", rx_handle);
            /* Read the current value */
            ble_gattc_read(conn_handle, rx_handle, on_read_complete, NULL);
        }
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Discovery complete");
        return 0;
    }
    return error->status;
}

/* ---- GAP event handler ---- */
static int ble_client_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            struct ble_hs_adv_fields fields;
            if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                                        event->disc.length_data) != 0)
                break;

            if (fields.name && fields.name_len > 0) {
                ESP_LOGW(TAG, "Device: %.*s (%d)",
                         fields.name_len, fields.name, fields.name_len);

                if (fields.name_len == strlen(TARGET_DEVICE_NAME) &&
                    strncmp((char *)fields.name, TARGET_DEVICE_NAME,
                            fields.name_len) == 0) {

                    ESP_LOGI(TAG, "Target found! Canceling scan...");

                    /* Cancel scan */
                    int rc = ble_gap_disc_cancel();
                    if (rc != 0 && rc != BLE_HS_EALREADY) {
                        ESP_LOGE(TAG, "disc_cancel failed: %d", rc);
                        break;
                    }

                    

                    /* Connection parameters */
                    struct ble_gap_conn_params params = {
                        .scan_itvl         = 0x0010,
                        .scan_window       = 0x0010,
                        .itvl_min          = 0x0018,  // 30ms
                        .itvl_max          = 0x0028,  // 50ms
                        .latency           = 0,
                        .supervision_timeout = 0x0100,
                        .min_ce_len        = 0,
                        .max_ce_len        = 0,
                    };

                    rc = ble_gap_connect(g_own_addr_type, &event->disc.addr,
                                        30000, &params,
                                        ble_client_gap_event, NULL);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "Connect failed: %d", rc);
                        
                        ble_client_scan();
                    } else {
                        ESP_LOGI(TAG, "Connect initiated to %02x:%02x:%02x:%02x:%02x:%02x",
                                 event->disc.addr.val[0], event->disc.addr.val[1],
                                 event->disc.addr.val[2], event->disc.addr.val[3],
                                 event->disc.addr.val[4], event->disc.addr.val[5]);
                    }
                }
            }
            break;
        }

        case BLE_GAP_EVENT_CONNECT: {
            if (event->connect.status == 0) {
                conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "*** CONNECTED! *** handle=%d", conn_handle);
                
                int rc = ble_gattc_disc_all_chrs(conn_handle, 0x0001, 0xffff,
                                                on_disc_chr, NULL);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Characteristic discovery failed: %d", rc);
                }
            } else {
                ESP_LOGE(TAG, "Connection failed: %d", event->connect.status);
                
                ble_client_scan();
            }
            break;
        }

        case BLE_GAP_EVENT_DISCONNECT: {
            ESP_LOGI(TAG, "*** DISCONNECTED *** reason=%d", event->disconnect.reason);
            conn_handle = 0;
            tx_handle = 0;
            rx_handle = 0;
            
            vTaskDelay(pdMS_TO_TICKS(2000));
            ble_client_scan();
            break;
        }

        default:
            break;
    }
    return 0;
}


/* ---- Scanning ---- */
static void ble_client_scan(void) {
    struct ble_gap_disc_params params = {0};
    params.filter_duplicates = 1;
    params.passive           = 0;
    params.itvl              = 0x0060;
    params.window            = 0x0030;

    int rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "addr infer failed: %d", rc);
        return;
    }

    rc = ble_gap_disc(g_own_addr_type, BLE_HS_FOREVER, &params,
                      ble_client_gap_event, NULL);
    if (rc != 0)
        ESP_LOGE(TAG, "scan start failed: %d", rc);
}

/* ---- Sync callback ---- */
static void on_sync(void) {
    assert(ble_hs_util_ensure_addr(0) == 0);
    ESP_LOGI(TAG, "Synced. Starting scan...");
    ble_client_scan();
}

/* ---- Reset callback ---- */
static void on_reset(int reason) {  // ADD THIS FUNCTION
    ESP_LOGI(TAG, "BLE reset: %d", reason);
}

/* ---- Host task ---- */
static void host_task(void *param) {
    ESP_LOGI(TAG, "Host task running");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void client_initialization(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(nimble_port_init());
    
    ble_svc_gatt_init();  // ADD THIS LINE
    
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gap_device_name_set("ESP32_CLIENT");

    nimble_port_freertos_init(host_task);
}