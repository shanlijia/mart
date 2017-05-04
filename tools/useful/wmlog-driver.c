#include <stdlib.h>
#include <stdio.h>

//#define LLGENMU_WM_LOG_OUTPUT __WM__OUTPUT__PATH_TO_PROGRAM__TEMPLATE.WM.covlabels

#define str(x) # x
#define xstr(x) str(x)

static FILE* llGenMu_WM_Log_file = (void*) 0;

void llGenMu_WM_Log_Function(unsigned id, int cond) 
{
    if (!llGenMu_WM_Log_file) 
    {
        char* LLGENMU_WM_LOG_OUTPUT_file = getenv(xstr(LLGENMU_WM_LOG_OUTPUT));
        if (! LLGENMU_WM_LOG_OUTPUT_file)
            LLGENMU_WM_LOG_OUTPUT_file = "defaultFileName.WM.covlabels";
        llGenMu_WM_Log_file = fopen(LLGENMU_WM_LOG_OUTPUT_file, "a");
        if (!llGenMu_WM_Log_file) 
        {
            printf("[TEST HARNESS] cannot init weak mutation output file (%s)\n", LLGENMU_WM_LOG_OUTPUT_file);
            return;
        }
    }

    // no caching
    if (cond)
    {
        fprintf(llGenMu_WM_Log_file, "%u\n", id);
        fflush(llGenMu_WM_Log_file); 
    } 
}
