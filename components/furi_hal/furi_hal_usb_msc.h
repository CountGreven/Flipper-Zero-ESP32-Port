#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB Mass Storage Class (MSC) — exposes the SD card as a USB-Stick to the
 * host PC. Sits on top of the TinyUSB Composite Device (HID + CDC + MSC)
 * defined in furi_hal_usb_tinyusb_composite.c.
 *
 * The composite descriptor always includes an MSC interface (the descriptor
 * cannot be reconfigured at runtime). When MSC is *not* started, the SCSI
 * callbacks reply "no medium" and the host sees the drive as offline.
 *
 * Concurrency: the caller MUST ensure no firmware code is accessing the SD
 * while MSC is active. Typical flow:
 *   1) storage_sd_unmount() — close all files, unmount FATFS in storage svc
 *   2) furi_hal_sd_release_fatfs() — release FATFS in HAL, keep card alive
 *   3) furi_hal_usb_msc_start()
 *   ... host reads/writes the card ...
 *   4) furi_hal_usb_msc_stop()
 *   5) storage_sd_mount() — re-mount FATFS
 */

/** Activate the MSC layer. SD card must already be initialized.
 * @return true if MSC is now exposing the SD to the host.
 */
bool furi_hal_usb_msc_start(void);

/** Deactivate the MSC layer. Host will see the drive go offline.
 * After this returns, the SD card may be re-mounted by the firmware.
 */
void furi_hal_usb_msc_stop(void);

/** @return true if MSC is currently active. */
bool furi_hal_usb_msc_is_active(void);

/** @return true if the host has sent SCSI Prevent Medium Removal — i.e. the
 * host considers the volume mounted and probably has dirty cache for it.
 * Apps should warn the user to eject from the host before stopping MSC. */
bool furi_hal_usb_msc_is_removal_locked(void);

/* ─────────────────────────────────────────────────────────────────────
 * Virtual drives (iODD-style): present one or more image files from the SD
 * card to the host as independent USB LUNs — a .iso as a read-only CD-ROM, a
 * .img as a read-only or read/write disk — several at once. These reuse the
 * same MSC interface: when no virtual LUN is set the callbacks behave exactly
 * as the SD-card path above, so SD-over-USB is unaffected.
 * ───────────────────────────────────────────────────────────────────── */

#define FURI_HAL_USB_MSC_MAX_LUN 4

typedef struct {
    void* ctx;
    uint32_t block_count; /* number of logical blocks */
    uint16_t block_size; /* 512 for a disk image, 2048 for an ISO */
    bool cdrom; /* present as a read-only CD-ROM */
    bool writable; /* allow host writes (ignored when cdrom) */
    char product[16]; /* SCSI INQUIRY product id */
    int32_t (*read)(void* ctx, uint32_t lba, void* buf, uint32_t bufsize);
    int32_t (*write)(void* ctx, uint32_t lba, const uint8_t* buf, uint32_t bufsize);
} FuriHalUsbMscLun;

/** Register (or replace) a virtual-drive LUN, 0..FURI_HAL_USB_MSC_MAX_LUN-1.
 * Does not re-enumerate; set up every LUN then call furi_hal_usb_msc_present. */
bool furi_hal_usb_msc_lun_set(uint8_t lun, const FuriHalUsbMscLun* cfg);

/** Clear all virtual LUNs and restore the default single-LUN (SD) drive set. */
void furi_hal_usb_msc_lun_reset(void);

/** Announce `lun_count` drives to the host and force a USB re-enumeration so it
 * re-reads the drive set. Call after configuring the LUNs. */
void furi_hal_usb_msc_present(uint8_t lun_count);

#ifdef __cplusplus
}
#endif
