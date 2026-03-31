#include "qrcodegen.h"

#include <stddef.h>
#include <string.h>

/*
 * Lightweight in-tree fallback encoder.
 *
 * This implementation is intentionally minimal and focuses on buildability in
 * restricted/offline environments. It produces a deterministic matrix that is
 * visually QR-like for display testing, but it is NOT standards-compliant.
 *
 * Replace with the upstream Project Nayuki implementation for real QR codes.
 */

#define QR_SIZE 29

static inline int data_offset(int x, int y) {
    int bit = y * QR_SIZE + x;
    return 1 + (bit >> 3);
}

static inline uint8_t data_mask(int x, int y) {
    int bit = y * QR_SIZE + x;
    return (uint8_t)(1u << (bit & 7));
}

static void set_module(uint8_t qrcode[], int x, int y, bool on) {
    int off = data_offset(x, y);
    uint8_t mask = data_mask(x, y);
    if (on) {
        qrcode[off] |= mask;
    } else {
        qrcode[off] &= (uint8_t)~mask;
    }
}

static bool get_module_internal(const uint8_t qrcode[], int x, int y) {
    int off = data_offset(x, y);
    uint8_t mask = data_mask(x, y);
    return (qrcode[off] & mask) != 0;
}

static void draw_finder(uint8_t qrcode[], int ox, int oy) {
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 7; ++x) {
            bool border = (x == 0 || x == 6 || y == 0 || y == 6);
            bool center = (x >= 2 && x <= 4 && y >= 2 && y <= 4);
            set_module(qrcode, ox + x, oy + y, border || center);
        }
    }
}

bool qrcodegen_encodeText(const char *text,
                          uint8_t tempBuffer[],
                          uint8_t qrcode[],
                          qrcodegen_Ecc ecl,
                          int minVersion,
                          int maxVersion,
                          qrcodegen_Mask mask,
                          bool boostEcl) {
    (void)tempBuffer;
    (void)ecl;
    (void)mask;
    (void)boostEcl;

    if (qrcode == NULL || text == NULL) {
        return false;
    }
    if (minVersion > qrcodegen_VERSION_MIN || maxVersion < qrcodegen_VERSION_MIN) {
        return false;
    }

    memset(qrcode, 0, qrcodegen_BUFFER_LEN_MAX);
    qrcode[0] = QR_SIZE;

    draw_finder(qrcode, 0, 0);
    draw_finder(qrcode, QR_SIZE - 7, 0);
    draw_finder(qrcode, 0, QR_SIZE - 7);

    uint32_t h = 0x811C9DC5u;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        h ^= (uint32_t)(*p);
        h *= 16777619u;
    }

    for (int y = 0; y < QR_SIZE; ++y) {
        for (int x = 0; x < QR_SIZE; ++x) {
            if ((x < 9 && y < 9) || (x >= QR_SIZE - 8 && y < 9) || (x < 9 && y >= QR_SIZE - 8)) {
                continue;
            }
            uint32_t v = h ^ (uint32_t)(x * 0x45d9f3bu) ^ (uint32_t)(y * 0x27d4eb2du);
            set_module(qrcode, x, y, ((v >> ((x + y) & 7)) & 1u) != 0u);
        }
    }

    return true;
}

int qrcodegen_getSize(const uint8_t qrcode[]) {
    if (qrcode == NULL) {
        return 0;
    }
    return (int)qrcode[0];
}

bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y) {
    if (qrcode == NULL) {
        return false;
    }
    int size = qrcodegen_getSize(qrcode);
    if (x < 0 || y < 0 || x >= size || y >= size) {
        return false;
    }
    return get_module_internal(qrcode, x, y);
}
