/**
 * @file msc_cdrom.h
 * CD-ROM (MMC) SCSI response builders for USB optical-drive emulation. TinyUSB
 * handles INQUIRY / READ CAPACITY / READ10 through their own callbacks; the
 * commands a host still needs to mount an optical medium (READ TOC, GET
 * CONFIGURATION, GET EVENT STATUS, READ HEADER) are not built in and arrive via
 * tud_msc_scsi_cb, which forwards them here.
 *
 * Pure byte-layout builders with no hardware or firmware dependency, unit tested
 * on the host against the Linux usb-mass-storage gadget layouts.
 */
#pragma once

#include <stdint.h>

/** Set the CD-ROM peripheral device type (0x05) and identity into a TinyUSB
 * scsi_inquiry_resp_t-shaped buffer's first bytes. `resp` points at the 36-byte
 * standard inquiry response; product is 16 chars (space padded by the caller is
 * not required, this copies exactly 16). */
void msc_cdrom_inquiry(uint8_t* resp, const char vendor[8], const char product[16]);

/** Handle an MMC command that reached tud_msc_scsi_cb. Returns the response
 * length written to out (<= cap), 0 for a command that needs no data, or -1 if
 * this command is not one we emulate (caller should stall). block_count is the
 * total number of 2048-byte sectors in the ISO. */
int32_t msc_cdrom_scsi(
    const uint8_t cmd[16],
    uint8_t* out,
    uint32_t cap,
    uint32_t block_count);
