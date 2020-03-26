#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <tchar.h>
#include <string>
#include <cstdio>
#include <vector>
#include <map>

struct FUNCTION
{
    std::string name;
    std::string convention;
    std::string ret;
    std::vector<std::string> params;
};

static std::map<std::string, FUNCTION> s_functions;

template <typename t_string_container, 
          typename t_string = typename t_string_container::value_type>
void split(t_string_container& container,
    const typename t_string_container::value_type& str,
    typename t_string::value_type sep)
{
    container.clear();
    std::size_t i = 0, j = str.find(sep);
    while (j != t_string_container::value_type::npos) {
        container.emplace_back(std::move(str.substr(i, j - i)));
        i = j + 1;
        j = str.find(sep, i);
    }
    container.emplace_back(std::move(str.substr(i, -1)));
}

BOOL GetWondersDirectory(LPWSTR pszPath, INT cchPath)
{
    WCHAR szDir[MAX_PATH], szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szDir, ARRAYSIZE(szDir));
    PathRemoveFileSpecW(szDir);

    lstrcpynW(szPath, szDir, ARRAYSIZE(szPath));
    PathAppendW(szPath, L"WondersXP");
    if (!PathIsDirectoryW(szPath))
    {
        lstrcpynW(szPath, szDir, ARRAYSIZE(szPath));
        PathAppendW(szPath, L"..\\WondersXP");
        if (!PathIsDirectoryW(szPath))
        {
            lstrcpynW(szPath, szDir, ARRAYSIZE(szPath));
            PathAppendW(szPath, L"..\\..\\WondersXP");
            if (!PathIsDirectoryW(szPath))
            {
                lstrcpynW(szPath, szDir, ARRAYSIZE(szPath));
                PathAppendW(szPath, L"..\\..\\..\\WondersXP");
                if (!PathIsDirectoryW(szPath))
                    return FALSE;
            }
        }
    }

    lstrcpynW(pszPath, szPath, cchPath);
    return TRUE;
}

BOOL DoLoadFunctions(LPCWSTR prefix, LPCWSTR suffix)
{
    std::wstring filename = prefix;
    filename += L"functions";
    filename += suffix;

    FILE *fp = _wfopen(filename.c_str(), L"r");
    if (!fp)
        return FALSE;

    char buf[512];
    fgets(buf, ARRAYSIZE(buf), fp);
    while (fgets(buf, ARRAYSIZE(buf), fp))
    {
        StrTrimA(buf, " \t\r\n");

        FUNCTION fn;
        split(fn.params, buf, '\t');
        if (fn.params.size() < 3)
            continue;

        fn.name = fn.params[0];
        fn.convention = fn.params[1];
        fn.ret = fn.params[2];
        fn.params.erase(fn.params.begin(), fn.params.begin() + 3);
        s_functions.insert(std::make_pair(fn.name, fn));
    }

    fclose(fp);
    return TRUE;
}

void DoUpdateList(HWND hwnd, LPCSTR pszText)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);

    SetWindowRedraw(hLst1, FALSE);
    SendDlgItemMessageA(hwnd, lst1, LB_RESETCONTENT, 0, 0);
    for (auto& pair : s_functions)
    {
        if (pair.first.find(pszText) == 0)
        {
            SendDlgItemMessageA(hwnd, lst1, LB_ADDSTRING, 0, (LPARAM)pair.first.c_str());
        }
    }
    if ((INT)SendDlgItemMessageA(hwnd, lst1, LB_GETCOUNT, 0, 0) == 1)
    {
        SendDlgItemMessageA(hwnd, lst1, LB_SETSEL, TRUE, 0);
    }
    SetWindowRedraw(hLst1, TRUE);

    InvalidateRect(hLst1, NULL, TRUE);
}

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
    HICON hIconSmall = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    WCHAR szPath[MAX_PATH];
    if (!GetWondersDirectory(szPath, ARRAYSIZE(szPath)))
    {
        MessageBoxW(hwnd, L"Cannot find WondersXP.", NULL, MB_ICONERROR);
        EndDialog(hwnd, IDABORT);
        return FALSE;
    }
    PathAddBackslashW(szPath);

#ifdef _WIN64
    if (!DoLoadFunctions(szPath, L"-cl-64-w.dat"))
#else
    if (!DoLoadFunctions(szPath, L"-cl-32-w.dat"))
