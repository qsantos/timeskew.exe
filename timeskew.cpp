// Usage: compile withdll.exe in Detours, then
// Detours\bin.X64\withdll.exe -d:timeskew.dll program.exe

// Needed fror Query*InterruptTime*
#pragma comment(lib, "mincore.lib")
// Needed for SetTimer
#pragma comment(lib, "user32.lib")

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "detours.h"

typedef unsigned __int64 QWORD;

// TODO: mitigate risk of overflow when multiplying by num or by denom
static DWORD num = 10;
static DWORD denom = 1;

// TODO: GetTimeSysInfoFunc
static int(*TrueSelect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const timeval *timeout) = select;
static void(WINAPI* TrueSleep)(DWORD dwMilliseconds) = Sleep;
static DWORD(WINAPI* TrueSleepEx)(DWORD dwMilliseconds, BOOL bAlertable) = SleepEx;
static void(WINAPI* TrueGetSystemTime)(LPSYSTEMTIME lpSystemTime) = GetSystemTime;
static void(WINAPI* TrueGetLocalTime)(LPSYSTEMTIME lpSystemTime) = GetLocalTime;
static void(WINAPI* TrueGetSystemTimeAsFileTime)(LPFILETIME lpSystemTimeAsFileTime) = GetSystemTimeAsFileTime;
static UINT_PTR(WINAPI* TrueSetTimer)(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc) = SetTimer;
static DWORD(WINAPI* TrueWaitForSingleObject)(HANDLE hHandle, DWORD dwMilliseconds) = WaitForSingleObject;
static DWORD(WINAPI* TrueWaitForSingleObjectEx)(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable) = WaitForSingleObjectEx;
static DWORD(WINAPI* TrueWaitForMultipleObjects)(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds) = WaitForMultipleObjects;
static DWORD(WINAPI* TrueWaitForMultipleObjectsEx)(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable) = WaitForMultipleObjectsEx;
static BOOL(WINAPI* TrueSleepConditionVariableCS)(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds) = SleepConditionVariableCS;
static BOOL(WINAPI* TrueSleepConditionVariableSRW)(PCONDITION_VARIABLE ConditionVariable, PSRWLOCK SRWLock, DWORD dwMilliseconds, ULONG Flags) = SleepConditionVariableSRW;
static BOOL(WINAPI* TrueSetWaitableTimer)(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, BOOL fResume) = SetWaitableTimer;
static BOOL(WINAPI* TrueSetWaitableTimerEx)(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext, ULONG TolerableDelay) = SetWaitableTimerEx;
static DWORD(WINAPI* TrueMsgWaitForMultipleObjects)(DWORD nCount, const HANDLE* pHandles, BOOL fWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask) = MsgWaitForMultipleObjects;
static DWORD(WINAPI* TrueMsgWaitForMultipleObjectsEx)(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD dwFlags) = MsgWaitForMultipleObjectsEx;
static DWORD(WINAPI* TrueWaitForInputIdle)(HANDLE hProcess, DWORD dwMilliseconds) = WaitForInputIdle;
static DWORD(WINAPI* TrueGetTickCount)() = GetTickCount;
static ULONGLONG(WINAPI* TrueGetTickCount64)() = GetTickCount64;
static BOOL(WINAPI* TrueQueryPerformanceCounter)(LARGE_INTEGER* lpPerformanceCount) = QueryPerformanceCounter;
static void(WINAPI* TrueQueryInterruptTime)(PULONGLONG lpInterruptTime) = QueryInterruptTime;
static void(WINAPI* TrueQueryInterruptTimePrecise)(PULONGLONG lpInterruptTimePrecise) = QueryInterruptTimePrecise;
static BOOL(WINAPI* TrueQueryUnbiasedInterruptTime)(PULONGLONG UnbiasedTime) = QueryUnbiasedInterruptTime;
static void(WINAPI* TrueQueryUnbiasedInterruptTimePrecise)(PULONGLONG lpUnbiasedInterruptTimePrecise) = QueryUnbiasedInterruptTimePrecise;

