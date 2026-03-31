/*
 * bip32.cpp
 *
 * Implements:
 *  - Base58Check decode
 *  - HMAC-SHA512 (RFC 2104)
 *  - SHA-256, SHA-512 (bare-metal, no external lib)
 *  - secp256k1 point addition and scalar multiplication (for child key derivation)
 *  - bip32DecodeXpub()
 *  - bip32DerivePublic()
 */

#include "bip32.h"
#include <string.h>
// Arduino.h not needed on Pico SDK

// ═══════════════════════════════════════════════════════════════════════════════
// SHA-256
// ═══════════════════════════════════════════════════════════════════════════════

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32-n)); }
static inline uint32_t Ch(uint32_t e,uint32_t f,uint32_t g)  { return (e&f)^(~e&g); }
static inline uint32_t Maj(uint32_t a,uint32_t b,uint32_t c) { return (a&b)^(a&c)^(b&c); }
static inline uint32_t Sig0(uint32_t a) { return rotr32(a,2)^rotr32(a,13)^rotr32(a,22); }
static inline uint32_t Sig1(uint32_t e) { return rotr32(e,6)^rotr32(e,11)^rotr32(e,25); }
static inline uint32_t sig0(uint32_t x) { return rotr32(x,7)^rotr32(x,18)^(x>>3); }
static inline uint32_t sig1(uint32_t x) { return rotr32(x,17)^rotr32(x,19)^(x>>10); }

struct SHA256Ctx {
    uint32_t h[8];
    uint8_t  buf[64];
    uint64_t total;
};

static void sha256_init(SHA256Ctx* c) {
    c->h[0]=0x6a09e667; c->h[1]=0xbb67ae85; c->h[2]=0x3c6ef372; c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f; c->h[5]=0x9b05688c; c->h[6]=0x1f83d9ab; c->h[7]=0x5be0cd19;
    c->total = 0;
}

