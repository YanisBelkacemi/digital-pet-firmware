#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs_adv.h"
#include "host/ble_hs_id.h"
#include "host/util/util.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <string.h>
#include "services/gap/ble_svc_gap.h"
#include "gatt_svc.h"
#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200

static const char *TAG = "BLE_CORE";

static uint8_t own_addr = 0;
static uint8_t addr_val[6] = {0};

void start_adv(void);
static int gap_init(void);
static void on_sync(void);
static void on_reset(int reason);
static void nimble_host_task(void *param);
static int gap_handler(struct ble_gap_event *event, void *args);

static int gap_init(void) {
    int rc;
    ble_svc_gap_init();

    rc = ble_svc_gap_device_name_set("ESP BLE");
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name: %d", rc);
        return rc;
    }

    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set appearance: %d", rc);
        return rc;
    }
    return 0;
}

static void on_sync(void) {
    /* Called when the host stack syncs with the controller.
       GATT/GAP services are already registered — now resolve
       our address and start advertising. */
    int rc = ble_hs_id_infer_auto(0, &own_addr);
    if (rc != 0) {
        ESP_LOGE(TAG, "Unable to infer addr: %d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(own_addr, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to copy address: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Addr type=%d, MAC=%02x:%02x:%02x:%02x:%02x:%02x",
             own_addr, addr_val[0], addr_val[1], addr_val[2],
             addr_val[3], addr_val[4], addr_val[5]);

    start_adv();
}

static void on_reset(int reason) {
    ESP_LOGI(TAG, "NimBLE stack reset, reason: %d", reason);
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void start_adv(void) {
    int rc;
    const char *name = "ESP BLE";

    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields: %d", rc);
        return;
    }

    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr;
    rsp_fields.device_addr_is_present = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set rsp fields: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start ADV: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "ADV started");
}

static int gap_handler(struct ble_gap_event *event, void *args) {
	ESP_LOGI(TAG , "Event recorded");
    switch (event->type) {
		
		
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "CONNECT %s; status=%d",
                     event->connect.status == 0 ? "ok" : "failed",
                     event->connect.status);

            if (event->connect.status != 0) {
                /* FIX: restart advertising on failed connection */
                start_adv();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "DISCONNECT; reason=%d", event->disconnect.reason);
            start_adv();
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "CONN UPDATE; status=%d", event->conn_update.status);
            break;

        default:
            break;
    }
    return 0;
}

void Ble_initilize(void) {
    int ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(nimble_port_init());

    /* Configure callbacks */
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;
	
    /* FIX: register GAP + GATT services BEFORE sync */
    gap_init();  
	gatt_svc_init();      /* ble_svc_gap_init() + name + appearance */
       /* ble_svc_gatt_init() + count + add_svcs */

    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
}