#endif
    {
        MessageBoxW(hwnd, L"Cannot load WondersXP.", NULL, MB_ICONERROR);
        EndDialog(hwnd, IDABORT);
        return FALSE;
    }

    DoUpdateList(hwnd, "");

    return TRUE;
}

BOOL DoWriteFunctionTypes(FILE *fp, const std::vector<std::string>& names)
{
    for (auto& name : names)
    {
        auto it = s_functions.find(name);
        if (it == s_functions.end())
            continue;

        auto& fn = it->second;
        std::vector<std::string> fields;
        split(fields, fn.ret, ':');
        if (fields.size() < 2)
            continue;

        auto ret = fields[1];

        fprintf(fp, "typedef %s (%s *FN_%s)(", ret.c_str(), fn.convention.c_str(),
                name.c_str());

        bool first = true;
        for (auto& param : fn.params)
        {
            if (!first)
                fprintf(fp, ", ");
            if (param == "...")
            {
                fprintf(fp, "%s", param.c_str());
                break;
            }
            split(fields, param, ':');
            fprintf(fp, "%s", fields[1].c_str());
            first = false;
        }
        fprintf(fp, ");\n");
    }
    fprintf(fp, "\n");
    return TRUE;
}

BOOL DoWriteFunctionVariables(FILE *fp, const std::vector<std::string>& names)
{
    for (auto& name : names)
    {
        fprintf(fp, "static FN_%s fn_%s = NULL;\n", name.c_str(), name.c_str());
    }
    fprintf(fp, "\n");
    return TRUE;
}

BOOL DoWriteSpecifier(FILE *fp, std::vector<std::string>& fields)
{
    switch (fields[0][0])
    {
    case 'i':
        if (fields[0] == "i64")
        {
            fprintf(fp, "%%I64d", fields[2].c_str());
        }
        else
        {
            fprintf(fp, "%%d", fields[2].c_str());
        }
        break;
    case 'u':
        if (fields[0] == "u64")
        {
            fprintf(fp, "%%I64u", fields[2].c_str());
        }
        else
        {
            fprintf(fp, "%%u", fields[2].c_str());
        }
        break;
    case 'f':
        fprintf(fp, "%%f", fields[2].c_str());
        break;
    case 'p': case 'h':
        fprintf(fp, "%%p", fields[2].c_str());
        break;
    default:
        fprintf(fp, "?", fields[2].c_str());
        break;
    }
    return TRUE;
}

BOOL DoWriteDetourFunctionBody(FILE *fp, const std::string& name, const FUNCTION& fn)
{
    std::vector<std::string> fields;
    split(fields, fn.ret, ':');

    if (fields[0] != "v")
        fprintf(fp, "    %s ret;\n", fields[1].c_str());

    fprintf(fp, "    DoEnableHook(FALSE);\n");

    fprintf(fp, "    TRACE(\"%s(", name.c_str());
    bool first = true;
    int iarg = 1;
    for (auto& param : fn.params)
    {
        if (!first)
            fprintf(fp, ", ");
        if (param == "...")
        {
            fprintf(fp, "...");
            break;
        }
        split(fields, param, ':');

        if (fields[2].empty())
            fprintf(fp, "arg%d=", iarg);
        else
            fprintf(fp, "%s=", fields[2].c_str());
        DoWriteSpecifier(fp, fields);
        first = false;
        ++iarg;
    }

    if (fn.params.empty())
        fprintf(fp, ")\\n\"");
    else
        fprintf(fp, ")\\n\",\n          ");

    first = true;
    iarg = 1;
    for (auto& param : fn.params)
    {
        if (!first)
            fprintf(fp, ", ");
        if (param == "...")
        {
            break;
        }
        split(fields, param, ':');
        if (fields[2].empty())
            fprintf(fp, "arg%d", iarg);
        else
            fprintf(fp, "%s", fields[2].c_str());
        first = false;
        ++iarg;
    }
    fprintf(fp, ");\n");

    split(fields, fn.ret, ':');
    if (fields[0] == "v")
        fprintf(fp, "    fn_%s(", name.c_str());
    else
        fprintf(fp, "    ret = fn_%s(", name.c_str());

    first = true;
    iarg = 1;
    for (auto& param : fn.params)
    {
        if (!first)
            fprintf(fp, ", ");
        if (param == "...")
        {
            fprintf(fp, "%s", param.c_str());
            break;
        }
        split(fields, param, ':');
        if (fields[2].empty())
            fprintf(fp, "arg%d", iarg);
        else
            fprintf(fp, "%s", fields[2].c_str());
        first = false;
        ++iarg;
    }
    fprintf(fp, ");\n");

    split(fields, fn.ret, ':');
    if (fields[0] != "v")
    {
        fprintf(fp, "    TRACE(\"%s returned ", name.c_str());
        split(fields, fn.ret, ':');
        DoWriteSpecifier(fp, fields);
        fprintf(fp, "\\n\", ret);\n");
    }

    fprintf(fp, "    DoEnableHook(TRUE);\n");

    if (fields[0] != "v")
        fprintf(fp, "    return ret;\n");

    return TRUE;
}