static void sha256_block(SHA256Ctx* c, const uint8_t* blk) {
    uint32_t w[64], a,b,d,e,f,g,h,t1,t2;
    uint32_t* hh = c->h;
    for(int i=0;i<16;i++)
        w[i]=((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|((uint32_t)blk[i*4+2]<<8)|blk[i*4+3];
    for(int i=16;i<64;i++)
        w[i]=sig1(w[i-2])+w[i-7]+sig0(w[i-15])+w[i-16];
    a=hh[0];b=hh[1];uint32_t cc=hh[2];d=hh[3];e=hh[4];f=hh[5];g=hh[6];h=hh[7];
    for(int i=0;i<64;i++){
        t1=h+Sig1(e)+Ch(e,f,g)+K256[i]+w[i];
        t2=Sig0(a)+Maj(a,b,cc);
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    hh[0]+=a;hh[1]+=b;hh[2]+=cc;hh[3]+=d;hh[4]+=e;hh[5]+=f;hh[6]+=g;hh[7]+=h;
}

static void sha256_update(SHA256Ctx* c, const uint8_t* data, size_t len) {
    size_t used = c->total & 63;
    c->total += len;
    if (used) {
        size_t free = 64 - used;
        if (len < free) { memcpy(c->buf+used, data, len); return; }
        memcpy(c->buf+used, data, free);
        sha256_block(c, c->buf);
        data += free; len -= free;
    }
    while (len >= 64) { sha256_block(c, data); data += 64; len -= 64; }
    if (len) memcpy(c->buf, data, len);
}

static void sha256_final(SHA256Ctx* c, uint8_t out[32]) {
    uint64_t bits = c->total * 8;
    uint8_t pad = 0x80;
    sha256_update(c, &pad, 1);
    pad = 0;
    while ((c->total & 63) != 56) sha256_update(c, &pad, 1);
    uint8_t lenBuf[8];
    for(int i=7;i>=0;i--){ lenBuf[i]=bits&0xFF; bits>>=8; }
    sha256_update(c, lenBuf, 8);
    for(int i=0;i<8;i++){
        out[i*4+0]=(c->h[i]>>24)&0xFF; out[i*4+1]=(c->h[i]>>16)&0xFF;
        out[i*4+2]=(c->h[i]>>8)&0xFF;  out[i*4+3]=(c->h[i]>>0)&0xFF;
    }
}

static void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    SHA256Ctx c; sha256_init(&c); sha256_update(&c, data, len); sha256_final(&c, out);
}

static void sha256d(const uint8_t* data, size_t len, uint8_t out[32]) {
    sha256(data, len, out); sha256(out, 32, out);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SHA-512 (for HMAC-SHA512)
// ═══════════════════════════════════════════════════════════════════════════════

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};

static inline uint64_t rotr64(uint64_t x,int n){return (x>>n)|(x<<(64-n));}
static inline uint64_t Ch64(uint64_t e,uint64_t f,uint64_t g){return(e&f)^(~e&g);}
static inline uint64_t Maj64(uint64_t a,uint64_t b,uint64_t c){return(a&b)^(a&c)^(b&c);}
static inline uint64_t Sig0_64(uint64_t a){return rotr64(a,28)^rotr64(a,34)^rotr64(a,39);}
static inline uint64_t Sig1_64(uint64_t e){return rotr64(e,14)^rotr64(e,18)^rotr64(e,41);}
static inline uint64_t sig0_64(uint64_t x){return rotr64(x,1)^rotr64(x,8)^(x>>7);}
static inline uint64_t sig1_64(uint64_t x){return rotr64(x,19)^rotr64(x,61)^(x>>6);}

struct SHA512Ctx {
    uint64_t h[8];
    uint8_t  buf[128];
    uint64_t total;
};

static void sha512_init(SHA512Ctx* c){
    c->h[0]=0x6a09e667f3bcc908ULL;c->h[1]=0xbb67ae8584caa73bULL;
    c->h[2]=0x3c6ef372fe94f82bULL;c->h[3]=0xa54ff53a5f1d36f1ULL;
    c->h[4]=0x510e527fade682d1ULL;c->h[5]=0x9b05688c2b3e6c1fULL;
    c->h[6]=0x1f83d9abfb41bd6bULL;c->h[7]=0x5be0cd19137e2179ULL;
    c->total=0;
}

static void sha512_block(SHA512Ctx* c, const uint8_t* blk){
    uint64_t w[80],a,b,cc,d,e,f,g,h,t1,t2;
    for(int i=0;i<16;i++){
        w[i]=0;
        for(int j=0;j<8;j++) w[i]=(w[i]<<8)|blk[i*8+j];
    }
    for(int i=16;i<80;i++) w[i]=sig1_64(w[i-2])+w[i-7]+sig0_64(w[i-15])+w[i-16];
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for(int i=0;i<80;i++){
        t1=h+Sig1_64(e)+Ch64(e,f,g)+K512[i]+w[i];
        t2=Sig0_64(a)+Maj64(a,b,cc);
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;
    c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}

static void sha512_update(SHA512Ctx* c, const uint8_t* data, size_t len){
    size_t used=c->total&127; c->total+=len;
    if(used){ size_t fr=128-used;
        if(len<fr){memcpy(c->buf+used,data,len);return;}
        memcpy(c->buf+used,data,fr);sha512_block(c,c->buf);data+=fr;len-=fr;
    }
    while(len>=128){sha512_block(c,data);data+=128;len-=128;}
    if(len)memcpy(c->buf,data,len);
}

static void sha512_final(SHA512Ctx* c, uint8_t out[64]){
    uint64_t bits=c->total*8;
    uint8_t pad=0x80; sha512_update(c,&pad,1); pad=0;
    while((c->total&127)!=112) sha512_update(c,&pad,1);
    uint8_t lenBuf[16]={0};
    for(int i=15;i>=8;i--){lenBuf[i]=bits&0xFF;bits>>=8;}
    sha512_update(c,lenBuf,16);
    for(int i=0;i<8;i++){
        for(int j=0;j<8;j++) out[i*8+j]=(c->h[i]>>(56-j*8))&0xFF;
    }
}

static void hmac_sha512(const uint8_t* key, size_t klen,
                        const uint8_t* data, size_t dlen,
                        uint8_t out[64])
{
    uint8_t k[128]={0};
    if(klen>128){ SHA512Ctx hc; sha512_init(&hc); sha512_update(&hc,key,klen); sha512_final(&hc,k); klen=64; }
    else memcpy(k,key,klen);

    uint8_t ipad[128], opad[128];
    for(int i=0;i<128;i++){ ipad[i]=k[i]^0x36; opad[i]=k[i]^0x5C; }

    SHA512Ctx hc;
    uint8_t inner[64];
    sha512_init(&hc); sha512_update(&hc,ipad,128); sha512_update(&hc,data,dlen); sha512_final(&hc,inner);
    sha512_init(&hc); sha512_update(&hc,opad,128); sha512_update(&hc,inner,64); sha512_final(&hc,out);
}

// ═══════════════════════════════════════════════════════════════════════════════
// secp256k1 — big-integer arithmetic (256-bit) for point operations
// ═══════════════════════════════════════════════════════════════════════════════
// We represent 256-bit numbers as uint32_t[8] big-endian.

typedef uint32_t u256[8];

static const uint8_t P_BYTES[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFC,0x2F
};
static const uint8_t N_BYTES[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x41
};
static const uint8_t GX_BYTES[32] = {
    0x79,0xBE,0x66,0x7E,0xF9,0xDC,0xBB,0xAC,0x55,0xA0,0x62,0x95,0xCE,0x87,0x0B,0x07,
    0x02,0x9B,0xFC,0xDB,0x2D,0xCE,0x28,0xD9,0x59,0xF2,0x81,0x5B,0x16,0xF8,0x17,0x98
};
static const uint8_t GY_BYTES[32] = {
    0x48,0x3A,0xDA,0x77,0x26,0xA3,0xC4,0x65,0x5D,0xA4,0xFB,0xFC,0x0E,0x11,0x08,0xA8,
    0xFD,0x17,0xB4,0x48,0xA6,0x85,0x54,0x19,0x9C,0x47,0xD0,0x8F,0xFB,0x10,0xD4,0xB8
};

static void u256_from_bytes(u256 r, const uint8_t* b){
    for(int i=0;i<8;i++) r[i]=((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|((uint32_t)b[i*4+2]<<8)|b[i*4+3];
}
static void u256_to_bytes(uint8_t* b, const u256 r){
    for(int i=0;i<8;i++){b[i*4]=(r[i]>>24)&0xFF;b[i*4+1]=(r[i]>>16)&0xFF;b[i*4+2]=(r[i]>>8)&0xFF;b[i*4+3]=r[i]&0xFF;}
}
static void u256_copy(u256 dst, const u256 src){ memcpy(dst,src,32); }
static bool u256_is_zero(const u256 a){ for(int i=0;i<8;i++) if(a[i]) return false; return true; }
static int  u256_cmp(const u256 a, const u256 b){
    for(int i=0;i<8;i++){ if(a[i]<b[i])return -1; if(a[i]>b[i])return 1; } return 0;
}
// a += b, returns carry
static uint32_t u256_add(u256 r, const u256 a, const u256 b){
    uint64_t carry=0;
    for(int i=7;i>=0;i--){ uint64_t s=(uint64_t)a[i]+b[i]+carry; r[i]=(uint32_t)s; carry=s>>32; }
    return (uint32_t)carry;
}
// r = a - b (assumes a >= b)
static void u256_sub(u256 r, const u256 a, const u256 b){
    int64_t borrow=0;
    for(int i=7;i>=0;i--){
        int64_t d=(int64_t)a[i]-(int64_t)b[i]-borrow;
        if(d<0){r[i]=(uint32_t)(d+0x100000000LL);borrow=1;}else{r[i]=(uint32_t)d;borrow=0;}
    }
}

static u256 FP, FN, FGX, FGY;

static void mod_reduce(u256 r, const u256 a, const u256 m){
    u256_copy(r, a);
    while(u256_cmp(r, m) >= 0) u256_sub(r, r, m);
}

static void mod_add(u256 r, const u256 a, const u256 b, const u256 m){
    uint32_t carry = u256_add(r, a, b);
    if(carry || u256_cmp(r, m) >= 0) u256_sub(r, r, m);
}

static void mod_sub(u256 r, const u256 a, const u256 b, const u256 m){
    if(u256_cmp(a, b) >= 0) u256_sub(r, a, b);
    else { u256_sub(r, b, a); u256_sub(r, m, r); }
}

// r = a * b mod m  (using schoolbook with 512-bit intermediate)
static void mod_mul(u256 r, const u256 a, const u256 b, const u256 m){
    // 512-bit product
    uint32_t prod[16]={0};
    for(int i=7;i>=0;i--){
        uint64_t carry=0;
        for(int j=7;j>=0;j--){
            uint64_t cur=(uint64_t)a[i]*b[j]+prod[i+j+1]+carry;
            prod[i+j+1]=(uint32_t)cur;
            carry=cur>>32;
        }
        prod[i]+=(uint32_t)carry;
    }
    // reduce mod m by repeated subtraction (slow but simple; key derivation is infrequent)
    // More efficient: Barrett or Montgomery — but we keep code small for embedded
    // Convert to u256 via iterative halving — use double-and-add reduction
    // For 512->256 mod P we do: prod mod m
    // Simple approach: extract 256-bit chunks and use the identity:
    //   (hi * 2^256 + lo) mod p, and reduce hi first
    // For secp256k1 p = 2^256 - 2^32 - 977, so 2^256 ≡ 2^32 + 977 (mod p)
    // This makes one step fast:
    u256 lo, hi;
    for(int i=0;i<8;i++){ hi[i]=prod[i]; lo[i]=prod[i+8]; }

    // hi * 2^256 mod p = hi * (2^32 + 977) mod p
    // = hi*2^32 + hi*977
    u256 tmp, tmp2;
    // hi << 32
    for(int i=0;i<7;i++) tmp[i]=hi[i+1]; tmp[7]=0; // shift left 32 bits in 32-bit words
    // hi * 977
    uint64_t carry2=0;
    for(int i=7;i>=0;i--){ uint64_t v=(uint64_t)hi[i]*977+carry2; tmp2[i]=(uint32_t)v; carry2=v>>32; }
    // (hi<<32 + hi*977) mod p
    u256 hmod;
    uint32_t c1=u256_add(hmod, tmp, tmp2);
    // Add carry * 2^256, i.e. carry*(2^32+977) — but carry is tiny (0 or 1)
    if(c1){ u256 extra={0,0,0,0,0,0,0,0}; extra[6]=c1; extra[7]=(uint32_t)(c1*977); u256_add(hmod,hmod,extra); }
    mod_reduce(hmod, hmod, FP);

    // hmod + lo mod p
    mod_add(r, hmod, lo, FP);
}

// r = a^(-1) mod p  using Fermat: a^(p-2) mod p
// Only called ONCE per derive (to convert Jacobian back to affine) — not per step
static void mod_inv_p(u256 r, const u256 a) {
    // p - 2 for secp256k1 — hardcoded to avoid recomputing
    static const uint8_t PM2[32] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFC,0x2D
    };
    u256 exp; u256_from_bytes(exp, PM2);
    u256 base; u256_copy(base, a);
    u256 res = {0,0,0,0,0,0,0,1};
    for(int bit=255; bit>=0; bit--) {
        mod_mul(res, res, res, FP);
        int word=bit/32, shift=bit%32;
        if((exp[7-word]>>shift)&1) mod_mul(res, res, base, FP);
    }
    u256_copy(r, res);
}

// ── Jacobian (projective) coordinates ────────────────────────────────────────
// Point (X:Y:Z) represents affine (X/Z², Y/Z³)
// Infinity = Z == 0
// No mod_inv needed during add/double — only once at the end to normalise.
struct JPt { u256 X, Y, Z; };

static void jpt_set_infinity(JPt& R) {
    memset(R.X,0,32); memset(R.Y,0,32); memset(R.Z,0,32);
}
static bool jpt_is_infinity(const JPt& P) { return u256_is_zero(P.Z); }

// Set Jacobian point from affine
static void jpt_from_affine(JPt& J, const u256 x, const u256 y) {
    u256_copy(J.X, x); u256_copy(J.Y, y);
    memset(J.Z,0,32); J.Z[7]=1; // Z=1
}

// Convert Jacobian back to affine — costs ONE mod_inv
static void jpt_to_affine(u256 rx, u256 ry, const JPt& P) {
    u256 zinv, zinv2, zinv3;
    mod_inv_p(zinv, P.Z);
    mod_mul(zinv2, zinv,  zinv,  FP);
    mod_mul(zinv3, zinv2, zinv,  FP);
    mod_mul(rx,    P.X,   zinv2, FP);
    mod_mul(ry,    P.Y,   zinv3, FP);
}

// Jacobian point doubling — 4 mod_mul, 4 mod_add/sub, 0 mod_inv
static void jpt_double(JPt& R, const JPt& P) {
    if(jpt_is_infinity(P)) { R=P; return; }
    u256 S, M, T, Y2, Z2;
    // S = 4*X*Y²
    mod_mul(Y2, P.Y, P.Y, FP);
    mod_mul(S, P.X, Y2, FP);
    u256 four={0,0,0,0,0,0,0,4};
    mod_mul(S, S, four, FP);
    // M = 3*X²  (a=0 for secp256k1)
    mod_mul(M, P.X, P.X, FP);
    u256 three={0,0,0,0,0,0,0,3};
    mod_mul(M, M, three, FP);
    // T = M² - 2*S
    mod_mul(T, M, M, FP);
    u256 S2; uint32_t c=u256_add(S2,S,S); if(c||u256_cmp(S2,FP)>=0) u256_sub(S2,S2,FP);
    mod_sub(T, T, S2, FP);
    // X' = T
    u256_copy(R.X, T);
    // Y' = M*(S-T) - 8*Y⁴
    mod_sub(R.Y, S, T, FP);
    mod_mul(R.Y, M, R.Y, FP);
    mod_mul(Y2, Y2, Y2, FP);  // Y⁴
    u256 eight={0,0,0,0,0,0,0,8};
    mod_mul(Y2, Y2, eight, FP);
    mod_sub(R.Y, R.Y, Y2, FP);
    // Z' = 2*Y*Z
    mod_mul(R.Z, P.Y, P.Z, FP);
    c=u256_add(R.Z,R.Z,R.Z); if(c||u256_cmp(R.Z,FP)>=0) u256_sub(R.Z,R.Z,FP);
}

// Jacobian + affine point addition — 8 mod_mul, 0 mod_inv
// Uses the mixed addition formula (Q is affine, Z_Q=1)
static void jpt_add_affine(JPt& R, const JPt& P, const u256 Qx, const u256 Qy) {
    if(jpt_is_infinity(P)) { jpt_from_affine(R, Qx, Qy); return; }
    u256 Z2, U2, S2, H, R2, H2, H3;
    mod_mul(Z2, P.Z, P.Z, FP);           // Z1²
    mod_mul(U2, Qx,  Z2,  FP);           // U2 = X2*Z1²
    mod_mul(S2, Qy,  Z2,  FP);
    mod_mul(S2, S2,  P.Z, FP);           // S2 = Y2*Z1³
    mod_sub(H,  U2,  P.X, FP);           // H  = U2 - X1
    mod_sub(R2, S2,  P.Y, FP);           // R  = S2 - Y1
    if(u256_is_zero(H)) {
        if(u256_is_zero(R2)) { jpt_double(R, P); return; }
        jpt_set_infinity(R); return;
    }
    mod_mul(H2, H,  H,  FP);             // H²
    mod_mul(H3, H2, H,  FP);             // H³
    // X3 = R²-H³-2*X1*H²
    mod_mul(R.X, R2, R2, FP);
    mod_sub(R.X, R.X, H3, FP);
    u256 X1H2; mod_mul(X1H2, P.X, H2, FP);
    u256 X1H2x2; uint32_t cc=u256_add(X1H2x2,X1H2,X1H2); if(cc||u256_cmp(X1H2x2,FP)>=0) u256_sub(X1H2x2,X1H2x2,FP);
    mod_sub(R.X, R.X, X1H2x2, FP);
    // Y3 = R*(X1*H²-X3)-Y1*H³
    mod_sub(R.Y, X1H2, R.X, FP);
    mod_mul(R.Y, R2, R.Y, FP);
    u256 Y1H3; mod_mul(Y1H3, P.Y, H3, FP);
    mod_sub(R.Y, R.Y, Y1H3, FP);
    // Z3 = H*Z1
    mod_mul(R.Z, H, P.Z, FP);
}

// Scalar multiplication using Jacobian coords + affine base point
// Only 1 mod_inv at the very end (jpt_to_affine)
static void pt_mul_jacobian(u256 rx, u256 ry, const u256 k, const u256 Px, const u256 Py) {
    JPt R; jpt_set_infinity(R);
    for(int bit=255; bit>=0; bit--) {
        jpt_double(R, R);
        int word=bit/32, shift=bit%32;
        if((k[7-word]>>shift)&1) jpt_add_affine(R, R, Px, Py);
    }
    jpt_to_affine(rx, ry, R);
}

// Affine point add (used only once at the end to combine ilPt + parentPt)
static void pt_add_affine(u256 rx, u256 ry, const u256 Px, const u256 Py,
                                             const u256 Qx, const u256 Qy) {
    JPt J; jpt_from_affine(J, Px, Py);
    jpt_add_affine(J, J, Qx, Qy);
    jpt_to_affine(rx, ry, J);
}

static void secp256k1_init(){
    u256_from_bytes(FP, P_BYTES);
    u256_from_bytes(FN, N_BYTES);
    u256_from_bytes(FGX, GX_BYTES);
    u256_from_bytes(FGY, GY_BYTES);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Base58 decode (for xpub)
// ═══════════════════════════════════════════════════════════════════════════════

static const char* BASE58_CHARS = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static int base58_char_val(char c){
    for(int i=0;i<58;i++) if(BASE58_CHARS[i]==c) return i;
    return -1;
}

// decode base58 string into out[outlen] bytes. returns actual length or -1.
static int base58_decode(const char* str, uint8_t* out, int outlen){
    memset(out, 0, outlen);
    int len = strlen(str);
    // count leading '1's → leading zero bytes
    int zeros=0;
    while(str[zeros]=='1') zeros++;

    // decode into big-endian big integer stored in out[]
    for(int i=zeros;i<len;i++){
        int val=base58_char_val(str[i]);
        if(val<0) return -1;
        uint32_t carry=val;
        for(int j=outlen-1;j>=0;j--){
            uint32_t cur=(uint32_t)out[j]*58+carry;
            out[j]=(uint8_t)cur;
            carry=cur>>8;
        }
    }
    return outlen;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════════════

bool bip32DecodeXpub(const char* xpubStr, uint8_t outPubKey[33], uint8_t outChainCode[32]){
    // xpub = 78 bytes + 4-byte checksum = 82 bytes in base58check (~111 chars)
    uint8_t raw[82];
    memset(raw, 0, sizeof(raw));
    if(base58_decode(xpubStr, raw, 82) < 0) return false;

    // Verify checksum
    uint8_t hash[32];
    sha256d(raw, 78, hash);
    if(raw[78]!=hash[0]||raw[79]!=hash[1]||raw[80]!=hash[2]||raw[81]!=hash[3]) return false;

    // raw layout: [4 version][1 depth][4 fingerprint][4 index][32 chain][33 key]
    memcpy(outChainCode, raw+13, 32);
    memcpy(outPubKey,    raw+45, 33);
    return true;
}

bool bip32DerivePublic(const uint8_t parentPub[33],
                       const uint8_t parentChain[32],
                       uint32_t index,
                       uint8_t childPub[33],
                       uint8_t childChain[32])
{
    if(index >= 0x80000000) return false; // hardened not supported

    secp256k1_init();

    // Data = 0x02/03 || parentPub[1..32] || index (big-endian 4 bytes)
    uint8_t data[37];
    memcpy(data, parentPub, 33);
    data[33]=(index>>24)&0xFF; data[34]=(index>>16)&0xFF;
    data[35]=(index>>8)&0xFF;  data[36]=index&0xFF;

    uint8_t I[64];
    hmac_sha512(parentChain, 32, data, 37, I);

    // IL must be < N
    u256 IL, N;
    u256_from_bytes(IL, I);
    u256_from_bytes(N, N_BYTES);
    if(u256_cmp(IL,N)>=0) return false;

    // Child pubkey = point(IL) + parentPub
    // Both operations use Jacobian coords — total of 3 mod_inv calls (1 per pt_mul_jacobian, 1 for pt_add_affine)
    u256 ilX, ilY;
    pt_mul_jacobian(ilX, ilY, IL, FGX, FGY);

    // Decompress parentPub
    u256 px; u256_from_bytes(px, parentPub+1);
    u256 py2, py;
    mod_mul(py2, px, px, FP);
    mod_mul(py2, py2, px, FP);
    uint32_t seven[8]={0,0,0,0,0,0,0,7};
    mod_add(py2, py2, seven, FP);
    // py = py2^((p+1)/4) — secp256k1 has p ≡ 3 mod 4, so sqrt = x^((p+1)/4)
    static const uint8_t SQRT_EXP[32] = {
        0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xbf,0xff,0xff,0x0c
    };
    u256 exp_u; u256_from_bytes(exp_u, SQRT_EXP);
    u256 base_u; u256_copy(base_u, py2);
    u256 res_u={0,0,0,0,0,0,0,1};
    for(int bit=255;bit>=0;bit--){
        mod_mul(res_u,res_u,res_u,FP);
        int word=bit/32, shift2=bit%32;
        if((exp_u[7-word]>>shift2)&1) mod_mul(res_u,res_u,base_u,FP);
    }
    u256_copy(py, res_u);
    bool yOdd = (py[7]&1)==1;
    bool wantOdd = (parentPub[0]==0x03);
    if(yOdd != wantOdd) mod_sub(py, FP, py, FP);

    // Add ilPt + parentPt (both affine, result via Jacobian)
    u256 cx, cy;
    pt_add_affine(cx, cy, ilX, ilY, px, py);
    if(u256_is_zero(cx) && u256_is_zero(cy)) return false;

    // Compress child point
    uint8_t cpub[33];
    cpub[0] = (cy[7]&1) ? 0x03 : 0x02;
    u256_to_bytes(cpub+1, cx);
    memcpy(childPub, cpub, 33);

    // IR = right 32 bytes of I = child chain code
    memcpy(childChain, I+32, 32);
    return true;
}
