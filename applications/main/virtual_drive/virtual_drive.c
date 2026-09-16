/**
 * @file virtual_drive.c
 * iODD-style USB virtual drives. Stage one or more image files from the SD card
 * and present them to a host at once as independent USB drives:
 *   - a .iso as a read-only CD-ROM (optical drive), or
 *   - a .img as a read-only disk (a forensic toolkit the target can't alter) or
 *     a read/write disk (a data-exfil / rescue drive).
 *
 * The USB side is the port's TinyUSB MSC, extended with per-LUN virtual backings
 * (furi_hal_usb_msc_lun_set / _present). This app is the on-device selector.
 */

#include <furi.h>
#include <furi_hal_usb_msc.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>

typedef enum {
    ItemAddIso,
    ItemAddImgRo,
    ItemAddImgRw,
    ItemPresent,
    ItemEject,
} Item;

typedef struct {
    File* file;
    uint32_t block_size;
    uint32_t block_count;
    bool cdrom;
    bool writable;
    char name[24];
} Slot;

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    Storage* storage;
    ViewDispatcher* vd;
    Submenu* menu;
    Slot slots[FURI_HAL_USB_MSC_MAX_LUN];
    int nslots;
    bool presented;
} VdApp;

/* ---- file-backed LUN callbacks (called from the TinyUSB task) ---- */

static int32_t slot_read(void* ctx, uint32_t lba, void* buf, uint32_t bufsize) {
    Slot* s = ctx;
    if(!storage_file_seek(s->file, (uint64_t)lba * s->block_size, true)) return -1;
    uint32_t n = storage_file_read(s->file, buf, bufsize);
    return n == bufsize ? (int32_t)bufsize : -1;
}

static int32_t slot_write(void* ctx, uint32_t lba, const uint8_t* buf, uint32_t bufsize) {
    Slot* s = ctx;
    if(!storage_file_seek(s->file, (uint64_t)lba * s->block_size, true)) return -1;
    uint32_t n = storage_file_write(s->file, (void*)buf, bufsize);
    return n == bufsize ? (int32_t)bufsize : -1;
}

/* ---- actions ---- */

static bool vd_add(VdApp* a, Item mode) {
    if(a->nslots >= FURI_HAL_USB_MSC_MAX_LUN) return false;
    const char* ext = (mode == ItemAddIso) ? ".iso" : ".img";
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ext, NULL);
    opts.base_path = "/ext";

    FuriString* path = furi_string_alloc_set("/ext");
    bool ok = dialog_file_browser_show(a->dialogs, path, path, &opts);
    if(ok) {
        File* f = storage_file_alloc(a->storage);
        uint32_t access = (mode == ItemAddImgRw) ? (FSAM_READ | FSAM_WRITE) : FSAM_READ;
        if(storage_file_open(f, furi_string_get_cstr(path), access, FSOM_OPEN_EXISTING)) {
            Slot* s = &a->slots[a->nslots];
            s->file = f;
            s->cdrom = (mode == ItemAddIso);
            s->writable = (mode == ItemAddImgRw);
            s->block_size = s->cdrom ? 2048 : 512;
            s->block_count = (uint32_t)(storage_file_size(f) / s->block_size);
            FuriString* name = furi_string_alloc();
            path_extract_filename(path, name, true);
            snprintf(s->name, sizeof(s->name), "%s", furi_string_get_cstr(name));
            furi_string_free(name);
            a->nslots++;
        } else {
            storage_file_free(f);
            ok = false;
        }
    }
    furi_string_free(path);
    return ok;
}

static void vd_present(VdApp* a) {
    for(int i = 0; i < a->nslots; i++) {
        Slot* s = &a->slots[i];
        FuriHalUsbMscLun cfg = {
            .ctx = s,
            .block_count = s->block_count,
            .block_size = s->block_size,
            .cdrom = s->cdrom,
            .writable = s->writable,
            .read = slot_read,
            .write = slot_write,
        };
        memset(cfg.product, ' ', sizeof(cfg.product));
        size_t n = strlen(s->name);
        memcpy(cfg.product, s->name, n > 16 ? 16 : n);
        furi_hal_usb_msc_lun_set((uint8_t)i, &cfg);
    }
    furi_hal_usb_msc_present((uint8_t)a->nslots);
    a->presented = true;
}

static void vd_eject(VdApp* a) {
    furi_hal_usb_msc_lun_reset();
    if(a->presented) furi_hal_usb_msc_present(1);
    a->presented = false;
    for(int i = 0; i < a->nslots; i++) {
        storage_file_close(a->slots[i].file);
        storage_file_free(a->slots[i].file);
    }
    a->nslots = 0;
}

/* ---- UI ---- */
static void vd_menu_cb(void* context, uint32_t index);

static void vd_rebuild_menu(VdApp* a) {
    submenu_reset(a->menu);
    char hdr[40];
    if(a->nslots == 0)
        snprintf(hdr, sizeof(hdr), "Virtual Drive");
    else
        snprintf(
            hdr, sizeof(hdr), "%d drive%s %s", a->nslots, a->nslots == 1 ? "" : "s",
            a->presented ? "connected" : "staged");
    submenu_set_header(a->menu, hdr);

    if(a->nslots < FURI_HAL_USB_MSC_MAX_LUN) {
        submenu_add_item(a->menu, "Add CD-ROM (.iso)", ItemAddIso, vd_menu_cb, a);
        submenu_add_item(a->menu, "Add Drive RO (.img)", ItemAddImgRo, vd_menu_cb, a);
        submenu_add_item(a->menu, "Add Drive RW (.img)", ItemAddImgRw, vd_menu_cb, a);
    }
    if(a->nslots > 0 && !a->presented)
        submenu_add_item(a->menu, "Connect to host", ItemPresent, vd_menu_cb, a);
    if(a->nslots > 0) submenu_add_item(a->menu, "Eject all", ItemEject, vd_menu_cb, a);
}

static void vd_menu_cb(void* context, uint32_t index) {
    VdApp* a = context;
    switch((Item)index) {
    case ItemAddIso:
    case ItemAddImgRo:
    case ItemAddImgRw:
        if(!a->presented) vd_add(a, (Item)index);
        break;
    case ItemPresent:
        if(a->nslots > 0) vd_present(a);
        break;
    case ItemEject:
        vd_eject(a);
        break;
    }
    vd_rebuild_menu(a);
}

static bool vd_nav_cb(void* context) {
    UNUSED(context);
    return false; /* Back exits the app */
}

int32_t virtual_drive_app(void* p) {
    UNUSED(p);
    VdApp* a = malloc(sizeof(VdApp));
    memset(a, 0, sizeof(VdApp));
    a->gui = furi_record_open(RECORD_GUI);
    a->dialogs = furi_record_open(RECORD_DIALOGS);
    a->storage = furi_record_open(RECORD_STORAGE);

    a->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(a->vd, a);
    view_dispatcher_set_navigation_event_callback(a->vd, vd_nav_cb);

    a->menu = submenu_alloc();
    submenu_set_selected_item(a->menu, 0);
    view_dispatcher_add_view(a->vd, 0, submenu_get_view(a->menu));

    /* submenu callbacks are set per item via add_item; wire the shared handler. */
    vd_rebuild_menu(a);

    view_dispatcher_attach_to_gui(a->vd, a->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(a->vd, 0);
    view_dispatcher_run(a->vd);

    vd_eject(a);
    view_dispatcher_remove_view(a->vd, 0);
    submenu_free(a->menu);
    view_dispatcher_free(a->vd);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(a);
    return 0;
}