static const std::map<std::string, std::string> s_ellipse_map =
{
    { "printf", "vprintf" },
    { "printf_s", "vprintf_s" },
    { "fprintf", "vfprintf" },
    { "fprintf_s", "vfprintf_s" },
    { "sprintf", "vsprintf" },
    { "sprintf_s", "vsprintf_s" },
    { "vsnprintf", "vsnprintf_s" },
    { "wprintf", "vwprintf" },
    { "fwprintf", "fvwprintf" },
    { "swprintf", "svwprintf" },
    { "scanf", "vscanf" },
    { "scanf_s", "vscanf_s" },
    { "fscanf", "vfscanf" },
    { "fscanf_s", "vfscanf_s" },
    { "sscanf", "vsscanf" },
    { "sscanf_s", "vsscanf_s" },
    { "wscanf", "vwscanf" },
    { "fwscanf", "fvwscanf" },
    { "swscanf", "svwscanf" },
    { "wsprintfA", "wvsprintfA" },
    { "wsprintfW", "wvsprintfW" },
    { "wnsprintfA", "wvnsprintfA" },
    { "wnsprintfW", "wvnsprintfW" },
};

BOOL DoWriteDetourEllipseFunctionBody(FILE *fp, const std::string& name, const FUNCTION& fn)
{
    auto it = s_ellipse_map.find(name);
    if (it == s_ellipse_map.end())
        return FALSE;
    auto vname = it->second;

    std::vector<std::string> fields;
    split(fields, fn.ret, ':');

    fprintf(fp, "    va_list va;\n");

    if (fields[0] != "v")
        fprintf(fp, "    %s ret;\n", fields[1].c_str());

    split(fields, fn.params[fn.params.size() - 2], ':');

    if (fields[2].empty())
        fprintf(fp, "    va_start(va, arg%d);\n", int(fn.params.size() - 1));
    else
        fprintf(fp, "    va_start(va, %s);\n", fields[2]);
    fprintf(fp, "    DoEnableHook(FALSE);\n");

    fprintf(fp, "    TRACE(\"%s(", name.c_str());
    bool first = true;
    int iarg = 1;
    for (auto& param : fn.params)
    {
        if (!first)
            fprintf(fp, ", ");
        if (param == "...")
        {
            fprintf(fp, "...");
            break;
        }
        split(fields, param, ':');

        if (fields[2].empty())
            fprintf(fp, "arg%d=", iarg);
        else
            fprintf(fp, "%s=", fields[2].c_str());
        DoWriteSpecifier(fp, fields);
        first = false;
        ++iarg;
    }

    if (fn.params.empty())
        fprintf(fp, ")\\n\"");
    else
        fprintf(fp, ")\\n\",\n          ");

    first = true;
    iarg = 1;
    for (auto& param : fn.params)
    {
        if (param == "...")
            break;
        if (!first)
            fprintf(fp, ", ");
        split(fields, param, ':');
        if (fields[2].empty())
            fprintf(fp, "arg%d", iarg);
        else
            fprintf(fp, "%s", fields[2].c_str());
        first = false;
        ++iarg;
    }
    fprintf(fp, ");\n");

    split(fields, fn.ret, ':');
    if (fields[0] == "v")
        fprintf(fp, "    %s(", vname.c_str());
    else
        fprintf(fp, "    ret = %s(", vname.c_str());

    first = true;
    iarg = 1;
    for (auto& param : fn.params)
    {
        if (!first)
            fprintf(fp, ", ");
        if (param == "...")
        {
            fprintf(fp, "va");
            break;
        }
        split(fields, param, ':');
        if (fields[2].empty())
            fprintf(fp, "arg%d", iarg);
        else
            fprintf(fp, "%s", fields[2].c_str());
        first = false;
        ++iarg;
    }
    fprintf(fp, ");\n");

    split(fields, fn.ret, ':');
    if (fields[0] != "v")
    {
        fprintf(fp, "    TRACE(\"%s returned ", name.c_str());
        split(fields, fn.ret, ':');
        DoWriteSpecifier(fp, fields);
        fprintf(fp, "\\n\", ret);\n");
    }

    fprintf(fp, "    DoEnableHook(TRUE);\n");
    fprintf(fp, "    va_end(va);\n");

    if (fields[0] != "v")
        fprintf(fp, "    return ret;\n");

    return TRUE;
}