// TODO: can we use critical sections instead?
// TODO: use separate mutexes for separate functions
HANDLE ghMutex;

static QWORD lastTrueDateTime = 0;
static QWORD lastSkewedDateTime = 0;

void log(const char* format, ...) {
#if 0
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    printf("\n");
    va_end(ap);
#elif 0
    va_list ap;
    FILE* f = NULL;
    if (fopen_s(&f, "C:\\Users\\Quentin\\skewed.log", "a") != 0) {
        return;
    }
    va_start(ap, format);
    vfprintf(f, format, ap);
    vfprintf(f, "\n");
    va_end(ap);
    fclose(f);
#else
    (void) format;
#endif
}
#define log(...)

DWORD WINAPI server(LPVOID lpParam) {
    (void) lpParam;

    log("Starting timeskew server");
    // Windows requires initializing before using sockets
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (res != 0) {
        log("WSAStartup failed with error: %d\n", res);
        exit(1);
    }
    // Get address
    struct addrinfo *addrinfo;
    res = getaddrinfo("127.0.0.1", "40000", NULL, &addrinfo);
    if (res != 0) {
        log("getaddrinfo() failed with error: %d\n", res);
        exit(1);
    }
    // Create socket
    SOCKET listeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listeningSocket == INVALID_SOCKET) {
        log("socket() failed with error: %ld\n", WSAGetLastError());
        exit(1);
    }
    // Bind socket
    res = bind(listeningSocket, addrinfo->ai_addr, (int) addrinfo->ai_addrlen);
    if (res == SOCKET_ERROR) {
        log("bind failed with error: %d\n", WSAGetLastError());
        exit(1);
    }
    freeaddrinfo(addrinfo);
    // Start listening
    res = listen(listeningSocket, SOMAXCONN);
    if (res == SOCKET_ERROR) {
        log("listen() failed with error: %d\n", WSAGetLastError());
        exit(1);
    }
    // Main loop
    SOCKET clientSockets[FD_SETSIZE];
    int nClients = 0;
    fd_set readfdsInput;
    FD_ZERO(&readfdsInput);
    FD_SET(listeningSocket, &readfdsInput);
    while (true) {
        fd_set readfds = readfdsInput;
        select(nClients + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(listeningSocket, &readfds)) {
            // Accept a client socket
            SOCKET sock = accept(listeningSocket, NULL, NULL);
            if (sock == INVALID_SOCKET) {
                log("accept() failed with error: %d\n", WSAGetLastError());
                closesocket(sock);
            } else {
                clientSockets[nClients] = sock;
                nClients += 1;
                FD_SET(sock, &readfdsInput);
            }
        }
        for (int i = 0; i < nClients; i++) {
            if (FD_ISSET(clientSockets[i], &readfds)) {
                // Receive data
                char recvbuf[100];
                res = recv(clientSockets[i], recvbuf, sizeof recvbuf - 1, 0);
                if (res < 0) {
                    // Close socket
                    FD_CLR(clientSockets[i], &readfdsInput);
                    closesocket(clientSockets[i]);
                    // Swap and pop
                    nClients -= 1;
                    clientSockets[i] = clientSockets[nClients];
                    // Play this loop iteration to account for the removed item
                    i -= 1; // NOTE: i is signed
                    continue;
                }
                // NOTE: 0 <= res < sizeof recvbuf
                recvbuf[res] = 0;
                int newNum, newDenom;
                res = _snscanf_s(recvbuf, res, "%d %d", &newNum, &newDenom);
                if (res == 2) {
                    num = newNum;
                    denom = newDenom;
                } else {
                    log("failed to parse '%s'", recvbuf);
                }
            }
        }
    }
}

int SkewedSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const timeval *timeout) {
    timeval *timeout2;
    if (timeout == NULL) {
        timeout2 = NULL;
    } else {
        static const ULONGLONG usecs_per_sec = 1000000ULL;
        timeval skewedTimeout;
        ULONGLONG usecs = timeout->tv_sec * usecs_per_sec + timeout->tv_usec;
        usecs = usecs * denom / num;
        skewedTimeout.tv_sec = (LONG) (usecs / usecs_per_sec);
        skewedTimeout.tv_usec = (LONG) (usecs % usecs_per_sec);
        timeout2 = &skewedTimeout;
    }
    return TrueSelect(nfds, readfds, writefds, exceptfds, timeout2);
}

