/**
 * @file furi_hal_ble_scan.h
 * Passive BLE observer: scan for nearby Bluetooth LE devices and report each
 * one's address, name and signal strength (RSSI). Backs a "BLE radar" style
 * app. Brings up the Bluetooth controller + Bluedroid on demand (idempotent)
 * and registers a GAP callback; this takes over the GAP callback, so it is not
 * meant to run alongside the BLE HID / serial features.
 *
 * NOTE: untested on hardware yet. start() returns false if the stack cannot be
 * brought up, so callers degrade gracefully.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t addr[6];
    int8_t rssi;
    char name[24]; /* complete/short local name, empty if none */
    uint32_t last_seen_ms; /* furi tick of the last advertisement */
} FuriHalBleDevice;

/** Start continuous BLE scanning. Returns false if the BLE stack is unavailable. */
bool furi_hal_ble_scan_start(void);

/** Stop scanning (leaves the controller initialized). */
void furi_hal_ble_scan_stop(void);

/** Snapshot the devices seen so far into out[] (up to max). Returns the count. */
int furi_hal_ble_scan_get(FuriHalBleDevice* out, int max);

/** Forget all seen devices. */
void furi_hal_ble_scan_clear(void);

#ifdef __cplusplus
}
#endif
