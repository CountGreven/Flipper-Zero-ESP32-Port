#include "msc_cdrom.h"
#include <stdbool.h>
#include <string.h>

#define SCSI_READ_HEADER       0x44
#define SCSI_READ_TOC          0x43
#define SCSI_GET_CONFIGURATION 0x46
#define SCSI_GET_EVENT_STATUS  0x4A

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v & 0xFF;
}

/* LBA -> MSF into a 4-byte TOC address field [reserved,M,S,F], 150-frame lead-in. */
static void put_msf(uint8_t* p, uint32_t lba) {
    uint32_t total = lba + 150;
    p[0] = 0;
    p[1] = (uint8_t)(total / (75 * 60));
    p[2] = (uint8_t)((total / 75) % 60);
    p[3] = (uint8_t)(total % 75);
}

void msc_cdrom_inquiry(uint8_t* resp, const char vendor[8], const char product[16]) {
    resp[0] = 0x05; // peripheral device type: CD-ROM
    resp[1] = 0x80; // removable
    resp[2] = 0x02; // version
    resp[3] = 0x02; // response data format
    resp[4] = 31; // additional length
    memcpy(resp + 8, vendor, 8);
    memcpy(resp + 16, product, 16);
    memcpy(resp + 32, "1.0 ", 4);
}

static int32_t read_toc(const uint8_t cmd[16], uint8_t* out, uint32_t cap, uint32_t block_count) {
    bool msf = (cmd[1] >> 1) & 1;
    uint8_t format = cmd[2] & 0x0F;
    uint16_t alloc = (cmd[7] << 8) | cmd[8];
    if(format == 2) {
        /* Raw TOC: session header + A0/A1/A2 + track 1, 11 bytes each (macOS). */
        if(cap < 48) return -1;
        memset(out, 0, 48);
        out[2] = 1; // first session
        out[3] = 1; // last session
        uint8_t* d = &out[4];
        d[0] = 1;
        d[1] = 0x16;
        d[3] = 0xA0;
        d[8] = 1; // first track
        d += 11;
        d[0] = 1;
        d[1] = 0x16;
        d[3] = 0xA1;
        d[8] = 1; // last track
        d += 11;
        d[0] = 1;
        d[1] = 0x16;
        d[3] = 0xA2;
        put_msf(&d[7], block_count); // lead-out
        d += 11;
        d[0] = 1;
        d[1] = 0x16;
        d[3] = 1;
        put_msf(&d[7], 0); // track 1 start
        out[0] = 0;
        out[1] = 46;
        int32_t len = 48;
        if(alloc && len > alloc) len = alloc;
        return len;
    }
    /* Format 0/1: track 1 descriptor + lead-out (Linux + Windows). */
    if(cap < 20) return -1;
    memset(out, 0, 20);
    out[2] = 1;
    out[3] = 1;
    out[5] = 0x16; // data track, copy allowed
    out[6] = 1;
    out[13] = 0x16;
    out[14] = 0xAA; // lead-out
    if(msf) {
        put_msf(&out[8], 0);
        put_msf(&out[16], block_count);
    } else {
        put_be32(&out[8], 0);
        put_be32(&out[16], block_count);
    }
    out[0] = 0;
    out[1] = 18;
    int32_t len = 20;
    if(alloc && len > alloc) len = alloc;
    return len;
}

int32_t msc_cdrom_scsi(const uint8_t cmd[16], uint8_t* out, uint32_t cap, uint32_t block_count) {
    switch(cmd[0]) {
    case SCSI_READ_TOC:
        return read_toc(cmd, out, cap, block_count);

    case SCSI_GET_CONFIGURATION: {
        if(cap < 16) return -1;
        memset(out, 0, 16);
        out[7] = 0x08; // current profile: CD-ROM
        out[9] = 0x00; // feature: Profile List
        out[10] = 0x03; // persistent + current
        out[11] = 4; // additional length
        out[13] = 0x08; // profile CD-ROM
        out[14] = 0x01; // current
        put_be32(&out[0], 16 - 4);
        int32_t len = 16;
        uint16_t alloc = (cmd[7] << 8) | cmd[8];
        if(alloc && len > alloc) len = alloc;
        return len;
    }
    case SCSI_GET_EVENT_STATUS: {
        if(cap < 4) return -1;
        memset(out, 0, 4);
        out[1] = 0x02; // event data length
        out[2] = 0x80; // No Event Available
        return 4;
    }
    case SCSI_READ_HEADER: {
        if(cap < 8) return -1;
        bool msf = (cmd[1] >> 1) & 1;
        uint32_t lba = cmd[2] << 24 | cmd[3] << 16 | cmd[4] << 8 | cmd[5];
        memset(out, 0, 8);
        out[0] = 0x01; // mode 1
        if(msf)
            put_msf(&out[4], lba);
        else
            put_be32(&out[4], lba);
        return 8;
    }
    default:
        return -1; // not emulated; caller stalls
    }
}