void SkewedSleep(DWORD dwMilliseconds) {
    log("SkewedSleep");
    TrueSleep(dwMilliseconds * denom / num);
}

DWORD WINAPI SkewedSleepEx(DWORD dwMilliseconds, BOOL bAlertable) {
    log("SkewedSleepEx");
    return TrueSleepEx(dwMilliseconds * denom / num, bAlertable);
}

void SkewedGetSystemTime(LPSYSTEMTIME lpSystemTime) {
    log("SkewedGetSystemTime");
    WaitForSingleObject(ghMutex, INFINITE);
    TrueGetSystemTime(lpSystemTime);
    FILETIME fileTime;
    SystemTimeToFileTime(lpSystemTime, &fileTime);
    QWORD dateTime = fileTime.dwHighDateTime * (1ULL << 32) + fileTime.dwLowDateTime;
    if (lastTrueDateTime == 0) {
        lastTrueDateTime = dateTime;
        lastSkewedDateTime = dateTime;
    } else {
        QWORD delta = dateTime - lastTrueDateTime;
        lastTrueDateTime = dateTime;
        dateTime = lastSkewedDateTime + delta * num / denom;
        lastSkewedDateTime = dateTime;
        fileTime.dwLowDateTime = (DWORD) dateTime;
        fileTime.dwHighDateTime = dateTime >> 32;
    }
    FileTimeToSystemTime(&fileTime, lpSystemTime);
    ReleaseMutex(ghMutex);
}

void SkewedGetLocalTime(LPSYSTEMTIME lpSystemTime) {
    log("SkewedGetLocalTime");
    WaitForSingleObject(ghMutex, INFINITE);
    // NOTE: FileTime is based on UTC, but, hopefully, that still works for our purposes
    TrueGetLocalTime(lpSystemTime);
    FILETIME fileTime;
    SystemTimeToFileTime(lpSystemTime, &fileTime);
    static QWORD lastLocalTrueDateTime = 0;
    static QWORD lastLocalSkewedDateTime = 0;
    QWORD dateTime = fileTime.dwHighDateTime * (1ULL << 32) + fileTime.dwLowDateTime;
    if (lastLocalTrueDateTime == 0) {
        lastLocalTrueDateTime = dateTime;
        lastLocalSkewedDateTime = dateTime;
    } else {
        QWORD delta = dateTime - lastLocalTrueDateTime;
        lastLocalTrueDateTime = dateTime;
        dateTime = lastLocalSkewedDateTime + delta * num / denom;
        lastLocalSkewedDateTime = dateTime;
        fileTime.dwLowDateTime = (DWORD)dateTime;
        fileTime.dwHighDateTime = dateTime >> 32;
    }
    FileTimeToSystemTime(&fileTime, lpSystemTime);
    ReleaseMutex(ghMutex);
}


void SkewedGetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
    log("SkewedGetSystemTimeAsFileTime");
    WaitForSingleObject(ghMutex, INFINITE);
    TrueGetSystemTimeAsFileTime(lpSystemTimeAsFileTime);
    QWORD dateTime = lpSystemTimeAsFileTime->dwHighDateTime * (1ULL << 32) + lpSystemTimeAsFileTime->dwLowDateTime;
    if (lastTrueDateTime == 0) {
        lastTrueDateTime = dateTime;
        lastSkewedDateTime = dateTime;
    } else {
        QWORD delta = dateTime - lastTrueDateTime;
        lastTrueDateTime = dateTime;
        dateTime = lastSkewedDateTime + delta * num / denom;
        lastSkewedDateTime = dateTime;
        lpSystemTimeAsFileTime->dwLowDateTime = (DWORD)dateTime;
        lpSystemTimeAsFileTime->dwHighDateTime = dateTime >> 32;
    }
    ReleaseMutex(ghMutex);
}

