// util.cpp (shared) - project-agnostic helpers used by Certify and Game.
// Per-project GetCommandString/IsCommandString live in each server's util.cpp;
// these tokenizer/IP/exit helpers are identical across both, so they live here.
#include "Util.h"
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

// Portable thread-safe local time (declared in Util.h). This is the single place
// the localtime_s/localtime_r platform split lives, so the rest of the tree calls
// LocalTime() with no ifdefs and no deprecated std::localtime.
std::tm LocalTime(std::time_t t)
{
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);   // MSVC signature: (tm*, const time_t*)
#else
    localtime_r(&t, &out);   // POSIX signature: (const time_t*, tm*)
#endif
    return out;
}

// ---------------------------------------------------------------------------
// Tokener - split src on the separator string sep, copying one token into str
// and returning a pointer to the remainder (NULL when exhausted). Faithful to
// the binary's hand-rolled tokenizer (sep is matched as a contiguous run).
// ---------------------------------------------------------------------------
char* Tokener(char* str, char* src, const char* sep)
{
    if (*src == '\0')
        return 0;

    int i = 0;
    while (sep[i] != '\0')
    {
        if (*src == sep[i]) ++i;
        else                i = 0;

        if (*src == '\0') { *str = '\0'; return src; }
        *str++ = *src++;
    }
    str -= strlen(sep);
    *str = '\0';
    return src;
}

// ---------------------------------------------------------------------------
// IsCheckSeparateString - true if sComp exactly matches one of the tokens in
// sText (separated by sep). Used for admin id/ip allow-lists.
// ---------------------------------------------------------------------------
bool IsCheckSeparateString(const char* sText, const char* sComp, const char* sep)
{
    if (sText == 0 || sComp == 0)
        return false;

    char tmp[40];
    char* next = (char*)sText;
    do
    {
        next = Tokener(tmp, next, sep);
        if (next == 0)
            return false;
    } while (strcmp(sComp, tmp) != 0);
    return true;
}

// ---------------------------------------------------------------------------
// IPTokener - copy the first three octets of a dotted IP into sOut (drops the
// last octet, e.g. "192.168.1.5" -> "192.168.1").
// ---------------------------------------------------------------------------
void IPTokener(char* sOut, const char* sIP)
{
    memcpy(sOut, sIP, 0x14);
    int nDot = 0;
    for (char* p = sOut; *p != '\0'; ++p)
    {
        if (*p == '.' && (++nDot == 3)) { *p = '\0'; return; }
    }
}

// ---------------------------------------------------------------------------
// ExitMessage - clean SIGINT shutdown banner; then exit(0).
// ---------------------------------------------------------------------------
void ExitMessage()
{
    printf("%s", "\x1b[31m[EXIT SERVER]\x1b[0m\n");
    LOGOUT_DATABASE("[EXIT SERVER]\n");
    exit(0);
}
