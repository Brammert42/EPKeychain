/*
 * btcaddr.h / btcaddr.cpp
 * Bitcoin address generation from a 33-byte compressed public key.
 * Supports:
 *   BIP44  – Legacy P2PKH       (1...)
 *   BIP84  – Native SegWit P2WPKH (bc1q...)
 *   BIP86  – Taproot P2TR        (bc1p...)
 */

#pragma once
#include <stdint.h>

// Legacy P2PKH address  (BIP44, type=44)
void btcLegacyAddress(const uint8_t pubkey[33], char* out);

// Native SegWit P2WPKH bech32 (BIP84, type=84)
void btcSegwitAddress(const uint8_t pubkey[33], char* out);

// Taproot P2TR bech32m (BIP86, type=86)
void btcTaprootAddress(const uint8_t pubkey[33], char* out);