UINT_PTR SkewedSetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc) {
    log("SkewedSetTimer");
    return TrueSetTimer(hWnd, nIDEvent, uElapse * denom / num, lpTimerFunc);
}

DWORD SkewedWaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    // NOTE: This just calls WaitForSingleObjectEx afterwards, so we must not apply the skew here
    return TrueWaitForSingleObject(hHandle, dwMilliseconds);
}

DWORD SkewedWaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable) {
    log("SkewedWaitForSingleObjectEx(%ld ms)", dwMilliseconds);
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    return TrueWaitForSingleObjectEx(hHandle, dwMilliseconds, bAlertable);
}

DWORD SkewedWaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds) {
    // NOTE: This just calls WaitForMultipleObjectsEx afterwards, so we must not apply the skew here
    return TrueWaitForMultipleObjects(nCount, lpHandles, bWaitAll, dwMilliseconds);
}

DWORD SkewedWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable) {
    log("SkewedWaitForMultipleObjectsEx(%ld, %ld ms)", nCount, dwMilliseconds);
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    DWORD ret = TrueWaitForMultipleObjectsEx(nCount, lpHandles, bWaitAll, dwMilliseconds, bAlertable);
    log("SkewedWaitForMultipleObjectsEx done");
    return ret;
}

BOOL SkewedSleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds) {
    log("SkewedSleepConditionVariableCS");
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    return TrueSleepConditionVariableCS(ConditionVariable, CriticalSection, dwMilliseconds);
}

BOOL SkewedSleepConditionVariableSRW(PCONDITION_VARIABLE ConditionVariable, PSRWLOCK SRWLock, DWORD dwMilliseconds, ULONG Flags) {
    log("SkewedSleepConditionVariableSRW");
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    return TrueSleepConditionVariableSRW(ConditionVariable, SRWLock, dwMilliseconds, Flags);
}

BOOL SkewedSetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, BOOL fResume) {
    // NOTE: This just calls SetWaitableTimerEx afterwards, so we must not apply the skew here
    return TrueSetWaitableTimer(hTimer, lpDueTime, lPeriod, pfnCompletionRoutine, lpArgToCompletionRoutine, fResume);
}

BOOL SkewedSetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext, ULONG TolerableDelay) {
    log("SkewedSetWaitableTimerEx(%lld, %ld)", lpDueTime->QuadPart, lPeriod);
    WaitForSingleObject(ghMutex, INFINITE);
    LARGE_INTEGER lpSkewedDueTime;
    if (lpDueTime->QuadPart >= 0) {
        // TODO: using lastSkewedDateTime without updating it might not be very reliable
        // absolute time
        QWORD delta = lpDueTime->QuadPart - lastSkewedDateTime;
        lpSkewedDueTime.QuadPart = lastTrueDateTime + delta * denom / num;
    } else {
        // relative time
        lpSkewedDueTime.QuadPart = lpDueTime->QuadPart * denom / num;
    }
    ReleaseMutex(ghMutex);
    log("TrueSetWaitableTimerEx(%lld)", lpSkewedDueTime.QuadPart);
    return TrueSetWaitableTimerEx(hTimer, &lpSkewedDueTime, lPeriod * denom / num, pfnCompletionRoutine, lpArgToCompletionRoutine, WakeContext, TolerableDelay);
}

DWORD SkewedMsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL fWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask) {
    log("SkewedMsgWaitForMultipleObjects");
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    return TrueMsgWaitForMultipleObjects(nCount, pHandles, fWaitAll, dwMilliseconds, dwWakeMask);
}

DWORD SkewedMsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD dwFlags) {
    log("SkewedMsgWaitForMultipleObjectsEx");
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    return TrueMsgWaitForMultipleObjectsEx(nCount, pHandles, dwMilliseconds, dwWakeMask, dwFlags);
}

DWORD SkewedWaitForInputIdle(HANDLE hProcess, DWORD dwMilliseconds) {
    log("SkewedWaitForInputIdle");
    if (dwMilliseconds > 0) {
        dwMilliseconds = dwMilliseconds * denom / num;
    }
    return TrueWaitForInputIdle(hProcess, dwMilliseconds);
}

