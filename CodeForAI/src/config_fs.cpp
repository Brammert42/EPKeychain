/*
 * config_fs.cpp — FAT12 RAM disk + USB MSC for BTC KeyChain (Pico SDK 2.x)
 *
 * In Pico SDK 2.x, when tinyusb_device is linked, the USB stack is
 * initialized automatically by the SDK. We do NOT call tusb_init() or
 * tud_task() manually. The SDK handles this on the background.
 *
 * VBUS detection: read USB_SIE_STATUS bit 19 before any USB activity.
 * If no VBUS → skip USB entirely and run in battery mode.
 */

#include "config_fs.h"
#include "bip32.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "tusb.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── FAT12 geometry ─────────────────────────────────────────────────────────
#define SECTOR_SIZE       512
#define SECTOR_COUNT      256
#define RESERVED_SECTORS  1
#define FAT_SECTORS       1
#define FAT_COPIES        2
#define ROOT_ENTRIES      16
#define DATA_START        4
#define CLUSTER_SIZE      1
#define TOTAL_DISK        (SECTOR_COUNT * SECTOR_SIZE)

// ── Flash storage ───────────────────────────────────────────────────────────
#define FLASH_TARGET_OFFSET  (PICO_FLASH_SIZE_BYTES - 256 * 1024)
#define DISK_VERSION         4
#define FLASH_STORE_SIZE     (((1 + TOTAL_DISK + FLASH_SECTOR_SIZE - 1) \
                               / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE)

static uint8_t disk[TOTAL_DISK];
static volatile bool  disk_dirty = false;
static absolute_time_t last_write_time;

// ── FAT12 helpers ───────────────────────────────────────────────────────────
static void fat12_set_cluster(uint8_t* fat, int cluster, uint16_t value) {
    int idx = cluster + (cluster / 2);
    if (cluster & 1) {
        fat[idx]     = (fat[idx] & 0x0F) | ((value & 0x0F) << 4);
        fat[idx + 1] = (value >> 4) & 0xFF;
    } else {
        fat[idx]     = value & 0xFF;
        fat[idx + 1] = (fat[idx + 1] & 0xF0) | ((value >> 8) & 0x0F);
    }
}

static void make_dir_entry(uint8_t* e, const char* name83,
                            uint16_t startCluster, uint32_t fileSize) {
    memcpy(e, name83, 11);
    e[11] = 0x20;
    memset(e + 12, 0, 20);
    e[26] = startCluster & 0xFF;
    e[27] = (startCluster >> 8) & 0xFF;
    e[28] = fileSize & 0xFF;
    e[29] = (fileSize >> 8) & 0xFF;
    e[30] = (fileSize >> 16) & 0xFF;
    e[31] = (fileSize >> 24) & 0xFF;
}

static const char CONFIG_TEMPLATE[] =
    "# BTC KeyChain Configuration\r\n"
    "# Paste your account xpub/zpub below.\r\n"
    "# NEVER paste a private key here.\r\n"
    "#\r\n"
    "xpub=PASTE_YOUR_XPUB_HERE\r\n"
    "\r\n"
    "# 44=Legacy  84=NativeSegWit  86=Taproot\r\n"
    "type=84\r\n"
    "\r\n"
    "# Account index (usually 0)\r\n"
    "account=0\r\n";

static void build_fat12_image(void) {
    memset(disk, 0, TOTAL_DISK);
    uint8_t* b = disk;
    b[0]=0xEB; b[1]=0x3C; b[2]=0x90;
    memcpy(b+3, "MSDOS5.0", 8);
    b[11]=SECTOR_SIZE&0xFF; b[12]=SECTOR_SIZE>>8;
    b[13]=CLUSTER_SIZE;
    b[14]=RESERVED_SECTORS;
    b[16]=FAT_COPIES;
    b[17]=ROOT_ENTRIES;
    b[19]=SECTOR_COUNT&0xFF; b[20]=SECTOR_COUNT>>8;
    b[21]=0xF8; b[22]=FAT_SECTORS;
    b[24]=1; b[26]=1;
    b[36]=0x80; b[38]=0x29;
    b[39]=0x01; b[40]=0x02; b[41]=0x03; b[42]=0x04;
    memcpy(b+43, "BTCKEY     ", 11);
    memcpy(b+54, "FAT12   ", 8);
    b[510]=0x55; b[511]=0xAA;

    uint8_t* fat1 = disk + SECTOR_SIZE;
    fat1[0]=0xF8; fat1[1]=0xFF; fat1[2]=0xFF;
    fat12_set_cluster(fat1, 2, 0xFFF);
    memcpy(disk + SECTOR_SIZE * 2, fat1, SECTOR_SIZE);

    uint8_t* root = disk + SECTOR_SIZE * 3;
    uint32_t cfgLen = strlen(CONFIG_TEMPLATE);
    make_dir_entry(root, "CONFIG  TXT", 2, cfgLen);
    memcpy(disk + SECTOR_SIZE * DATA_START, CONFIG_TEMPLATE, cfgLen);
}

// ── Flash persistence ───────────────────────────────────────────────────────
static void save_disk_to_flash(void) {
    static uint8_t pagebuf[FLASH_STORE_SIZE];
    memset(pagebuf, 0xFF, sizeof(pagebuf));
    pagebuf[0] = DISK_VERSION;
    memcpy(pagebuf + 1, disk, TOTAL_DISK);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_STORE_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, pagebuf, FLASH_STORE_SIZE);
    restore_interrupts(ints);
}

