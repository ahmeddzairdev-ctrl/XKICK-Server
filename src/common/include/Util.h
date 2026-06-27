// =============================================================================
// Util.h - shared string/utility helpers used by BOTH Certify and Game.
//
// These are project-agnostic (no CMember/CPlayer/command-table coupling), so
// they live in kicks_common. Per-project pieces such as GetCommandString /
// IsCommandString stay in each server's own util.cpp (the command tables differ).
// Implementation: common/src/util.cpp. Faithful to the binary's helpers.
// =============================================================================
#ifndef _XKICK_UTIL_H_
#define _XKICK_UTIL_H_

#include <ctime>
#include <cstddef>

// Portable bounded string copy with exact strncpy(3) semantics: copies at most n
// bytes from src; if src is shorter, the remainder up to n is zero-filled; if src
// is >= n it is NOT null-terminated within n (the caller's fixed field owns that).
// Byte-for-byte equivalent to strncpy, but WITHOUT MSVC's non-standard Annex-K
// (_s) deprecation - so it compiles warning-free on both Windows and Linux. Used
// in place of strncpy for fixed-size wire-struct fields.
inline void StrCopyN(char* dst, const char* src, std::size_t n)
{
    std::size_t i = 0;
    for (; i < n && src && src[i] != '\0'; ++i) dst[i] = src[i];
    for (; i < n; ++i) dst[i] = '\0';
}

// Portable thread-safe local time: wraps localtime_s (Windows) / localtime_r
// (POSIX) and returns the broken-down time by value. Replaces the deprecated,
// non-reentrant std::localtime. Defined in common/src/util.cpp (the one place the
// platform split lives, keeping headers ifdef-free). Caller passes the time_t.
std::tm LocalTime(std::time_t t);

// Split src on the separator string sep, copying one token into str and
// returning a pointer to the remainder (NULL when exhausted).
char* Tokener(char* str, char* src, const char* sep);

// True if sComp exactly matches one of the tokens in sText (separated by sep).
bool  IsCheckSeparateString(const char* sText, const char* sComp, const char* sep);

// Copy the first three octets of a dotted IP into sOut ("1.2.3.4" -> "1.2.3").
void  IPTokener(char* sOut, const char* sIP);

// Clean SIGINT shutdown banner, then exit(0).
void  ExitMessage();

#endif // _XKICK_UTIL_H_
