/*
 * gatt_svc.c
 *
 *  Created on: Aug 31, 2026
 *      Author: ASUS
 */

#include "gatt_svc.h"
#include "esp_log.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "host/ble_esp_gatt.h"
#include "os/os_mbuf.h"
#include "services/gatt/ble_svc_gatt.h"

#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include <stdint.h>
#include <string.h> 

// Handlers 
static uint16_t pet_chr_val_handler;

// UUID 128bit for svc 
static const ble_uuid128_t ble_pet_svc_uuid = BLE_UUID128_INIT(
    0x60, 0xE7, 0x45, 0xCC, 0x61, 0xAA, 0x82, 0xB7, 0x38, 0x48, 0x4B, 0x72, 0xE3, 0x2A, 0x4C, 0x36
);

// UUID 128bits for chr 
static const ble_uuid128_t ble_pet_chr_uuid = BLE_UUID128_INIT(
    0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00
);

static const char *TAG = "BLE GATT"; 

static int pet_chr_access(uint16_t conn_handle, uint16_t att_handle, struct ble_gatt_access_ctxt *ctxt, void *args);
void gatt_svc_init(void);

// FIX STEP 1: Define characteristics outside the service array
static const struct ble_gatt_chr_def pet_chars[] = {
    {
        .uuid = &ble_pet_chr_uuid.u,
        .access_cb = pet_chr_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        //.val_handle = &pet_chr_val_handler
    },
    {0} // Explicit characteristic terminator
};

// FIX STEP 2: Reference the clean static array inside the service definition
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &ble_pet_svc_uuid.u,
        .characteristics = pet_chars, // Clean static pointer reference
    },
    {0} // Explicit service terminator
};

static int pet_chr_access(uint16_t conn_handle, uint16_t att_handle, struct ble_gatt_access_ctxt *ctxt, void *args) {
    int rc = 0;
    
    switch(ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "CHAR READ ESTABLISHED");
            const char *response = "Read successful!";
            rc = os_mbuf_append(ctxt->om, response, strlen(response));
            break;
            
        case BLE_GATT_ACCESS_OP_WRITE_CHR: { 
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            char recieved[len + 1];
            rc = os_mbuf_copydata(ctxt->om, 0, len, recieved);
            
            if (rc == 0) {
                recieved[len] = '\0'; 
                ESP_LOGI(TAG, "Data received: %s", recieved);
            }
            break;
        } 
    }
    return rc;
}

void gatt_svc_init(void) {
    ESP_LOGI(TAG, "GATT service started");
    int rc;
    
    // Initialize GATT service
    ble_svc_gatt_init();
    
    // Count the services
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT config %d", rc);
        return;
    }
    
    // Add the services
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to add GATT services %d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "GATT Services registered successfully!");
}
