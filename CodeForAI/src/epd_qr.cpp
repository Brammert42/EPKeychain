/*
 * epd_qr.cpp
 */

#include "epd_qr.h"
#include <string.h>

void renderQRToEPD(const uint8_t* qrcode, uint8_t* epd_buf, int w, int h) {
    // Fill with white (0xFF = all bits set = white on this EPD)
    memset(epd_buf, 0xFF, w * h / 8);

    int qrSize = qrcodegen_getSize(qrcode);

    // Quiet zone: QR spec requires 4 modules. We add 2 extra for aesthetics.
    int quietZone = 4;

    // Scale: fit (qrSize + 2*quietZone) modules into min(w,h) pixels
    int available = (w < h ? w : h);
    int scale = available / (qrSize + 2 * quietZone);
    if (scale < 1) scale = 1;

    // Center the QR on the display
    int qrPixels = qrSize * scale;
    int totalPixels = qrPixels + 2 * quietZone * scale;
    int offsetX = (w - totalPixels) / 2 + quietZone * scale;
    int offsetY = (h - totalPixels) / 2 + quietZone * scale;

    for (int row = 0; row < qrSize; row++) {
        for (int col = 0; col < qrSize; col++) {
            if (qrcodegen_getModule(qrcode, col, row)) {
                // Black module — fill scale×scale pixel block
                for (int dy = 0; dy < scale; dy++) {
                    int py = offsetY + row * scale + dy;
                    if (py < 0 || py >= h) continue;
                    for (int dx = 0; dx < scale; dx++) {
                        int px = offsetX + col * scale + dx;
                        if (px < 0 || px >= w) continue;
                        // Clear bit = black
                        int byteIdx = py * (w / 8) + px / 8;
                        int bitIdx  = 7 - (px % 8);
                        epd_buf[byteIdx] &= ~(1 << bitIdx);
                    }
                }
            }
        }
    }
}
