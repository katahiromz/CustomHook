#pragma once

#ifndef TRACE
    void CH_TraceV(const char *fmt, va_list va)
    {
        FILE *fp;
        fp = fopen("ch-trace.log", "a");
        if (!fp)
            return;
        vfprintf(fp, fmt, va);
        fclose(fp);
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
