#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     xpubStr[120];
    uint8_t  xpubKey[33];
    uint8_t  xpubChain[32];
    int      addrType;
    uint32_t account;
    bool     valid;
} WalletConfig;

// Returns true if USB host detected and MSC drive mounted.
// Returns false if running on battery — safe to call either way.
bool configFSBegin(void);

// Call from main loop — saves disk to flash ~500ms after last write.
void configPeriodicSave(void);

// Parse config.txt from RAM disk into cfg. Returns true if valid xpub found.
bool configLoad(WalletConfig* cfg);

#ifdef __cplusplus
}
#endif
