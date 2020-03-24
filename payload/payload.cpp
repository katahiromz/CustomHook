#include "win32.h"
#include "../config.h"
#include "../minhook/include/MinHook.h"

#include "hookbody.hpp"

EXTERN_C BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (MH_Initialize() != MH_OK || !DoHook(TRUE) || !DoEnableHook(TRUE))
            return FALSE;
        break;

    case DLL_PROCESS_DETACH:
        DoEnableHook(FALSE);
        DoHook(FALSE);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