DWORD SkewedGetTickCount() {
    log("SkewedGetTickCount");
    WaitForSingleObject(ghMutex, INFINITE);
    DWORD tickCount = TrueGetTickCount();
    static DWORD lastTrueTickCount= 0;
    static DWORD lastSkewedTickCount = 0;
    if (lastTrueTickCount == 0) {
        lastTrueTickCount = tickCount;
        lastSkewedTickCount = tickCount;
    } else {
        DWORD delta = tickCount - lastTrueTickCount;
        lastTrueTickCount = tickCount;
        tickCount = lastSkewedTickCount + delta * num / denom;
        lastSkewedTickCount = tickCount;
    }
    ReleaseMutex(ghMutex);
    return tickCount;
}

ULONGLONG SkewedGetTickCount64() {
    log("SkewedGetTickCount64");
    WaitForSingleObject(ghMutex, INFINITE);
    ULONGLONG tickCount = TrueGetTickCount64();
    static ULONGLONG lastTrueTickCount = 0;
    static ULONGLONG lastSkewedTickCount = 0;
    if (lastTrueTickCount == 0) {
        lastTrueTickCount = tickCount;
        lastSkewedTickCount = tickCount;
    } else {
        ULONGLONG delta = tickCount - lastTrueTickCount;
        lastTrueTickCount = tickCount;
        tickCount = lastSkewedTickCount + delta * num / denom;
        lastSkewedTickCount = tickCount;
    }
    ReleaseMutex(ghMutex);
    return tickCount;
}

BOOL SkewedQueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    log("SkewedQueryPerformanceCounter");
    WaitForSingleObject(ghMutex, INFINITE);
    BOOL ret = TrueQueryPerformanceCounter(lpPerformanceCount);
    if (ret == 0) {
        return ret;
    }
    static LONGLONG lastTrue = 0;
    static LONGLONG lastSkewed = 0;
    if (lastTrue == 0) {
        lastTrue = lpPerformanceCount->QuadPart;
        lastSkewed = lpPerformanceCount->QuadPart;
    } else {
        LONGLONG delta = lpPerformanceCount->QuadPart - lastTrue;
        lastTrue = lpPerformanceCount->QuadPart;
        lpPerformanceCount->QuadPart = lastSkewed + delta * num / denom;
        lastSkewed = lpPerformanceCount->QuadPart;
    }
    ReleaseMutex(ghMutex);
    return ret;
}

void SkewedQueryInterruptTime(PULONGLONG lpInterruptTime) {
    log("SkewedQueryInterruptTime");
    WaitForSingleObject(ghMutex, INFINITE);
    TrueQueryInterruptTime(lpInterruptTime);
    static ULONGLONG lastTrue = 0;
    static ULONGLONG lastSkewed = 0;
    if (lastTrue == 0) {
        lastTrue = *lpInterruptTime;
        lastSkewed = *lpInterruptTime;
    } else {
        ULONGLONG delta = *lpInterruptTime - lastTrue;
        lastTrue = *lpInterruptTime;
        *lpInterruptTime = lastSkewed + delta * num / denom;
        lastSkewed = *lpInterruptTime;
    }
    ReleaseMutex(ghMutex);
}

void SkewedQueryInterruptTimePrecise(PULONGLONG lpInterruptTimePrecise) {
    log("SkewedQueryInterruptTimePrecise");
    WaitForSingleObject(ghMutex, INFINITE);
    TrueQueryInterruptTimePrecise(lpInterruptTimePrecise);
    static ULONGLONG lastTrue = 0;
    static ULONGLONG lastSkewed = 0;
    if (lastTrue == 0) {
        lastTrue = *lpInterruptTimePrecise;
        lastSkewed = *lpInterruptTimePrecise;
    } else {
        ULONGLONG delta = *lpInterruptTimePrecise - lastTrue;
        lastTrue = *lpInterruptTimePrecise;
        *lpInterruptTimePrecise = lastSkewed + delta * num / denom;
        lastSkewed = *lpInterruptTimePrecise;
    }
    ReleaseMutex(ghMutex);
}