BOOL DoWriteDetourFunctionHead(FILE *fp, const std::string& name, const FUNCTION& fn)
{
    std::vector<std::string> fields;
    split(fields, fn.ret, ':');
    if (fields.size() < 2)
        return FALSE;

    auto ret = fields[1];

    fprintf(fp, "%s %s\n", ret.c_str(), fn.convention.c_str());
    fprintf(fp, "Detour%s(", name.c_str());

    bool first = true;
    int iarg = 1;
    if (fn.params.empty())
    {
        fprintf(fp, "void");
    }
    else
    {
        for (auto& param : fn.params)
        {
            if (!first)
                fprintf(fp, ", ");
            if (param == "...")
            {
                fprintf(fp, "%s", param.c_str());
                break;
            }

            split(fields, param, ':');
            if (fields[2].empty())
                fprintf(fp, "%s arg%d", fields[1].c_str(), iarg);
            else
                fprintf(fp, "%s %s", fields[1].c_str(), fields[2].c_str());
            first = false;
            ++iarg;
        }
    }
    fprintf(fp, ")\n");

    return TRUE;
}

BOOL IsEllipseFunction(const FUNCTION& fn)
{
    return fn.params.size() >= 2 && fn.params[fn.params.size() - 1] == "...";
}

BOOL DoWriteDetourFunctions(FILE *fp, const std::vector<std::string>& names)
{
    for (auto& name : names)
    {
        auto it = s_functions.find(name);
        if (it == s_functions.end())
            continue;

        auto& fn = it->second;

        DoWriteDetourFunctionHead(fp, name, fn);

        fprintf(fp, "{\n");

        if (IsEllipseFunction(fn))
            DoWriteDetourEllipseFunctionBody(fp, name, fn);
        else
            DoWriteDetourFunctionBody(fp, name, fn);

        fprintf(fp, "}\n\n");
    }
    return TRUE;
}

BOOL DoWriteDoHook(FILE *fp, const std::vector<std::string>& names)
{
    fprintf(fp, "BOOL DoHook(BOOL bHook)\n");
    fprintf(fp, "{\n");
    fprintf(fp, "    if (bHook)\n");
    fprintf(fp, "    {\n");
    for (auto& name : names)
    {
        fprintf(fp, "        if (MH_CreateHookCast(&%s, &Detour%s, &fn_%s) != MH_OK)\n",
                name.c_str(), name.c_str(), name.c_str());
        fprintf(fp, "            return FALSE;\n");
    }
    fprintf(fp, "    }\n");
    fprintf(fp, "    else\n");
    fprintf(fp, "    {\n");
    for (auto& name : names)
    {
        fprintf(fp, "        if (MH_RemoveHookCast(&%s) != MH_OK)\n", name.c_str());
        fprintf(fp, "            return FALSE;\n");
    }
    fprintf(fp, "    }\n");
    fprintf(fp, "    return TRUE;\n");
    fprintf(fp, "}\n\n");
    return TRUE;
}

BOOL DoWriteDoEnableHook(FILE *fp, const std::vector<std::string>& names)
{
    fprintf(fp, "BOOL DoEnableHook(BOOL bEnable)\n");
    fprintf(fp, "{\n");
    fprintf(fp, "    if (bEnable)\n");
    fprintf(fp, "    {\n");
    for (auto& name : names)
    {
        fprintf(fp, "        if (MH_EnableHookCast(&%s) != MH_OK)\n", name.c_str());
        fprintf(fp, "            return FALSE;\n");
    }
    fprintf(fp, "    }\n");
    fprintf(fp, "    else\n");
    fprintf(fp, "    {\n");
    for (auto& name : names)
    {
        fprintf(fp, "        if (MH_DisableHookCast(&%s) != MH_OK)\n", name.c_str());
        fprintf(fp, "            return FALSE;\n");
    }
    fprintf(fp, "    }\n");
    fprintf(fp, "    return TRUE;\n");
    fprintf(fp, "}\n\n");
    return TRUE;
}

