#include "win32.h"
#include "../config.h"
#include "../minhook/include/MinHook.h"

BOOL DoHook(BOOL bHook);
BOOL DoEnableHook(BOOL bEnable);

#include "trace.h"
#include "hookbody.h"

EXTERN_C BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        CH_Init(TRUE);
        if (MH_Initialize() != MH_OK || !DoHook(TRUE) || !DoEnableHook(TRUE))
            return FALSE;
        break;

    case DLL_PROCESS_DETACH:
        DoEnableHook(FALSE);
        DoHook(FALSE);
        MH_Uninitialize();
        CH_Init(FALSE);
        break;
    }
    return TRUE;
}