BOOL SkewedQueryUnbiasedInterruptTime(PULONGLONG UnbiasedTime) {
    log("SkewedQueryUnbiasedInterruptTime");
    WaitForSingleObject(ghMutex, INFINITE);
    BOOL ret = TrueQueryUnbiasedInterruptTime(UnbiasedTime);
    if (ret == 0) {
        return ret;
    }
    static ULONGLONG lastTrue = 0;
    static ULONGLONG lastSkewed = 0;
    if (lastTrue == 0) {
        lastTrue = *UnbiasedTime;
        lastSkewed = *UnbiasedTime;
    } else {
        ULONGLONG delta = *UnbiasedTime - lastTrue;
        lastTrue = *UnbiasedTime;
        *UnbiasedTime = lastSkewed + delta * num / denom;
        lastSkewed = *UnbiasedTime;
    }
    ReleaseMutex(ghMutex);
    return ret;
}

void SkewedQueryUnbiasedInterruptTimePrecise(PULONGLONG lpUnbiasedInterruptTimePrecise) {
    log("SkewedQueryUnbiasedInterruptTimePrecise");
    WaitForSingleObject(ghMutex, INFINITE);
    TrueQueryUnbiasedInterruptTimePrecise(lpUnbiasedInterruptTimePrecise);
    static ULONGLONG lastTrue = 0;
    static ULONGLONG lastSkewed = 0;
    if (lastTrue == 0) {
        lastTrue = *lpUnbiasedInterruptTimePrecise;
        lastSkewed = *lpUnbiasedInterruptTimePrecise;
    } else {
        ULONGLONG delta = *lpUnbiasedInterruptTimePrecise - lastTrue;
        lastTrue = *lpUnbiasedInterruptTimePrecise;
        *lpUnbiasedInterruptTimePrecise = lastSkewed + delta * num / denom;
        lastSkewed = *lpUnbiasedInterruptTimePrecise;
    }
    ReleaseMutex(ghMutex);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
    (void) hinst;
    (void) reserved;
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    ghMutex = CreateMutex(NULL, FALSE, NULL);
    if (ghMutex == NULL) {
        fprintf(stderr, "CreateMutex error: %ld\n", GetLastError());
        return 1;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        if (CreateThread(NULL, 1 << 20, server, NULL, 0, NULL) == NULL) {
            log("Failed to create server thread");
            exit(1);
        }
        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)TrueSelect, SkewedSelect);
        DetourAttach(&(PVOID&)TrueSleep, SkewedSleep);
        DetourAttach(&(PVOID&)TrueSleepEx, SkewedSleepEx);
        DetourAttach(&(PVOID&)TrueGetSystemTime, SkewedGetSystemTime);
        DetourAttach(&(PVOID&)TrueGetLocalTime, SkewedGetLocalTime);
        DetourAttach(&(PVOID&)TrueGetSystemTimeAsFileTime, SkewedGetSystemTimeAsFileTime);
        DetourAttach(&(PVOID&)TrueSetTimer, SkewedSetTimer);
        DetourAttach(&(PVOID&)TrueWaitForSingleObject, SkewedWaitForSingleObject);
        DetourAttach(&(PVOID&)TrueWaitForSingleObjectEx, SkewedWaitForSingleObjectEx);
        DetourAttach(&(PVOID&)TrueWaitForMultipleObjects, SkewedWaitForMultipleObjects);
        DetourAttach(&(PVOID&)TrueWaitForMultipleObjectsEx, SkewedWaitForMultipleObjectsEx);
        DetourAttach(&(PVOID&)TrueSleepConditionVariableCS, SkewedSleepConditionVariableCS);
        DetourAttach(&(PVOID&)TrueSleepConditionVariableSRW, SkewedSleepConditionVariableSRW);
        DetourAttach(&(PVOID&)TrueSetWaitableTimer, SkewedSetWaitableTimer);
        DetourAttach(&(PVOID&)TrueSetWaitableTimerEx, SkewedSetWaitableTimerEx);
        DetourAttach(&(PVOID&)TrueMsgWaitForMultipleObjects, SkewedMsgWaitForMultipleObjects);
        DetourAttach(&(PVOID&)TrueMsgWaitForMultipleObjectsEx, SkewedMsgWaitForMultipleObjectsEx);
        DetourAttach(&(PVOID&)TrueWaitForInputIdle, SkewedWaitForInputIdle);
        DetourAttach(&(PVOID&)TrueGetTickCount, SkewedGetTickCount);
        DetourAttach(&(PVOID&)TrueGetTickCount64, SkewedGetTickCount64);
        DetourAttach(&(PVOID&)TrueQueryPerformanceCounter, SkewedQueryPerformanceCounter);
        DetourAttach(&(PVOID&)TrueQueryInterruptTime, SkewedQueryInterruptTime);
        DetourAttach(&(PVOID&)TrueQueryInterruptTimePrecise, SkewedQueryInterruptTimePrecise);
        DetourAttach(&(PVOID&)TrueQueryUnbiasedInterruptTime, SkewedQueryUnbiasedInterruptTime);
        DetourAttach(&(PVOID&)TrueQueryUnbiasedInterruptTimePrecise, SkewedQueryUnbiasedInterruptTimePrecise);
        DetourTransactionCommit();
    } else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)TrueQueryUnbiasedInterruptTimePrecise, SkewedQueryUnbiasedInterruptTimePrecise);
        DetourDetach(&(PVOID&)TrueQueryUnbiasedInterruptTime, SkewedQueryUnbiasedInterruptTime);
        DetourDetach(&(PVOID&)TrueQueryInterruptTimePrecise, SkewedQueryInterruptTimePrecise);
        DetourDetach(&(PVOID&)TrueQueryInterruptTime, SkewedQueryInterruptTime);
        DetourDetach(&(PVOID&)TrueQueryPerformanceCounter, SkewedQueryPerformanceCounter);
        DetourDetach(&(PVOID&)TrueGetTickCount64, SkewedGetTickCount64);
        DetourDetach(&(PVOID&)TrueGetTickCount, SkewedGetTickCount);
        DetourDetach(&(PVOID&)TrueWaitForInputIdle, SkewedWaitForInputIdle);
        DetourDetach(&(PVOID&)TrueMsgWaitForMultipleObjectsEx, SkewedMsgWaitForMultipleObjectsEx);
        DetourDetach(&(PVOID&)TrueMsgWaitForMultipleObjects, SkewedMsgWaitForMultipleObjects);
        DetourDetach(&(PVOID&)TrueSetWaitableTimerEx, SkewedSetWaitableTimerEx);
        DetourDetach(&(PVOID&)TrueSetWaitableTimer, SkewedSetWaitableTimer);
        DetourDetach(&(PVOID&)TrueSleepConditionVariableSRW, SkewedSleepConditionVariableSRW);
        DetourDetach(&(PVOID&)TrueSleepConditionVariableCS, SkewedSleepConditionVariableCS);
        DetourDetach(&(PVOID&)TrueWaitForMultipleObjectsEx, SkewedWaitForMultipleObjectsEx);
        DetourDetach(&(PVOID&)TrueWaitForMultipleObjects, SkewedWaitForMultipleObjects);
        DetourDetach(&(PVOID&)TrueWaitForSingleObjectEx, SkewedWaitForSingleObjectEx);
        DetourDetach(&(PVOID&)TrueWaitForSingleObject, SkewedWaitForSingleObject);
        DetourDetach(&(PVOID&)TrueSetTimer, SkewedSetTimer);
        DetourDetach(&(PVOID&)TrueGetSystemTimeAsFileTime, SkewedGetSystemTimeAsFileTime);
        DetourDetach(&(PVOID&)TrueGetLocalTime, SkewedGetLocalTime);
        DetourDetach(&(PVOID&)TrueGetSystemTime, SkewedGetSystemTime);
        DetourDetach(&(PVOID&)TrueSleepEx, SkewedSleepEx);
        DetourDetach(&(PVOID&)TrueSleep, SkewedSleep);
        DetourDetach(&(PVOID&)TrueSelect, SkewedSelect);
        DetourTransactionCommit();
    }
    return TRUE;
}
