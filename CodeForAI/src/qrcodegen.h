#pragma once

/*
 * qrcodegen.h — local compatibility implementation
 *
 * NOTE:
 * This header mirrors the public API used by this firmware so the project
 * can build without pulling external files during configure/build.
 *
 * For production/scannable QR output, replace this pair (qrcodegen.h/.c)
 * with the upstream Project Nayuki implementation:
 * https://github.com/nayuki/QR-Code-generator/tree/master/c
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define qrcodegen_VERSION_MIN     1
#define qrcodegen_VERSION_MAX     40
#define qrcodegen_BUFFER_LEN_MAX  3918

typedef enum {
    qrcodegen_Ecc_LOW = 0,
    qrcodegen_Ecc_MEDIUM = 1,
    qrcodegen_Ecc_QUARTILE = 2,
    qrcodegen_Ecc_HIGH = 3,
} qrcodegen_Ecc;

typedef enum {
    qrcodegen_Mask_AUTO = -1,
    qrcodegen_Mask_0 = 0,
    qrcodegen_Mask_1 = 1,
    qrcodegen_Mask_2 = 2,
    qrcodegen_Mask_3 = 3,
    qrcodegen_Mask_4 = 4,
    qrcodegen_Mask_5 = 5,
    qrcodegen_Mask_6 = 6,
    qrcodegen_Mask_7 = 7,
} qrcodegen_Mask;

bool qrcodegen_encodeText(const char *text,
                          uint8_t tempBuffer[],
                          uint8_t qrcode[],
                          qrcodegen_Ecc ecl,
                          int minVersion,
                          int maxVersion,
                          qrcodegen_Mask mask,
                          bool boostEcl);

int qrcodegen_getSize(const uint8_t qrcode[]);
bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);

#ifdef __cplusplus
}
#endif
