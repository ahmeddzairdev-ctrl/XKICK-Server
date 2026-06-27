// =============================================================================
// AhnHS_stub.cpp - DUMMY AhnLab HackShield implementation.
//
// Always-OK stubs so the server builds and runs cross-platform WITHOUT the real
// proprietary AhnLab library. Every call succeeds; no actual integrity checking
// is performed. Replace by linking the real AhnLab lib and defining
// XKICK_USE_REAL_HACKSHIELD (which compiles this file out).
//
// The handles returned are non-null sentinel pointers so caller null-checks pass
// (e.g. CInit::InitServerHack / InitClientHack treat NULL as failure).
// =============================================================================
#ifndef XKICK_USE_REAL_HACKSHIELD

#include "AhnHS.h"
#include <cstdio>

// Distinct non-null sentinels (never dereferenced by the stub).
static int s_serverSentinel = 0xA5;
static int s_clientSentinel = 0x5A;

extern "C" {

AHNHS_SERVER_HANDLE _AhnHS_CreateServerObject(const char* pszHShieldFilePath)
{
    (void)pszHShieldFilePath;
    // Real lib loads the .hsb here; stub just returns a valid sentinel handle.
    return (AHNHS_SERVER_HANDLE)&s_serverSentinel;
}

void _AhnHS_CloseServerHandle(AHNHS_SERVER_HANDLE hServer)
{
    (void)hServer;
}

AHNHS_CLIENT_HANDLE _AhnHS_CreateClientObject(AHNHS_SERVER_HANDLE hServer)
{
    (void)hServer;
    return (AHNHS_CLIENT_HANDLE)&s_clientSentinel;
}

void _AhnHS_CloseClientHandle(AHNHS_CLIENT_HANDLE hClient)
{
    (void)hClient;
}

unsigned long _AhnHS_MakeRequest(AHNHS_CLIENT_HANDLE hClient,
                                 AHNHS_TRANS_BUFFER  pRequestBuffer)
{
    (void)hClient;
    if (pRequestBuffer) pRequestBuffer[0] = 0; // empty/benign request body
    return AHNHS_SUCCESS;
}

unsigned long _AhnHS_VerifyResponse(AHNHS_CLIENT_HANDLE hClient,
                                    AHNHS_TRANS_BUFFER  pResponseBuffer,
                                    unsigned short      usResponseSize)
{
    (void)hClient; (void)pResponseBuffer; (void)usResponseSize;
    return AHNHS_SUCCESS; // stub: every client passes
}

unsigned long _AhnHS_VerifyResponseEx(AHNHS_CLIENT_HANDLE hClient,
                                      AHNHS_TRANS_BUFFER  pResponseBuffer,
                                      unsigned short      usResponseSize,
                                      unsigned long       ulOption)
{
    (void)hClient; (void)pResponseBuffer; (void)usResponseSize; (void)ulOption;
    return AHNHS_SUCCESS;
}

} // extern "C"

#endif // XKICK_USE_REAL_HACKSHIELD
