#include "win32.h"
#include "mzcrt/mzstr.h"
#include "mzcrt/mzlib.h"

BOOL DoHook(BOOL bHook);

static DWORD s_dwCurrentProcessId = 0;
static HANDLE s_hCurrentProcess = NULL;

typedef PVOID (WINAPI *FN_ImageDirectoryEntryToData)(PVOID, BOOLEAN, USHORT, PULONG);
static FN_ImageDirectoryEntryToData s_fnImageDirectoryEntryToData = NULL;

typedef BOOL (WINAPI *FN_VirtualProtect)(LPVOID, SIZE_T, DWORD, PDWORD);
static FN_VirtualProtect s_fnVirtualProtect = NULL;

typedef BOOL (WINAPI *FN_WriteProcessMemory)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T *);
static FN_WriteProcessMemory s_fnWriteProcessMemory = NULL;

LPVOID
CH_DoHookEx(HMODULE hModule, const char *module_name, const char *func_name, LPVOID fn)
{
    ULONG size;
    PIMAGE_IMPORT_DESCRIPTOR pImports;
    LPSTR mod_name;
    PIMAGE_THUNK_DATA pIAT;
    PIMAGE_THUNK_DATA pINT;
    WORD ordinal;
    PIMAGE_IMPORT_BY_NAME pName;
    DWORD dwOldProtect;
    LPVOID fnOriginal;

    if (hModule == NULL)
        return NULL;

    if (s_fnImageDirectoryEntryToData)
    {
        pImports = (PIMAGE_IMPORT_DESCRIPTOR)
            s_fnImageDirectoryEntryToData(hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
    }
    else
    {
        pImports = (PIMAGE_IMPORT_DESCRIPTOR)
            ImageDirectoryEntryToData(hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
    }

    for (; pImports->Characteristics != 0; ++pImports)
    {
        mod_name = (LPSTR)((LPBYTE)hModule + pImports->Name);
        if (stricmp(module_name, mod_name) != 0)
            continue;

        pIAT = (PIMAGE_THUNK_DATA)((LPBYTE)hModule + pImports->FirstThunk);
        pINT = (PIMAGE_THUNK_DATA)((LPBYTE)hModule + pImports->OriginalFirstThunk);
        for (; pINT->u1.AddressOfData != 0 && pIAT->u1.Function != 0; pIAT++, pINT++)
        {
            if (IMAGE_SNAP_BY_ORDINAL(pINT->u1.Ordinal))
            {
                ordinal = (WORD)IMAGE_ORDINAL(pINT->u1.Ordinal);
                if (func_name[0] == '#')
                {
                    if (atoi(&func_name[1]) != ordinal)
                        continue;
                }
                else if (HIWORD(func_name) == 0)
                {
                    if (LOWORD(func_name) != ordinal)
                        continue;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                pName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)hModule + pINT->u1.AddressOfData);
                if (stricmp((LPSTR)pName->Name, func_name) != 0)
                    continue;
            }

            if (s_fnVirtualProtect && s_fnWriteProcessMemory)
            {
                if (!s_fnVirtualProtect(&pIAT->u1.Function, sizeof(pIAT->u1.Function), PAGE_READWRITE, &dwOldProtect))
                    return NULL;

                fnOriginal = (LPVOID)pIAT->u1.Function;
                if (fn)
                {
                    s_fnWriteProcessMemory(s_hCurrentProcess, &pIAT->u1.Function, &fn, sizeof(pIAT->u1.Function), NULL);
                    pIAT->u1.Function = (DWORD_PTR)fn;
                }

                s_fnVirtualProtect(&pIAT->u1.Function, sizeof(pIAT->u1.Function), dwOldProtect, &dwOldProtect);
            }
            else
            {
                if (!VirtualProtect(&pIAT->u1.Function, sizeof(pIAT->u1.Function), PAGE_READWRITE, &dwOldProtect))
                    return NULL;

                fnOriginal = (LPVOID)pIAT->u1.Function;
                if (fn)
                {
                    WriteProcessMemory(s_hCurrentProcess, &pIAT->u1.Function, &fn, sizeof(pIAT->u1.Function), NULL);
                    pIAT->u1.Function = (DWORD_PTR)fn;
                }

                VirtualProtect(&pIAT->u1.Function, sizeof(pIAT->u1.Function), dwOldProtect, &dwOldProtect);
            }

            return fnOriginal;
        }
    }

    return NULL;
}

LPVOID
CH_DoHook(const char *module_name, const char *func_name, LPVOID fn)
{
    HMODULE hModule;
    LPVOID ret;

    hModule = GetModuleHandleA(module_name);
    ret = CH_DoHookEx(hModule, module_name, func_name, fn);
    if (ret)
        return ret;

    hModule = GetModuleHandleA(NULL);
    ret = CH_DoHookEx(hModule, module_name, func_name, fn);
    if (ret)
        return ret;

    return ret;
}

BOOL CH_Init(BOOL bInit)
{
    if (bInit)
    {
        s_dwCurrentProcessId = GetCurrentProcessId();
        s_hCurrentProcess = GetCurrentProcess();
        s_fnImageDirectoryEntryToData = CH_DoHook("imagehlp.dll", "ImageDirectoryEntryToData", NULL);
        s_fnVirtualProtect = CH_DoHook("kernel32.dll", "VirtualProtect", NULL);
        s_fnWriteProcessMemory = CH_DoHook("kernel32.dll", "WriteProcessMemory", NULL);
    }
    return TRUE;
}

#include "trace.h"
#include "hookbody.h"

EXTERN_C BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!CH_Init(TRUE))
            return FALSE;
        if (!DoHook(TRUE))
            return FALSE;
        break;

    case DLL_PROCESS_DETACH:
        DoHook(FALSE);
        CH_Init(FALSE);
        break;
    }
    return TRUE;
}
