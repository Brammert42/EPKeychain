/*
 * main.cpp — BTC KeyChain firmware (Pico SDK)
 *
 * Hardware:
 *   EPD RST=9  DC=10  CS=11  BUSY=8  PWR=6
 *   SPI RX=0  SCK=2  TX=3
 *   Button A = GPIO13  (next address, active LOW)
 *   Button B = GPIO23  (reset to index 0, active LOW)
 *   SW3 = mechanical CR2032 power switch
 *
 * Build:
 *   mkdir build && cd build
 *   cmake .. -DPICO_SDK_PATH=/path/to/pico-sdk
 *   make -j4
 */

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/time.h"

#include "epd1in54_V2.h"
#include "epdpaint.h"
#include "bip32.h"
#include "btcaddr.h"
#include "qrcodegen.h"
#include "epd_qr.h"
#include "config_fs.h"

// Display color constants
#define COLORED   0
#define UNCOLORED 1

#include <string.h>
#include <stdio.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
#define BTN_A       13
#define BTN_B       23
#define HOLD_MS     800

// ── EEPROM-style index persistence in flash ───────────────────────────────────
// Store address index in the 4KB sector just below the disk image area.
// config_fs uses the last 256KB; we use the sector just before that.
#define INDEX_FLASH_OFFSET  (PICO_FLASH_SIZE_BYTES - 256*1024 - FLASH_SECTOR_SIZE)
#define MAGIC_COOKIE        0xB7C0FFEEu

static void save_index(uint32_t idx) {
    // Pack: [magic 4 bytes][index 4 bytes][0xFF padding...]
    static uint8_t buf[FLASH_SECTOR_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    buf[0] = (MAGIC_COOKIE >> 24) & 0xFF;
    buf[1] = (MAGIC_COOKIE >> 16) & 0xFF;
    buf[2] = (MAGIC_COOKIE >>  8) & 0xFF;
    buf[3] = (MAGIC_COOKIE      ) & 0xFF;
    buf[4] = (idx >> 24) & 0xFF;
    buf[5] = (idx >> 16) & 0xFF;
    buf[6] = (idx >>  8) & 0xFF;
    buf[7] = (idx      ) & 0xFF;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(INDEX_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(INDEX_FLASH_OFFSET, buf, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

static uint32_t load_index(void) {
    const uint8_t* p = (const uint8_t*)(XIP_BASE + INDEX_FLASH_OFFSET);
    uint32_t magic = ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
                     ((uint32_t)p[2]<<8) |((uint32_t)p[3]);
    if (magic != MAGIC_COOKIE) return 0;
    return ((uint32_t)p[4]<<24)|((uint32_t)p[5]<<16)|
           ((uint32_t)p[6]<<8) |((uint32_t)p[7]);
}

// ── Display helpers ───────────────────────────────────────────────────────────
static Epd epd;

static void show_text(const char* line1,
                      const char* line2 = nullptr,
                      const char* line3 = nullptr) {
    unsigned char buf[200*200/8];
    memset(buf, 0xFF, sizeof(buf));
    Paint paint(buf, 200, 200);
    paint.SetRotate(ROTATE_0);
    if (line1) paint.DrawStringAt(10, 10, line1, &Font16, COLORED);
    if (line2) paint.DrawStringAt(10, 40, line2, &Font12, COLORED);
    if (line3) paint.DrawStringAt(10, 60, line3, &Font12, COLORED);
    epd.Display(buf);
}

static void show_setup_screen(void) {
    unsigned char buf[200*200/8];
    memset(buf, 0xFF, sizeof(buf));
    Paint paint(buf, 200, 200);
    paint.SetRotate(ROTATE_0);
    paint.DrawStringAt(10,  10, "BTC KeyChain",    &Font16, COLORED);
    paint.DrawStringAt(10,  40, "No config found.", &Font12, COLORED);
    paint.DrawStringAt(10,  60, "Plug in USB and",  &Font12, COLORED);
    paint.DrawStringAt(10,  76, "edit config.txt",  &Font12, COLORED);
    paint.DrawStringAt(10, 100, "xpub=YOUR_KEY",    &Font12, COLORED);
    paint.DrawStringAt(10, 116, "type=84",           &Font12, COLORED);
    epd.Display(buf);
}

static void show_address(const char* address) {
    uint8_t qrBuf[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tmpBuf[qrcodegen_BUFFER_LEN_MAX];
    bool ok = qrcodegen_encodeText(
        address, tmpBuf, qrBuf,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO, true);
    if (!ok) return;
    unsigned char buf[200*200/8];
    renderQRToEPD(qrBuf, buf, 200, 200);
    epd.Display(buf);
}

// ── Address derivation ────────────────────────────────────────────────────────
static WalletConfig cfg;

static bool derive_and_display(uint32_t idx) {
    uint8_t key[33], chain[32];
    if (!bip32DerivePublic(cfg.xpubKey, cfg.xpubChain, 0,   key, chain)) return false;
    if (!bip32DerivePublic(key,          chain,          idx, key, chain)) return false;
    char addr[128] = {0};
    switch (cfg.addrType) {
        case 44: btcLegacyAddress(key, addr);  break;
        case 86: btcTaprootAddress(key, addr); break;
        default: btcSegwitAddress(key, addr);  break;
    }
    show_address(addr);
    return true;
}

// ── Button hold detection ─────────────────────────────────────────────────────
static bool btn_held(uint pin) {
    if (gpio_get(pin)) return false;   // not pressed (active LOW)
    absolute_time_t t = get_absolute_time();
    while (!gpio_get(pin)) {
        if (absolute_time_diff_us(t, get_absolute_time()) >= HOLD_MS * 1000)
            return true;
        sleep_ms(10);
    }
    return false;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void) {
    stdio_init_all();

    // Init SPI for e-ink
    spi_init(spi0, 2000000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(0, GPIO_FUNC_SPI);  // RX  (MISO, not used but claim it)
    gpio_set_function(2, GPIO_FUNC_SPI);  // SCK
    gpio_set_function(3, GPIO_FUNC_SPI);  // TX  (MOSI)

    // Init display — shows immediately on battery
    epd.LDirInit();
    epd.HDirInit();

    show_text("BOOT", "Checking USB...");

    // configFSBegin: loads disk from flash, checks VBUS,
    // starts USB only if host present. Safe on battery.
    bool usb_mode = configFSBegin();

    show_text("BOOT", usb_mode ? "USB ready" : "Battery mode",
              "Loading config...");

    // Load address index from flash
    uint32_t addr_index = load_index();

    // Load wallet config from disk image
    if (!configLoad(&cfg)) {
        show_setup_screen();
        // In USB mode: stay alive so the user can edit config.txt
        // In battery mode: nothing to show, sleep forever
        while (true) {
            if (usb_mode) configPeriodicSave();
            sleep_ms(100);
        }
    }

    // Show current address
    derive_and_display(addr_index);

    // Init buttons
    gpio_init(BTN_A); gpio_set_dir(BTN_A, GPIO_IN); gpio_pull_up(BTN_A);
    gpio_init(BTN_B); gpio_set_dir(BTN_B, GPIO_IN); gpio_pull_up(BTN_B);

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (true) {
        if (usb_mode) configPeriodicSave();

        if (btn_held(BTN_A)) {
            addr_index++;
            save_index(addr_index);
            derive_and_display(addr_index);
            sleep_ms(300);
        }

        if (btn_held(BTN_B)) {
            addr_index = 0;
            save_index(addr_index);
            derive_and_display(addr_index);
            sleep_ms(300);
        }

        sleep_ms(20);
    }
}
