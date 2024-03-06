//  Microsoft Research Detours Package
//  Copyright (c) Microsoft Corporation.  All rights reserved.
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>

#include <detours.h>

//  Verify that the DLL exports a function with ordinal #1 so that the import table touch-up magic works.
struct ExportContext {
    BOOL    fHasOrdinal1;
    ULONG   nExports;
};
static BOOL CALLBACK ExportCallback(_In_opt_ PVOID pContext, _In_ ULONG nOrdinal, _In_opt_ LPCSTR pszSymbol, _In_opt_ PVOID pbTarget) {
    (void)pbTarget;
    (void)pszSymbol;
    ExportContext *pec = (ExportContext *)pContext;
    if (nOrdinal == 1) {
        pec->fHasOrdinal1 = TRUE;
    }
    pec->nExports++;
    return TRUE;
}

int CDECL main(int argc, char **argv) {
    if (argc <= 1) {
        puts("Usage: timeskew.exe COMMAND...");
        puts("Allow changing the speed of time for a given command");
        return 1;
    }

    // Find timeskew.dll
    CHAR dllPath[MAX_PATH];
    CHAR *end = dllPath + GetModuleFileNameA(NULL, dllPath, MAX_PATH);
    while (*end != '\\') {
        end -= 1;
    }
    end += 1;
    memcpy(end, "timeskew.dll", 13);

    // Load timeskew.dll
    HMODULE hDll = LoadLibraryExA(dllPath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (hDll == NULL) {
        printf("withdll.exe: Error: %s failed to load (error %ld).\n", dllPath, GetLastError());
        return 9003;
    }
    ExportContext ec;
    ec.fHasOrdinal1 = FALSE;
    ec.nExports = 0;
    DetourEnumerateExports(hDll, &ec, ExportCallback);
    FreeLibrary(hDll);
    if (!ec.fHasOrdinal1) {
        printf("withdll.exe: Error: %s does not export ordinal #1.\n", dllPath);
        printf("             See help entry DetourCreateProcessWithDllEx in Detours.chm.\n");
        return 9004;
    }

    // Read command
    int arg = 1;
    CHAR szExe[1024];
    CHAR szCommand[2048] = "\0";
    StringCchCopyA(szExe, sizeof(szExe), argv[arg]);
    while (arg < argc) {
        if (strchr(argv[arg], ' ') != NULL || strchr(argv[arg], '\t') != NULL) {
            StringCchCatA(szCommand, sizeof(szCommand), "\"");
            StringCchCatA(szCommand, sizeof(szCommand), argv[arg]);
            StringCchCatA(szCommand, sizeof(szCommand), "\"");
        }
        else {
            StringCchCatA(szCommand, sizeof(szCommand), argv[arg]);
        }

        if (arg + 1 < argc) {
            StringCchCatA(szCommand, sizeof(szCommand), " ");
        }
        arg++;
    }

    // Create process
    SetLastError(0);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    CHAR szFullExe[1024] = "\0";
    LPCSTR dlls[] = {dllPath};
    DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;
    SearchPathA(NULL, szExe, ".exe", ARRAYSIZE(szFullExe), szFullExe, NULL);
    if (!DetourCreateProcessWithDllsA(szFullExe[0] ? szFullExe : NULL, szCommand,
                                     NULL, NULL, TRUE, dwFlags, NULL, NULL,
                                     &si, &pi, 1, dlls, NULL)) {
        DWORD dwError = GetLastError();
        printf("withdll.exe: DetourCreateProcessWithDllEx failed: %ld\n", dwError);
        if (dwError == ERROR_INVALID_HANDLE) {
#if DETOURS_64BIT
            printf("withdll.exe: Can't detour a 32-bit target process from a 64-bit parent process.\n");
#else
            printf("withdll.exe: Can't detour a 64-bit target process from a 32-bit parent process.\n");
#endif
        }
        ExitProcess(9009);
    }

    // Run process
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Propagate exit code
    DWORD dwResult = 0;
    if (!GetExitCodeProcess(pi.hProcess, &dwResult)) {
        printf("withdll.exe: GetExitCodeProcess failed: %ld\n", GetLastError());
        return 9010;
    }
    return dwResult;
}
