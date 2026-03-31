#pragma once

// ── TinyUSB device-mode configuration ────────────────────────────────────────
// We only need Mass Storage (MSC). No CDC serial, no HID.


#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             1
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

// MSC buffer size
#define CFG_TUD_MSC_EP_BUFSIZE  512
