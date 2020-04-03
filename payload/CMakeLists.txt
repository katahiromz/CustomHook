cmake_minimum_required(VERSION 3.2)

add_library(payload SHARED payload.c)
set_target_properties(payload PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(payload PROPERTIES OUTPUT_NAME "payload64")
else()
    set_target_properties(payload PROPERTIES OUTPUT_NAME "payload32")
endif()

target_link_libraries(payload
    aclui
    advapi32
    cap
    comctl32
    comdlg32
    crypt32
    ctl3d32
    d3drm
    ddraw
    dhcpcsvc
    dinput
    dlcapi
    dplayx
    dsetup
    dsound
    gdi32
    gdiplus
    glu32
    icmui
    imagehlp
    imm32
    iphlpapi
    kernel32
    ksproxy
    lz32
    mapi32
    mfcuia32
    mgmtapi
    mpr
    mprapi
    mqrt
    msacm32
    mscms
    msimg32
    msvfw32
    mswsock
    nddeapi
    netapi32
    odbc32
    ole32
    oleacc
    oleaut32
    olecli32
    oledlg
    olepro32
    olesvr32
    opengl32
    penwin32
    pkpd32
    psapi
    quartz
    rasapi32
    rasdlg
    rpcdce4
    rpcns4
    rpcrt4
    rtm
    rtutils
    secur32
    setupapi
    shell32
    shfolder
    shlwapi
    snmpapi
    svrapi
    tapi32
    url
    user32
    userenv
    uuid
    vdmdbg
    version
    vfw32
    win32spl
    wininet
    winmm
    winspool
    winstrm
    wldap32
    wow32
    ws2_32
    wsnmp32
    wsock32
    wst
    wtsapi32)
