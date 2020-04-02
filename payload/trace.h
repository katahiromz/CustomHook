#pragma once

#ifndef TRACE
    CRITICAL_SECTION s_lock;

    void CH_Init(BOOL bInit)
    {
        if (bInit)
            InitializeCriticalSection(&s_lock);
        else
            DeleteCriticalSection(&s_lock);
    }

    void CH_TraceV(const char *fmt, va_list va)
    {
        FILE *fp;
        EnterCriticalSection(&s_lock);
        fp = fopen("ch-trace.log", "a");
        if (!fp)
            return;
        vfprintf(fp, fmt, va);
        fclose(fp);
        LeaveCriticalSection(&s_lock);
    }

    void CH_Trace(const char *fmt, ...)
    {
        va_list va;
        va_start(va, fmt);
        CH_TraceV(fmt, va);
        va_end(va);
    }

    #define TRACE CH_Trace
#endif
