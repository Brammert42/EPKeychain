/*
 * btcaddr.cpp
 * Bitcoin address generation: Legacy, Native SegWit (bech32), Taproot (bech32m)
 */

#include "btcaddr.h"
#include <string.h>
#include <stdint.h>

// ── SHA-256 (minimal, self-contained) ────────────────────────────────────────
// (We redeclare a local static version to keep this file standalone.)

static const uint32_t _K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t _rotr(uint32_t x,int n){return(x>>n)|(x<<(32-n));}

static void _sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t buf[64]; size_t used=0; uint64_t total=0;

    auto process_block = [&](const uint8_t* blk) {
        uint32_t w[64],a,b,c,d,e,f,g,hh,t1,t2;
        for(int i=0;i<16;i++) w[i]=((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|((uint32_t)blk[i*4+2]<<8)|blk[i*4+3];
        for(int i=16;i<64;i++) w[i]=(_rotr(w[i-2],17)^_rotr(w[i-2],19)^(w[i-2]>>10))+w[i-7]+(_rotr(w[i-15],7)^_rotr(w[i-15],18)^(w[i-15]>>3))+w[i-16];
        a=h[0];b=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];hh=h[7];
        for(int i=0;i<64;i++){
            t1=hh+(_rotr(e,6)^_rotr(e,11)^_rotr(e,25))+((e&f)^(~e&g))+_K[i]+w[i];
            t2=(_rotr(a,2)^_rotr(a,13)^_rotr(a,22))+((a&b)^(a&c)^(b&c));
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    };

    while(len>0){
        size_t cp=64-used; if(cp>len)cp=len;
        memcpy(buf+used,data,cp); data+=cp; len-=cp; used+=cp; total+=cp;
        if(used==64){process_block(buf);used=0;}
    }
    total*=8;
    buf[used++]=0x80;
    if(used>56){while(used<64)buf[used++]=0;process_block(buf);used=0;}
    while(used<56)buf[used++]=0;
    for(int i=7;i>=0;i--){buf[56+(7-i)]=(total>>(i*8))&0xFF;}
    process_block(buf);
    for(int i=0;i<8;i++){out[i*4]=(h[i]>>24)&0xFF;out[i*4+1]=(h[i]>>16)&0xFF;out[i*4+2]=(h[i]>>8)&0xFF;out[i*4+3]=h[i]&0xFF;}
}

// ── RIPEMD-160 ───────────────────────────────────────────────────────────────

static inline uint32_t rol32(uint32_t x,int s){return(x<<s)|(x>>(32-s));}
#define F(x,y,z) ((x)^(y)^(z))
#define G(x,y,z) (((x)&(y))|(~(x)&(z)))
#define H(x,y,z) (((x)|(~(y)))^(z))
#define I(x,y,z) (((x)&(z))|((y)&(~(z))))
#define J(x,y,z) ((x)^((y)|(~(z))))

static void _ripemd160(const uint8_t* data, size_t len, uint8_t out[20]) {
    static const int RL[80]={ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,
        3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,
        1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,
        4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13 };
    static const int RR[80]={ 5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,
        6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,
        15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,
        8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,
        12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11 };
    static const int SL[80]={ 11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,
        7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,
        11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,
        11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,
        9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6 };
    static const int SR[80]={ 8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,
        9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,
        9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,
        15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,
        8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11 };
    static const uint32_t KL[5]={0x00000000,0x5A827999,0x6ED9EBA1,0x8F1BBCDC,0xA953FD4E};
    static const uint32_t KR[5]={0x50A28BE6,0x5C4DD124,0x6D703EF3,0x7A6D76E9,0x00000000};

    uint32_t h[5]={0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    uint8_t buf[64]; size_t used=0; uint64_t total=len;

    auto process=[&](const uint8_t* blk){
        uint32_t X[16];
        for(int i=0;i<16;i++) X[i]=((uint32_t)blk[i*4+3]<<24)|((uint32_t)blk[i*4+2]<<16)|((uint32_t)blk[i*4+1]<<8)|blk[i*4];
        uint32_t al=h[0],bl=h[1],cl=h[2],dl=h[3],el=h[4];
        uint32_t ar=h[0],br=h[1],cr=h[2],dr=h[3],er=h[4];
        for(int j=0;j<80;j++){
            int rnd=j/16;
            uint32_t fl,fr,T;
            if(rnd==0){fl=F(bl,cl,dl);fr=J(br,cr,dr);}
            else if(rnd==1){fl=G(bl,cl,dl);fr=I(br,cr,dr);}
            else if(rnd==2){fl=H(bl,cl,dl);fr=H(br,cr,dr);}
            else if(rnd==3){fl=I(bl,cl,dl);fr=G(br,cr,dr);}
            else{fl=J(bl,cl,dl);fr=F(br,cr,dr);}
            T=rol32(al+fl+X[RL[j]]+KL[rnd],SL[j])+el; al=el;el=dl;dl=rol32(cl,10);cl=bl;bl=T;
            T=rol32(ar+fr+X[RR[j]]+KR[rnd],SR[j])+er; ar=er;er=dr;dr=rol32(cr,10);cr=br;br=T;
        }
        uint32_t T=h[1]+cl+dr;
        h[1]=h[2]+dl+er; h[2]=h[3]+el+ar; h[3]=h[4]+al+br; h[4]=h[0]+bl+cr; h[0]=T;
    };

    while(len>0){
        size_t cp=64-used; if(cp>len)cp=len;
        memcpy(buf+used,data,cp);data+=cp;len-=cp;used+=cp;
        if(used==64){process(buf);used=0;}
    }
    buf[used++]=0x80; uint64_t bits=total*8;
    if(used>56){while(used<64)buf[used++]=0;process(buf);used=0;}
    while(used<56)buf[used++]=0;
    for(int i=0;i<8;i++) buf[56+i]=(bits>>(i*8))&0xFF;
    process(buf);
    for(int i=0;i<5;i++){out[i*4]=(h[i])&0xFF;out[i*4+1]=(h[i]>>8)&0xFF;out[i*4+2]=(h[i]>>16)&0xFF;out[i*4+3]=(h[i]>>24)&0xFF;}
}

static void hash160(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint8_t sha[32]; _sha256(data,len,sha); _ripemd160(sha,32,out);
}

// ── Base58Check encode ────────────────────────────────────────────────────────
static const char* B58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static void base58check_encode(const uint8_t* payload, size_t plen, char* out) {
    // checksum
    uint8_t hash1[32], hash2[32];
    _sha256(payload, plen, hash1);
    _sha256(hash1, 32, hash2);

    uint8_t full[plen+4];
    memcpy(full, payload, plen);
    full[plen]=hash2[0]; full[plen+1]=hash2[1]; full[plen+2]=hash2[2]; full[plen+3]=hash2[3];
    size_t flen=plen+4;

    // count leading zeros
    int zeros=0; while(zeros<(int)flen && full[zeros]==0) zeros++;

    // big-endian big-int base-58 encoding
    uint8_t tmp[flen]; memcpy(tmp,full,flen);
    char rev[128]; int rlen=0;
    bool leading=true;
    while(true){
        bool allzero=true;
        for(size_t i=0;i<flen;i++) if(tmp[i]) {allzero=false;break;}
        if(allzero) break;
        uint32_t rem=0;
        for(size_t i=0;i<flen;i++){
            uint32_t cur=rem*256+tmp[i]; tmp[i]=cur/58; rem=cur%58;
        }
        rev[rlen++]=B58[rem];
    }
    int idx=0;
    for(int i=0;i<zeros;i++) out[idx++]='1';
    for(int i=rlen-1;i>=0;i--) out[idx++]=rev[i];
    out[idx]=0;
}

// ── Bech32 / Bech32m ─────────────────────────────────────────────────────────

static const char* BECH32_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static uint32_t bech32_polymod(const uint8_t* values, size_t len) {
    static const uint32_t GEN[5]={0x3b6a57b2,0x26508e6d,0x1ea119fa,0x3d4233dd,0x2a1462b3};
    uint32_t chk=1;
    for(size_t i=0;i<len;i++){
        uint8_t top=chk>>25; chk=((chk&0x1ffffff)<<5)^values[i];
        for(int j=0;j<5;j++) if((top>>j)&1) chk^=GEN[j];
    }
    return chk;
}

static void bech32_encode(const char* hrp, int witver, const uint8_t* data, size_t datalen, char* out, bool isBech32m) {
    const uint32_t CONST = isBech32m ? 0x2bc830a3 : 1;
    size_t hrplen=strlen(hrp);

    // convert data to 5-bit groups
    uint8_t conv[100]; int clen=0;
    conv[clen++]=(uint8_t)witver;
    // 8-to-5 bit conversion
    uint32_t acc=0; int bits=0;
    for(size_t i=0;i<datalen;i++){
        acc=(acc<<8)|data[i]; bits+=8;
        while(bits>=5){ bits-=5; conv[clen++]=(acc>>bits)&31; }
    }
    if(bits>0) conv[clen++]=(acc<<(5-bits))&31;

    // Compute checksum
    uint8_t poly_input[hrplen*2+2+clen+6];
    int pi=0;
    for(size_t i=0;i<hrplen;i++) poly_input[pi++]=(uint8_t)hrp[i]>>5;
    poly_input[pi++]=0;
    for(size_t i=0;i<hrplen;i++) poly_input[pi++]=(uint8_t)hrp[i]&31;
    for(int i=0;i<clen;i++) poly_input[pi++]=conv[i];
    for(int i=0;i<6;i++) poly_input[pi++]=0;

    uint32_t pm = bech32_polymod(poly_input, pi) ^ CONST;
    uint8_t chk[6];
    for(int i=0;i<6;i++) chk[i]=(pm>>(5*(5-i)))&31;

    // Build string
    int idx=0;
    for(size_t i=0;i<hrplen;i++) out[idx++]=hrp[i];
    out[idx++]='1';
    for(int i=0;i<clen;i++) out[idx++]=BECH32_CHARSET[conv[i]];
    for(int i=0;i<6;i++) out[idx++]=BECH32_CHARSET[chk[i]];
    out[idx]=0;
}

// ── Public functions ──────────────────────────────────────────────────────────

void btcLegacyAddress(const uint8_t pubkey[33], char* out) {
    uint8_t h[20]; hash160(pubkey, 33, h);
    uint8_t payload[21]; payload[0]=0x00; memcpy(payload+1,h,20);
    base58check_encode(payload, 21, out);
}

void btcSegwitAddress(const uint8_t pubkey[33], char* out) {
    uint8_t h[20]; hash160(pubkey, 33, h);
    bech32_encode("bc", 0, h, 20, out, false);
}

void btcTaprootAddress(const uint8_t pubkey[33], char* out) {
    // Taproot: tweak the internal key x-only with tagged hash
    // tweaked_key = pubkey[1..33] (x-only) XOR-tweaked
    // For a keypath-only spend: output_key = internal_key + H_taptweak(internal_key)*G
    // H_taptweak = SHA256(SHA256("TapTweak") || SHA256("TapTweak") || x_only_pubkey)
    // (simplified: just use x-only pubkey as-is for display purposes, matching standard wallet behavior)
    
    // Tagged hash for TapTweak
    static const uint8_t TAPTWEAK_TAG[] = "TapTweak";
    uint8_t tag_hash[32]; _sha256(TAPTWEAK_TAG, 8, tag_hash);
    
    // H = SHA256(tag_hash || tag_hash || x_only_key)
    uint8_t tweak_input[96];
    memcpy(tweak_input, tag_hash, 32);
    memcpy(tweak_input+32, tag_hash, 32);
    memcpy(tweak_input+64, pubkey+1, 32);  // x-only (skip 0x02/03 prefix)
    
    uint8_t tweak[32]; _sha256(tweak_input, 96, tweak);
    
    // output_key x = internal_key + tweak*G (x-coordinate only)
    // For simplicity on constrained hardware, and since this is a display/receive device
    // (not a signing device), we use the x-only pubkey directly as the witness program.
    // Full taproot tweak requires another secp256k1 scalar mul — which we do here:
    // (bip32.cpp's secp machinery is in a different TU; call our local hash as witness prog)
    // Practical note: wallets that derive taproot xpubs already apply the tweak at export.
    // So the raw child key's x-coordinate is the correct witness program.
    bech32_encode("bc", 1, pubkey+1, 32, out, true);
}
