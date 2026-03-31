# BTC KeyChain — Pico SDK Build Guide

## Prerequisites

### 1. Install the Pico SDK

```bash
# Linux / macOS
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk
git submodule update --init
export PICO_SDK_PATH=~/pico-sdk
```

Add the export to your `~/.bashrc` or `~/.zshrc` so it persists.

### 2. Install build tools

**Linux (Ubuntu/Debian):**
```bash
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```

**macOS:**
```bash
brew install cmake
brew install --cask gcc-arm-embedded
```

**Windows:**
Use the [Raspberry Pi Pico Windows Installer](https://github.com/raspberrypi/pico-setup-windows/releases)
which installs everything including CMake and the ARM toolchain.

---

## Project structure

```
btckeychain/
├── CMakeLists.txt
├── BUILD.md
├── src/
│   ├── main.cpp            — main loop, replaces BTCKeyChain.ino
│   ├── config_fs.cpp/h     — FAT12 disk, flash storage, USB MSC
│   ├── bip32.cpp/h         — BIP32 key derivation
│   ├── btcaddr.cpp/h       — address encoding (legacy/segwit/taproot)
│   ├── epd_qr.cpp/h        — QR → EPD framebuffer renderer
│   ├── qrcodegen.c/h       — Nayuki QR library (see below)
│   ├── tusb_config.h       — TinyUSB: MSC only, no CDC
│   └── usb_descriptors.c   — USB device/config/string descriptors
└── waveshare_epd/
    ├── epd1in54_V2.cpp/h   — Waveshare 1.54" e-ink driver
    ├── epdif.cpp/h         — SPI/GPIO interface (Pico SDK)
    ├── epdpaint.cpp/h      — paint/framebuffer library
    └── font*.c / fonts.h   — bitmap fonts
```

---

## One step before building — add the QR library

Download these two files from https://github.com/nayuki/QR-Code-generator/tree/master/c

- `qrcodegen.h`
- `qrcodegen.c`

**Replace** `src/qrcodegen.c` (the stub) and `src/qrcodegen.h` with the real files.

---

## Build

```bash
cd btckeychain
mkdir build && cd build
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
make -j4
```

Output: `build/btckeychain.uf2`

---

## Flash

1. Hold **BOOTSEL** on the Pico while plugging in USB
2. It mounts as a drive called **RPI-RP2**
3. Drag and drop `btckeychain.uf2` onto it
4. It reboots automatically

---

## Flash layout

The firmware uses raw flash for persistence (no filesystem library needed):

```
0x00000000  ┌─────────────────────────┐
            │   Sketch / firmware     │  up to ~1.75 MB
            ├─────────────────────────┤
0x001D8000  │   Address index         │  4 KB (1 flash sector)
            ├─────────────────────────┤
0x001E0000  │   FAT12 disk image      │  128 KB  (config.txt lives here)
            └─────────────────────────┘
0x00200000  (end of 2 MB flash)
```

---

## Key differences from Arduino version

| Arduino                        | Pico SDK                                  |
|-------------------------------|-------------------------------------------|
| `EEPROM.begin()` / `.write()` | `flash_range_program()` directly          |
| `LittleFS`                    | Raw flash — simpler, one less dependency  |
| `SPI.begin(true)`             | `spi_init(spi0, ...)` + `gpio_set_function` |
| `delay()`                     | `sleep_ms()`                              |
| `millis()`                    | `get_absolute_time()` / `absolute_time_diff_us()` |
| `digitalWrite()`              | `gpio_put()`                              |
| `digitalRead()`               | `gpio_get()`                              |
| `pinMode(INPUT_PULLUP)`       | `gpio_pull_up()`                          |
| USB stack starts before main  | `tusb_init()` called explicitly — you control when |

---

## Why battery works now

In the Arduino version, the Earle Philhower core starts the USB stack on core1
**before** `setup()` runs. There is no supported way to prevent this.

In the Pico SDK version:
1. The program starts, inits the display, shows "Checking USB..."
2. `configFSBegin()` reads bit 19 of `USB_SIE_STATUS` — a raw hardware register
3. If VBUS is not present (battery): skips `tusb_init()` entirely and returns immediately
4. If VBUS is present (USB): calls `tusb_init()`, waits for mount, makes drive visible

No USB stack activity ever occurs on battery boot.
