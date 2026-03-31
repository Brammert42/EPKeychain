/*
 * bip32.h / bip32.cpp
 * BIP32 public-key-only child derivation (non-hardened)
 * Uses only secp256k1 point addition and HMAC-SHA512.
 * NO private key material ever touches this device.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * Decode a Base58Check xpub/ypub/zpub string into raw 33-byte compressed
 * public key and 32-byte chain code.
 *
 * Returns true on success.
 */
bool bip32DecodeXpub(const char* xpubStr,
                     uint8_t outPubKey[33],
                     uint8_t outChainCode[32]);

/*
 * Derive a non-hardened child public key.
 *
 *   parentPub[33]   – compressed parent public key
 *   parentChain[32] – parent chain code
 *   index           – child index (must be < 0x80000000)
 *   childPub[33]    – output: compressed child public key
 *   childChain[32]  – output: child chain code
 *
 * Returns true on success.
 */
bool bip32DerivePublic(const uint8_t parentPub[33],
                       const uint8_t parentChain[32],
                       uint32_t index,
                       uint8_t childPub[33],
                       uint8_t childChain[32]);
