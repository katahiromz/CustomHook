#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <psapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <tchar.h>
#include <cstdlib>
#include <cassert>
#include "resource.h"

LPWSTR LoadStringDx(INT nID)
{
    static UINT s_index = 0;
    const UINT cchBuffMax = 1024;
    static WCHAR s_sz[4][cchBuffMax];

    WCHAR *pszBuff = s_sz[s_index];
    s_index = (s_index + 1) % _countof(s_sz);
    pszBuff[0] = 0;
    if (!::LoadStringW(NULL, nID, pszBuff, cchBuffMax))
        assert(0);
    return pszBuff;
}

BOOL IsWow64(HANDLE hProcess)
{
    typedef BOOL (WINAPI *FN_IsWow64Process)(HANDLE, LPBOOL);
    HMODULE hKernel32 = GetModuleHandleA("kernel32");
    FN_IsWow64Process pIsWow64Process =
        (FN_IsWow64Process)GetProcAddress(hKernel32, "IsWow64Process");
    if (!pIsWow64Process)
        return FALSE;

    BOOL bWow64;
    if ((*pIsWow64Process)(hProcess, &bWow64))
        return bWow64;
    return FALSE;
}

BOOL DoCheckBits(HANDLE hProcess)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    WCHAR szPath[MAX_PATH];
    switch (info.wProcessorArchitecture)
    {
#ifdef _WIN64
    case PROCESSOR_ARCHITECTURE_AMD64:
    case PROCESSOR_ARCHITECTURE_IA64:
        if (IsWow64(hProcess))
            return FALSE;
        return TRUE;
#else
    case PROCESSOR_ARCHITECTURE_INTEL:
        if (GetModuleFileNameExW(hProcess, NULL, szPath, MAX_PATH))
        {
            DWORD dwType;
            if (GetBinaryTypeW(szPath, &dwType) && dwType == SCS_64BIT_BINARY)
                return FALSE;
        }
        return TRUE;
#endif
    }
    return FALSE;
}

struct AutoCloseHandle
{
    HANDLE m_h;
    AutoCloseHandle(HANDLE h) : m_h(h)
    {
    }
    ~AutoCloseHandle()
    {
        CloseHandle(m_h);
    }
    operator HANDLE()
    {
        return m_h;
    }
};

BOOL DoInjectDLL(HWND hwnd, DWORD pid, LPCWSTR pszDllFile)
{
    AutoCloseHandle hProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
    if (!hProcess)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTOPENPROCESS), NULL, MB_ICONERROR);
        return FALSE;
    }

    if (!DoCheckBits(hProcess))
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_BITSDIFFER), NULL, MB_ICONERROR);
        return FALSE;
    }

    DWORD cbParam = (lstrlenW(pszDllFile) + 1) * sizeof(WCHAR);
    LPVOID pParam = VirtualAllocEx(hProcess, NULL, cbParam, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!pParam)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_OUTOFMEMORY), NULL, MB_ICONERROR);
        return FALSE;
    }

    WriteProcessMemory(hProcess, pParam, pszDllFile, cbParam, NULL);

    HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
    FARPROC pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTFINDLOADER), NULL, MB_ICONERROR);
        VirtualFreeEx(hProcess, pParam, cbParam, MEM_RELEASE);
        return FALSE;
    }

    AutoCloseHandle hThread(CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibraryW, pParam, 0, NULL));
    if (!hThread)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTMAKERTHREAD), NULL, MB_ICONERROR);
        VirtualFreeEx(hProcess, pParam, cbParam, MEM_RELEASE);
        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD dwCode = 0;
    GetExitCodeThread(hThread, &dwCode);

    VirtualFreeEx(hProcess, pParam, cbParam, MEM_RELEASE);
    if (dwCode == 0)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_INJECTIONFAIL), NULL, MB_ICONERROR);
        return FALSE;
    }

    return TRUE;
}

