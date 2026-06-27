// =============================================================================
// md5.cpp - MD5 message digest, RSA Data Security reference algorithm
// (independent reimplementation by L. Peter Deutsch, public domain). This is the
// same algorithm the binary statically linked behind md5_init/append/finish, so
// the resulting digests are identical to the ones stored by the original server.
//
// Portable: only <cstring>/<cstdio>; no OS headers, no endianness assumptions
// (words are read/written byte-wise).
// =============================================================================
#include "md5.h"
#include <cstring>
#include <cstdio>

#define T_MASK ((md5_word_t)~0)
#define T1  /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2  /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4  /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5  /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7  /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8  /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13   0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16   0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19   0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22   0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25   0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28   0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31   0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35   0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38   0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41   0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44   0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47   0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50   0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53   0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57   0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60   0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63   0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)

static void md5_process(md5_state_t* pms, const md5_byte_t* data /*[64]*/)
{
    md5_word_t a = pms->abcd[0], b = pms->abcd[1],
               c = pms->abcd[2], d = pms->abcd[3];
    md5_word_t t;
    md5_word_t xbuf[16];
    const md5_word_t* X;

    {
        // Read 64 bytes as 16 little-endian 32-bit words (portable).
        const md5_byte_t* xp = data;
        int i;
        for (i = 0; i < 16; ++i, xp += 4)
            xbuf[i] = (md5_word_t)(xp[0]) | ((md5_word_t)(xp[1]) << 8) |
                      ((md5_word_t)(xp[2]) << 16) | ((md5_word_t)(xp[3]) << 24);
        X = xbuf;
    }

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define SET(a, b, c, d, k, s, Ti)                       \
    t = a + f + X[k] + Ti;                              \
    a = ROTATE_LEFT(t, s) + b

    // Round 1: f = (b & c) | (~b & d)
#define F(b, c, d) (((b) & (c)) | (~(b) & (d)))
    { md5_word_t f;
      f = F(b,c,d); SET(a,b,c,d, 0, 7, T1);
      f = F(a,b,c); SET(d,a,b,c, 1,12, T2);
      f = F(d,a,b); SET(c,d,a,b, 2,17, T3);
      f = F(c,d,a); SET(b,c,d,a, 3,22, T4);
      f = F(b,c,d); SET(a,b,c,d, 4, 7, T5);
      f = F(a,b,c); SET(d,a,b,c, 5,12, T6);
      f = F(d,a,b); SET(c,d,a,b, 6,17, T7);
      f = F(c,d,a); SET(b,c,d,a, 7,22, T8);
      f = F(b,c,d); SET(a,b,c,d, 8, 7, T9);
      f = F(a,b,c); SET(d,a,b,c, 9,12, T10);
      f = F(d,a,b); SET(c,d,a,b,10,17, T11);
      f = F(c,d,a); SET(b,c,d,a,11,22, T12);
      f = F(b,c,d); SET(a,b,c,d,12, 7, T13);
      f = F(a,b,c); SET(d,a,b,c,13,12, T14);
      f = F(d,a,b); SET(c,d,a,b,14,17, T15);
      f = F(c,d,a); SET(b,c,d,a,15,22, T16);
    }
#undef F

    // Round 2: g = (b & d) | (c & ~d)
#define G(b, c, d) (((b) & (d)) | ((c) & ~(d)))
    { md5_word_t f;
      f = G(b,c,d); SET(a,b,c,d, 1, 5, T17);
      f = G(a,b,c); SET(d,a,b,c, 6, 9, T18);
      f = G(d,a,b); SET(c,d,a,b,11,14, T19);
      f = G(c,d,a); SET(b,c,d,a, 0,20, T20);
      f = G(b,c,d); SET(a,b,c,d, 5, 5, T21);
      f = G(a,b,c); SET(d,a,b,c,10, 9, T22);
      f = G(d,a,b); SET(c,d,a,b,15,14, T23);
      f = G(c,d,a); SET(b,c,d,a, 4,20, T24);
      f = G(b,c,d); SET(a,b,c,d, 9, 5, T25);
      f = G(a,b,c); SET(d,a,b,c,14, 9, T26);
      f = G(d,a,b); SET(c,d,a,b, 3,14, T27);
      f = G(c,d,a); SET(b,c,d,a, 8,20, T28);
      f = G(b,c,d); SET(a,b,c,d,13, 5, T29);
      f = G(a,b,c); SET(d,a,b,c, 2, 9, T30);
      f = G(d,a,b); SET(c,d,a,b, 7,14, T31);
      f = G(c,d,a); SET(b,c,d,a,12,20, T32);
    }
#undef G

    // Round 3: h = b ^ c ^ d
#define H(b, c, d) ((b) ^ (c) ^ (d))
    { md5_word_t f;
      f = H(b,c,d); SET(a,b,c,d, 5, 4, T33);
      f = H(a,b,c); SET(d,a,b,c, 8,11, T34);
      f = H(d,a,b); SET(c,d,a,b,11,16, T35);
      f = H(c,d,a); SET(b,c,d,a,14,23, T36);
      f = H(b,c,d); SET(a,b,c,d, 1, 4, T37);
      f = H(a,b,c); SET(d,a,b,c, 4,11, T38);
      f = H(d,a,b); SET(c,d,a,b, 7,16, T39);
      f = H(c,d,a); SET(b,c,d,a,10,23, T40);
      f = H(b,c,d); SET(a,b,c,d,13, 4, T41);
      f = H(a,b,c); SET(d,a,b,c, 0,11, T42);
      f = H(d,a,b); SET(c,d,a,b, 3,16, T43);
      f = H(c,d,a); SET(b,c,d,a, 6,23, T44);
      f = H(b,c,d); SET(a,b,c,d, 9, 4, T45);
      f = H(a,b,c); SET(d,a,b,c,12,11, T46);
      f = H(d,a,b); SET(c,d,a,b,15,16, T47);
      f = H(c,d,a); SET(b,c,d,a, 2,23, T48);
    }
#undef H

    // Round 4: i = c ^ (b | ~d)
#define I(b, c, d) ((c) ^ ((b) | ~(d)))
    { md5_word_t f;
      f = I(b,c,d); SET(a,b,c,d, 0, 6, T49);
      f = I(a,b,c); SET(d,a,b,c, 7,10, T50);
      f = I(d,a,b); SET(c,d,a,b,14,15, T51);
      f = I(c,d,a); SET(b,c,d,a, 5,21, T52);
      f = I(b,c,d); SET(a,b,c,d,12, 6, T53);
      f = I(a,b,c); SET(d,a,b,c, 3,10, T54);
      f = I(d,a,b); SET(c,d,a,b,10,15, T55);
      f = I(c,d,a); SET(b,c,d,a, 1,21, T56);
      f = I(b,c,d); SET(a,b,c,d, 8, 6, T57);
      f = I(a,b,c); SET(d,a,b,c,15,10, T58);
      f = I(d,a,b); SET(c,d,a,b, 6,15, T59);
      f = I(c,d,a); SET(b,c,d,a,13,21, T60);
      f = I(b,c,d); SET(a,b,c,d, 4, 6, T61);
      f = I(a,b,c); SET(d,a,b,c,11,10, T62);
      f = I(d,a,b); SET(c,d,a,b, 2,15, T63);
      f = I(c,d,a); SET(b,c,d,a, 9,21, T64);
    }
#undef I
#undef SET
#undef ROTATE_LEFT

    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void md5_init(md5_state_t* pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void md5_append(md5_state_t* pms, const md5_byte_t* data, int nbytes)
{
    const md5_byte_t* p = data;
    int left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    md5_word_t nbits = (md5_word_t)(nbytes << 3);

    if (nbytes <= 0)
        return;

    // Update the message length count.
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
        pms->count[1]++;

    // Process an initial partial block.
    if (offset)
    {
        int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);
        memcpy(pms->buf + offset, p, copy);
        if (offset + copy < 64)
            return;
        p += copy;
        left -= copy;
        md5_process(pms, pms->buf);
    }

    // Process full blocks.
    for (; left >= 64; p += 64, left -= 64)
        md5_process(pms, p);

    // Process a final partial block.
    if (left)
        memcpy(pms->buf, p, left);
}

void md5_finish(md5_state_t* pms, md5_byte_t digest[16])
{
    static const md5_byte_t pad[64] = { 0x80 };
    md5_byte_t data[8];
    int i;

    // Save the length before padding.
    for (i = 0; i < 8; ++i)
        data[i] = (md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    // Pad to 56 bytes mod 64, then append the length.
    md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    md5_append(pms, data, 8);
    for (i = 0; i < 16; ++i)
        digest[i] = (md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

// High-level helper: lowercase hex digest of sIn -> sOut (matches the binary).
void md5(std::string& sOut, const std::string& sIn)
{
    md5_state_t state;
    md5_byte_t  digest[16];
    char        hex[33];

    md5_init(&state);
    md5_append(&state, (const md5_byte_t*)sIn.c_str(), (int)sIn.length());
    md5_finish(&state, digest);

    for (int i = 0; i < 16; ++i)
        snprintf(hex + i * 2, 3, "%02x", (unsigned)digest[i]);

    sOut.assign(hex);
}
