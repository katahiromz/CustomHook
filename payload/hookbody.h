/* This file is automatically generated by CustomHook hookmaker. */

typedef int (__stdcall *FN_MessageBoxW)(HWND, LPCWSTR, LPCWSTR, UINT);

static FN_MessageBoxW fn_MessageBoxW = &MessageBoxW;

int __stdcall
DetourMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
    DWORD dwLastError;
    int ret;
    TRACE("MessageBoxW(hWnd=%p, lpText=%ls, lpCaption=%ls, uType=%u (0x%X))\n",
          hWnd, do_LPCWSTR(lpText), do_LPCWSTR(lpCaption), uType, uType);
    ret = fn_MessageBoxW(hWnd, lpText, lpCaption, uType);
    dwLastError = GetLastError();
    TRACE("MessageBoxW returned %d (0x%X)\n", ret, ret);
    SetLastError(dwLastError);
    return ret;
}

BOOL DoHook(BOOL bHook)
{
    if (bHook)
    {
        fn_MessageBoxW = CH_DoHook("user32.dll", "MessageBoxW", &DetourMessageBoxW);
        if (!fn_MessageBoxW) return FALSE;
    }
    else
    {
        CH_DoHook("user32.dll", "MessageBoxW", fn_MessageBoxW);
    }
    return TRUE;
}