BOOL DoEnableProcessPriviledge(LPCTSTR pszSE_)
{
    BOOL f;
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;
    
    f = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        if (LookupPrivilegeValue(NULL, pszSE_, &luid))
        {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            tp.Privileges[0].Luid = luid;
            f = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
        }
        CloseHandle(hToken);
    }
    
    return f;
}

BOOL DoGetProcessModuleInfo(LPMODULEENTRY32W pme, DWORD pid, LPCWSTR pszModule)
{
    MODULEENTRY32W me = { sizeof(me) };

    AutoCloseHandle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid));
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    if (Module32FirstW(hSnapshot, &me))
    {
        do
        {
            if (lstrcmpiW(me.szModule, pszModule) == 0)
            {
                *pme = me;
                CloseHandle(hSnapshot);
                return TRUE;
            }
        } while (Module32NextW(hSnapshot, &me));
    }

    return FALSE;
}

BOOL DoUninjectDLL(HWND hwnd, DWORD pid, LPCWSTR pszDllFile)
{
    AutoCloseHandle hProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
    if (!hProcess)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTOPENPROCESS), NULL, MB_ICONERROR);
        return FALSE;
    }

    if (!DoCheckBits(hProcess))
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_BITSDIFFER), NULL, MB_ICONERROR);
        return FALSE;
    }

    MODULEENTRY32W me;
    if (!DoGetProcessModuleInfo(&me, pid, PathFindFileNameW(pszDllFile)))
    {
        assert(0);
        return FALSE;
    }
    HMODULE hModule = me.hModule;

    HMODULE hNTDLL = GetModuleHandle(TEXT("ntdll"));
    FARPROC pLdrUnloadDll = GetProcAddress(hNTDLL, "LdrUnloadDll");
    if (!pLdrUnloadDll)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTFINDUNLOADER), NULL, MB_ICONERROR);
        return FALSE;
    }

    AutoCloseHandle hThread(CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLdrUnloadDll, hModule, 0, NULL));
    if (!hThread)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTMAKERTHREAD), NULL, MB_ICONERROR);
        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);

    return TRUE;
}

void OnInject(HWND hwnd, BOOL bInject)
{
    BOOL bTranslated = FALSE;
    DWORD pid = GetDlgItemInt(hwnd, edt1, &bTranslated, FALSE);
    if (!bTranslated)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_INVALIDPID), NULL, MB_ICONERROR);
        return;
    }

    WCHAR szDllFile[MAX_PATH];
    GetModuleFileNameW(NULL, szDllFile, MAX_PATH);
    PathRemoveFileSpecW(szDllFile);
#ifdef _WIN64
    PathAppendW(szDllFile, L"payload64.dll");
#else
    PathAppendW(szDllFile, L"payload32.dll");
#endif
    //MessageBoxW(NULL, szDllFile, NULL, 0);

    if (bInject)
    {
        DoInjectDLL(hwnd, pid, szDllFile);
    }
    else
    {
        DoUninjectDLL(hwnd, pid, szDllFile);
    }
}

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    DragAcceptFiles(hwnd, TRUE);

    WCHAR szExeFile[MAX_PATH];
    GetModuleFileNameW(NULL, szExeFile, MAX_PATH);
    PathRemoveFileSpecW(szExeFile);
    PathAppendW(szExeFile, L"target.exe");

    SetDlgItemTextW(hwnd, edt2, szExeFile);
    return TRUE;
}

void OnBrowse(HWND hwnd)
{
    WCHAR szExeFile[MAX_PATH];
    GetDlgItemTextW(hwnd, edt2, szExeFile, MAX_PATH);

    OPENFILENAMEW ofn = { OPENFILENAME_SIZE_VERSION_400W };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"EXE files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szExeFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST |
                OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Choose EXE file";
    ofn.lpstrDefExt = L"exe";
    if (GetOpenFileNameW(&ofn))
    {
        SetDlgItemTextW(hwnd, edt2, szExeFile);
    }
}

