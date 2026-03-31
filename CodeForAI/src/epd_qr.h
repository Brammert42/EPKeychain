/*
 * epd_qr.h / epd_qr.cpp
 * Renders a qrcodegen QR code maximally into a 200x200 1bpp EPD framebuffer.
 * White background, black modules, centered with quiet zone.
 */

#pragma once
#include <stdint.h>
#include "qrcodegen.h"

/*
 * Render a QR code buffer (from qrcodegen_encodeText) into an EPD 200x200
 * 1bpp framebuffer (1 = white, 0 = black, MSB first, row-major).
 *
 *   qrcode   – buffer returned by qrcodegen_encodeText
 *   epd_buf  – output 200*200/8 = 5000 byte buffer
 *   w, h     – display dimensions (200, 200)
 */
void renderQRToEPD(const uint8_t* qrcode, uint8_t* epd_buf, int w, int h);
