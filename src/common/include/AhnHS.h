// =============================================================================
// AhnHS.h - AhnLab HackShield (Online / AntiCpXSvr) server-side API.
//
// This declares the HackShield SDK surface that XKICK_Certify / XKICK_Game call.
// The original Linux build statically linked AhnLab's proprietary library; the
// function names/signatures here are recovered from the binary and match the
// real SDK. The implementation is provided by AhnHS_stub.cpp (dummy, always-OK)
// until the proper AhnLab files are dropped in.
//
// To use the REAL library later:
//   - replace this header with AhnLab's official one (or keep it; it's compatible)
//   - define XKICK_USE_REAL_HACKSHIELD and link the AhnLab lib
//   - exclude AhnHS_stub.cpp from the build
//
// NOTE: HackShield is platform-specific (no Windows server lib here). Per the
// cross-platform mandate, these calls are reached ONLY through the wrapper
// functions in HackShield.h; with the stub, the whole tree builds on Win+Linux.
// =============================================================================
#ifndef _AHNHS_H_
#define _AHNHS_H_

#ifdef __cplusplus
extern "C" {
#endif

// ---- handle types (opaque) ----
typedef void* AHNHS_SERVER_HANDLE;   // returned by _AhnHS_CreateServerObject
typedef void* AHNHS_CLIENT_HANDLE;   // per-connection, returned by CreateClientObject

// ---- transmission buffer: fixed 402 bytes (matches binary CSCHackRequest) ----
#define AHNHS_TRANS_BUFFER_SIZE 402
typedef unsigned char AHNHS_TRANS_BUFFER[AHNHS_TRANS_BUFFER_SIZE];

// ---- result codes ----
// Success is 0 (or a non-null handle for Create*). The server treats a specific
// set of 0xE904xxxx codes from VerifyResponse as "client failed integrity check".
#define AHNHS_SUCCESS              0UL

// ---- API (names/signatures recovered from XKICK_Certify) ----
// Loads ../Hack/<HackCode>.hsb; returns a server handle or NULL on failure.
AHNHS_SERVER_HANDLE _AhnHS_CreateServerObject(const char* pszHShieldFilePath);
void                _AhnHS_CloseServerHandle(AHNHS_SERVER_HANDLE hServer);

// Per-connection client object; returns a client handle or NULL on failure.
AHNHS_CLIENT_HANDLE _AhnHS_CreateClientObject(AHNHS_SERVER_HANDLE hServer);
void                _AhnHS_CloseClientHandle(AHNHS_CLIENT_HANDLE hClient);

// Build the request the server pushes to the client (fills pRequestBuffer).
// Returns 0 on success.
unsigned long _AhnHS_MakeRequest(AHNHS_CLIENT_HANDLE hClient,
                                 AHNHS_TRANS_BUFFER  pRequestBuffer);

// Verify the client's response. Returns 0 (or a non-error code) on success,
// or one of the 0xE904xxxx codes if the client failed.
unsigned long _AhnHS_VerifyResponse(AHNHS_CLIENT_HANDLE hClient,
                                    AHNHS_TRANS_BUFFER  pResponseBuffer,
                                    unsigned short      usResponseSize);

unsigned long _AhnHS_VerifyResponseEx(AHNHS_CLIENT_HANDLE hClient,
                                      AHNHS_TRANS_BUFFER  pResponseBuffer,
                                      unsigned short      usResponseSize,
                                      unsigned long       ulOption);

#ifdef __cplusplus
}
#endif

#endif // _AHNHS_H_