void OnRunWithInjection(HWND hwnd)
{
    WCHAR szExeFile[MAX_PATH], szParams[320];
    GetDlgItemTextW(hwnd, edt2, szExeFile, MAX_PATH);
    GetDlgItemTextW(hwnd, edt3, szParams, MAX_PATH);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    LPWSTR parameters = _wcsdup(szParams);
    BOOL ret = CreateProcessW(szExeFile, parameters, NULL, NULL, TRUE,
                              CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    std::free(parameters);

    if (!ret)
    {
        MessageBoxW(hwnd, LoadStringDx(IDS_CANTSTARTPROCESS), NULL, MB_ICONERROR);
        return;
    }

    SetDlgItemInt(hwnd, edt1, pi.dwProcessId, FALSE);

    WCHAR szDllFile[MAX_PATH];
    GetModuleFileNameW(NULL, szDllFile, MAX_PATH);
    PathRemoveFileSpecW(szDllFile);
#ifdef _WIN64
    PathAppendW(szDllFile, L"payload64.dll");
#else
    PathAppendW(szDllFile, L"payload32.dll");
#endif
    //MessageBoxW(NULL, szDllFile, NULL, 0);

    DoInjectDLL(hwnd, pi.dwProcessId, szDllFile);

    ResumeThread(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case psh1:
        OnInject(hwnd, TRUE);
        break;
    case psh2:
        OnInject(hwnd, FALSE);
        break;
    case psh3:
        OnBrowse(hwnd);
        break;
    case psh4:
        OnRunWithInjection(hwnd);
        break;
    }
}

BOOL GetPathOfShortcut(HWND hWnd, LPCWSTR pszLnkFile, LPWSTR pszPath)
{
    WCHAR            szPath[MAX_PATH];
    IShellLinkW*     pShellLink;
    IPersistFile*    pPersistFile;
    WIN32_FIND_DATAW find;
    BOOL             bRes = FALSE;

    szPath[0] = '\0';
    HRESULT hRes = CoInitialize(NULL);
    if (SUCCEEDED(hRes))
    {
        if (SUCCEEDED(hRes = CoCreateInstance(CLSID_ShellLink, NULL, 
            CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID *)&pShellLink)))
        {
            if (SUCCEEDED(hRes = pShellLink->QueryInterface(IID_IPersistFile, 
                (VOID **)&pPersistFile)))
            {
                hRes = pPersistFile->Load(pszLnkFile,  STGM_READ);
                if (SUCCEEDED(hRes))
                {
                    if (SUCCEEDED(hRes = pShellLink->GetPath(szPath, MAX_PATH, &find, 0)))
                    {
                        if ('\0' != szPath[0])
                        {
                            lstrcpynW(pszPath, szPath, MAX_PATH);
                            bRes = TRUE;
                        }
                    }
                }
                pPersistFile->Release();
            }
            pShellLink->Release();
        }
        CoUninitialize();
    }
    return bRes;
}

void OnDropFiles(HWND hwnd, HDROP hdrop)
{
    WCHAR szPath[MAX_PATH];
    DragQueryFileW(hdrop, 0, szPath, MAX_PATH);

    if (lstrcmpiW(PathFindExtensionW(szPath), L".lnk") == 0)
    {
        WCHAR szTarget[MAX_PATH];
        GetPathOfShortcut(hwnd, szPath, szTarget);
        lstrcpynW(szPath, szTarget, MAX_PATH);
    }

    SetDlgItemTextW(hwnd, edt2, szPath);

    DragFinish(hdrop);
}

INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_DROPFILES, OnDropFiles);
    }
    return 0;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    DoEnableProcessPriviledge(SE_DEBUG_NAME);
    DialogBoxW(hInstance, MAKEINTRESOURCEW(1), NULL, DialogProc);
    return 0;
}