BOOL DoUpdateFile(HWND hwnd, std::vector<std::string>& names)
{
    WCHAR szPath[MAX_PATH];
    if (!GetWondersDirectory(szPath, ARRAYSIZE(szPath)))
    {
        MessageBoxW(hwnd, L"Cannot find WondersXP.", NULL, MB_ICONERROR);
        return FALSE;
    }
    PathAppendW(szPath, L"..\\payload\\hookbody.hpp");

    FILE *fp = _wfopen(szPath, L"w");
    if (!fp)
    {
        MessageBoxW(hwnd, L"Cannot write file payload\\hookbody.hpp.", NULL, MB_ICONERROR);
        return FALSE;
    }

    fprintf(fp, "/* This file is automatically generated by CustomHook hookmaker. */\n\n");

    DoWriteFunctionTypes(fp, names);
    DoWriteFunctionVariables(fp, names);
    DoWriteDetourFunctions(fp, names);
    DoWriteDoHook(fp, names);
    DoWriteDoEnableHook(fp, names);

    fclose(fp);
    return TRUE;
}

void OnAdd(HWND hwnd)
{
    INT nCount = (INT)SendDlgItemMessageA(hwnd, lst1, LB_GETSELCOUNT, 0, 0);
    if (nCount <= 0)
    {
        return;
    }

    std::vector<INT> selection;
    selection.resize(nCount);
    INT ret = (INT)SendDlgItemMessageA(hwnd, lst1, LB_GETSELITEMS, nCount, (LPARAM)&selection[0]);
    if (ret == LB_ERR)
    {
        MessageBoxW(hwnd, L"Internal error.", NULL, MB_ICONERROR);
        return;
    }

    std::vector<std::string> functions;
    char szBuff[256];
    for (INT i = 0; i < nCount; ++i)
    {
        SendDlgItemMessageA(hwnd, lst1, LB_GETTEXT, selection[i], (LPARAM)szBuff);
        if ((INT)SendDlgItemMessageA(hwnd, lst2, LB_FINDSTRINGEXACT, -1, (LPARAM)szBuff) == LB_ERR)
        {
            SendDlgItemMessageA(hwnd, lst2, LB_ADDSTRING, 0, (LPARAM)szBuff);
        }
    }
}

void OnEdt1(HWND hwnd)
{
    CHAR szTextA[64];
    GetDlgItemTextA(hwnd, edt1, szTextA, ARRAYSIZE(szTextA));
    StrTrimA(szTextA, " \t\r\n");

    DoUpdateList(hwnd, szTextA);
}

void OnUpdateFile(HWND hwnd)
{
    INT nCount = (INT)SendDlgItemMessageA(hwnd, lst2, LB_GETCOUNT, 0, 0);
    if (nCount <= 0)
    {
        MessageBoxW(hwnd, L"Please add function(s) to be hooked.", NULL, MB_ICONERROR);
        return;
    }

    std::vector<std::string> functions;
    char szBuff[256];
    for (INT i = 0; i < nCount; ++i)
    {
        SendDlgItemMessageA(hwnd, lst2, LB_GETTEXT, i, (LPARAM)szBuff);
        functions.push_back(szBuff);
    }

    if (DoUpdateFile(hwnd, functions))
    {
        MessageBoxW(hwnd, L"File payload/hookbody.hpp is successfully updated.",
                    L"CustomHook hookmaker", MB_ICONINFORMATION);
    }
}

void OnClear(HWND hwnd)
{
    SendDlgItemMessageA(hwnd, lst2, LB_RESETCONTENT, 0, 0);
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case edt1:
        OnEdt1(hwnd);
        break;
    case psh1:
        OnAdd(hwnd);
        break;
    case psh2:
        OnUpdateFile(hwnd);
        break;
    case psh3:
        OnClear(hwnd);
        break;
    }
}

INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    InitCommonControls();
    DialogBoxW(hInstance, MAKEINTRESOURCEW(1), NULL, DialogProc);
    return 0;
}
