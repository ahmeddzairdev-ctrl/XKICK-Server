// =============================================================================
// md5.h - portable MD5 (RSA Data Security / L. Peter Deutsch reference variant).
//
// The Aug-2010 binary's md5() wraps the classic md5_init/md5_append/md5_finish
// C API over md5_state_t / md5_byte_t and formats a 32-char lowercase hex digest.
// This is that exact algorithm, so password hashes match the DB byte-for-byte.
//
// The public entry point used by the server (sql.cpp / packet.cpp):
//     void md5(std::string& sOut, const std::string& sIn);
// produces the lowercase hex digest of sIn into sOut (matching the binary).
//
// Fully portable: only <cstdint>/<string>, no OS headers.
// =============================================================================
#ifndef _XKICK_MD5_H_
#define _XKICK_MD5_H_

#include <cstdint>
#include <string>

// ---- low-level RSA reference API (same names/shape as the binary) ----
typedef uint8_t  md5_byte_t;   // 8-bit byte
typedef uint32_t md5_word_t;   // 32-bit word

typedef struct md5_state_s
{
    md5_word_t count[2];   // message length in bits, lsw first
    md5_word_t abcd[4];    // digest buffer
    md5_byte_t buf[64];    // accumulate block
} md5_state_t;

void md5_init(md5_state_t* pms);
void md5_append(md5_state_t* pms, const md5_byte_t* data, int nbytes);
void md5_finish(md5_state_t* pms, md5_byte_t digest[16]);

// ---- high-level helper used across the server (matches sql.cpp's extern) ----
void md5(std::string& sOut, const std::string& sIn);

#endif // _XKICK_MD5_H_
