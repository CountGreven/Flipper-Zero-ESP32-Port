#include "furi_hal_ble_scan.h"

#include "sdkconfig.h"

#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BT_BLUEDROID_ENABLED)

#include <furi.h>
#include <string.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>

#define TAG "FuriHalBleScan"
#define BLE_SCAN_MAX 48

typedef struct {
    FuriHalBleDevice dev[BLE_SCAN_MAX];
    int count;
    FuriMutex* lock;
    bool active;
} BleScanState;

static BleScanState s_ble = {0};

static esp_ble_scan_params_t s_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50, /* 50 ms */
    .scan_window = 0x30, /* 30 ms */
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

/* Upsert a device (dedup by address, refresh RSSI/name/time). */
static void ble_upsert(const uint8_t* addr, int8_t rssi, const char* name) {
    furi_mutex_acquire(s_ble.lock, FuriWaitForever);
    int idx = -1;
    for(int i = 0; i < s_ble.count; i++) {
        if(memcmp(s_ble.dev[i].addr, addr, 6) == 0) {
            idx = i;
            break;
        }
    }
    if(idx < 0 && s_ble.count < BLE_SCAN_MAX) idx = s_ble.count++;
    if(idx >= 0) {
        FuriHalBleDevice* d = &s_ble.dev[idx];
        memcpy(d->addr, addr, 6);
        d->rssi = rssi;
        d->last_seen_ms = furi_get_tick();
        if(name && name[0]) {
            strncpy(d->name, name, sizeof(d->name) - 1);
            d->name[sizeof(d->name) - 1] = 0;
        }
    }
    furi_mutex_release(s_ble.lock);
}

static void ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch(event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(0); /* 0 = scan until stopped */
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        struct ble_scan_result_evt_param* r = &param->scan_rst;
        if(r->search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            uint8_t nlen = 0;
            uint8_t* n = esp_ble_resolve_adv_data(
                r->ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &nlen);
            if(!n) n = esp_ble_resolve_adv_data(r->ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &nlen);
            char name[24] = {0};
            if(n && nlen) {
                if(nlen > sizeof(name) - 1) nlen = sizeof(name) - 1;
                memcpy(name, n, nlen);
            }
            ble_upsert(r->bda, r->rssi, name);
        } else if(r->search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
            if(s_ble.active) esp_ble_gap_start_scanning(0); /* restart if it ended */
        }
        break;
    }
    default:
        break;
    }
}

static bool ble_stack_up(void) {
    esp_err_t e;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    e = esp_bt_controller_init(&bt_cfg);
    if(e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;
    e = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if(e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;
    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    e = esp_bluedroid_init_with_cfg(&bd_cfg);
    if(e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;
    e = esp_bluedroid_enable();
    if(e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;
    return true;
}

bool furi_hal_ble_scan_start(void) {
    if(s_ble.active) return true;
    if(!s_ble.lock) s_ble.lock = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!ble_stack_up()) {
        FURI_LOG_E(TAG, "BLE stack unavailable");
        return false;
    }
    if(esp_ble_gap_register_callback(ble_gap_cb) != ESP_OK) return false;
    s_ble.active = true;
    /* Setting params triggers PARAM_SET_COMPLETE which starts the scan. */
    if(esp_ble_gap_set_scan_params(&s_scan_params) != ESP_OK) {
        s_ble.active = false;
        return false;
    }
    return true;
}

void furi_hal_ble_scan_stop(void) {
    if(!s_ble.active) return;
    s_ble.active = false;
    esp_ble_gap_stop_scanning();
}

int furi_hal_ble_scan_get(FuriHalBleDevice* out, int max) {
    if(!s_ble.lock || !out) return 0;
    furi_mutex_acquire(s_ble.lock, FuriWaitForever);
    int n = s_ble.count < max ? s_ble.count : max;
    memcpy(out, s_ble.dev, (size_t)n * sizeof(FuriHalBleDevice));
    furi_mutex_release(s_ble.lock);
    return n;
}

void furi_hal_ble_scan_clear(void) {
    if(!s_ble.lock) return;
    furi_mutex_acquire(s_ble.lock, FuriWaitForever);
    s_ble.count = 0;
    furi_mutex_release(s_ble.lock);
}

#else /* BT not enabled */

bool furi_hal_ble_scan_start(void) {
    return false;
}
void furi_hal_ble_scan_stop(void) {
}
int furi_hal_ble_scan_get(FuriHalBleDevice* out, int max) {
    (void)out;
    (void)max;
    return 0;
}
void furi_hal_ble_scan_clear(void) {
}

#endif
