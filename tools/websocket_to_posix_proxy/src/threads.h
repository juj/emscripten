#pragma once

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#define THREAD_T pthread_t
#define CREATE_THREAD(threadPtr, threadFunc, threadArg) pthread_create(&threadPtr, 0, threadFunc, threadArg)
#define CREATE_THREAD_RETURN_T int
#define CREATE_THREAD_SUCCEEDED(x) (x == 0)
#define EXIT_THREAD(code) pthread_exit((void*)(uintptr_t)code)
#define THREAD_RETURN_T void*
#define MUTEX_T pthread_mutex_t
inline MUTEX_T CREATE_MUTEX()
{
	pthread_mutex_t m;
	pthread_mutex_init(&m, 0);
	return m;
}
#define LOCK_MUTEX(m) pthread_mutex_lock(m)
#define UNLOCK_MUTEX(m) pthread_mutex_unlock(m)
#endif

#if defined(_WIN32)
#include <Windows.h>
#define THREAD_T HANDLE
#define CREATE_THREAD(threadPtr, threadFunc, threadArg) threadPtr = CreateThread(0, 0, threadFunc, threadArg, 0, 0)
#define CREATE_THREAD_RETURN_T HANDLE
#define CREATE_THREAD_SUCCEEDED(x) (x != 0)
#define EXIT_THREAD(code) ExitThread((DWORD)code)
#define THREAD_RETURN_T DWORD WINAPI
#define MUTEX_T CRITICAL_SECTION
inline MUTEX_T CREATE_MUTEX()
{
	CRITICAL_SECTION cs;
	InitializeCriticalSectionAndSpinCount(&cs, 0x00000400);
	return cs;
}
#define LOCK_MUTEX(m) EnterCriticalSection(m)
#define UNLOCK_MUTEX(m) LeaveCriticalSection(m)
#endif