static bool load_disk_from_flash(void) {
    const uint8_t* p = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (p[0] != DISK_VERSION) return false;
    memcpy(disk, p + 1, TOTAL_DISK);
    return true;
}

// ── VBUS detection ──────────────────────────────────────────────────────────
static bool usb_host_present(void) {
    volatile uint32_t* sie_status = (volatile uint32_t*)0x50110050u;
    return (*sie_status & (1u << 19)) != 0;
}

// ── TinyUSB MSC callbacks ───────────────────────────────────────────────────
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void)lun;
    *block_count = SECTOR_COUNT;
    *block_size  = SECTOR_SIZE;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id,   "BTCKEY  ", 8);
    memcpy(product_id,  "Config Drive    ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return true;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba,
                           uint32_t offset, void* buffer, uint32_t bufsize) {
    (void)lun; (void)offset;
    uint32_t off = lba * SECTOR_SIZE;
    if (off + bufsize > TOTAL_DISK) return -1;
    memcpy(buffer, disk + off, bufsize);
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba,
                            uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void)lun; (void)offset;
    uint32_t off = lba * SECTOR_SIZE;
    if (off + bufsize > TOTAL_DISK) return -1;
    memcpy(disk + off, buffer, bufsize);
    disk_dirty = true;
    last_write_time = get_absolute_time();
    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                         void* buffer, uint16_t bufsize) {
    (void)lun; (void)buffer; (void)bufsize;
    if (scsi_cmd[0] == 0x35 && disk_dirty) {
        disk_dirty = false;
        save_disk_to_flash();
    }
    return -1;
}

// ── Public API ──────────────────────────────────────────────────────────────
bool configFSBegin(void) {
    if (!load_disk_from_flash()) {
        build_fat12_image();
        save_disk_to_flash();
    }

    // Battery mode: no USB host, skip everything
    if (!usb_host_present()) {
        return false;
    }

    // USB host present — the Pico SDK 2.x inits TinyUSB automatically
    // when tinyusb_device is linked. Just wait for enumeration.
    absolute_time_t deadline = make_timeout_time_ms(2000);
    while (!tud_mounted() && !time_reached(deadline)) {
        tud_task();
        sleep_ms(10);
    }
    return tud_mounted();
}

void configPeriodicSave(void) {
    tud_task();
    if (disk_dirty) {
        int64_t elapsed = absolute_time_diff_us(last_write_time, get_absolute_time());
        if (elapsed >= 500000) {
            disk_dirty = false;
            save_disk_to_flash();
        }
    }
}

// ── Config parser ───────────────────────────────────────────────────────────
static void trim(char* s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1]=='\r'||s[len-1]=='\n'||s[len-1]==' ')) s[--len]=0;
    int st = 0;
    while (s[st]==' ') st++;
    if (st > 0) memmove(s, s+st, strlen(s)-st+1);
}

bool configLoad(WalletConfig* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->addrType = 84;
    cfg->account  = 0;

    uint8_t* root = disk + SECTOR_SIZE * 3;
    uint16_t startCluster = 0;
    uint32_t fileSize = 0;

    for (int i = 0; i < ROOT_ENTRIES; i++) {
        uint8_t* e = root + i * 32;
        if (e[0] == 0x00) break;
        if (e[0] == 0xE5 || (e[11] & 0x08)) continue;
        if (memcmp(e, "CONFIG  TXT", 11) == 0) {
            startCluster = e[26] | ((uint16_t)e[27] << 8);
            fileSize = e[28] | ((uint32_t)e[29]<<8) |
                       ((uint32_t)e[30]<<16) | ((uint32_t)e[31]<<24);
            break;
        }
    }

    if (startCluster < 2 || fileSize == 0) return false;

    uint32_t dataOff = SECTOR_SIZE * (DATA_START + (startCluster - 2) * CLUSTER_SIZE);
    if (dataOff + fileSize > TOTAL_DISK) return false;

    char* content = (char*)(disk + dataOff);
    char line[200];
    int li = 0;
    bool hasXpub = false;

    for (uint32_t i = 0; i <= fileSize; i++) {
        char c = (i < fileSize) ? content[i] : '\n';
        if (c == '\n' || c == '\r') {
            if (li > 0) {
                line[li] = 0;
                trim(line);
                if (line[0] != '#' && line[0] != 0) {
                    char* eq = strchr(line, '=');
                    if (eq) {
                        *eq = 0;
                        char* key = line;
                        char* val = eq + 1;
                        trim(key); trim(val);
                        if (!strcmp(key,"xpub")||!strcmp(key,"ypub")||!strcmp(key,"zpub")) {
                            strncpy(cfg->xpubStr, val, sizeof(cfg->xpubStr)-1);
                            hasXpub = true;
                        } else if (!strcmp(key,"type")) {
                            cfg->addrType = atoi(val);
                            if (cfg->addrType!=44&&cfg->addrType!=84&&cfg->addrType!=86)
                                cfg->addrType = 84;
                        } else if (!strcmp(key,"account")) {
                            cfg->account = (uint32_t)atoi(val);
                        }
                    }
                }
                li = 0;
            }
        } else {
            if (li < (int)sizeof(line)-1) line[li++] = c;
        }
    }

    if (!hasXpub || strlen(cfg->xpubStr) < 50) return false;
    if (strstr(cfg->xpubStr, "PASTE_YOUR")) return false;
    if (!bip32DecodeXpub(cfg->xpubStr, cfg->xpubKey, cfg->xpubChain)) return false;
    cfg->valid = true;
    return true;
}
