/*
 * qrcodegen.h  — STUB
 *
 * This project uses the QR Code generator library by Project Nayuki.
 * License: MIT
 * Source:  https://github.com/nayuki/QR-Code-generator/tree/master/c
 *
 * SETUP INSTRUCTIONS:
 * -------------------
 * 1. Download these two files from the link above:
 *      qrcodegen.h
 *      qrcodegen.c
 * 2. Place them in this sketch folder alongside BTCKeyChain.ino.
 * 3. Delete this stub file.
 *
 * The library is ~500 lines of pure C with no dependencies.
 * It compiles cleanly on RP2040 / ARM Cortex-M0+.
 *
 * Functions used by this project:
 *   qrcodegen_encodeText()
 *   qrcodegen_getSize()
 *   qrcodegen_getModule()
 *   qrcodegen_BUFFER_LEN_MAX
 *   qrcodegen_VERSION_MIN / _MAX
 *   qrcodegen_Ecc_MEDIUM
 *   qrcodegen_Mask_AUTO
 */

#pragma once
// The real header will define these. Shown here for reference only:
//
// #define qrcodegen_BUFFER_LEN_MAX  3918
// #define qrcodegen_VERSION_MIN     1
// #define qrcodegen_VERSION_MAX     40
//
// typedef enum { qrcodegen_Ecc_LOW, qrcodegen_Ecc_MEDIUM, ... } qrcodegen_Ecc;
// typedef enum { qrcodegen_Mask_AUTO = -1, ... } qrcodegen_Mask;
//
// bool qrcodegen_encodeText(const char *text,
//     uint8_t tempBuffer[], uint8_t qrcode[],
//     qrcodegen_Ecc ecl, int minVersion, int maxVersion,
//     qrcodegen_Mask mask, bool boostEcl);
//
// int  qrcodegen_getSize(const uint8_t qrcode[]);
// bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);
