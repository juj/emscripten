#pragma once

#if defined(__APPLE__) || defined(____linux__)

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define SHUTDOWN_READ SHUT_RD
#define SHUTDOWN_WRITE SHUT_WR
#define SHUTDOWN_BIDIRECTIONAL SHUT_RDWR
#define SETSOCKOPT_PTR_TYPE const int*
#define SEND_RET_TYPE ssize_t
#define SEND_FORMATTING_SPECIFIER "%ld"

#define GET_SOCKET_ERROR() (errno)

#define PRINT_SOCKET_ERROR(errorCode) do { \
  printf("Call failed! errno: %s(%d)\n", strerror(errorCode), errorCode); \
  } while(0)

#elif defined(_MSC_VER)

#include <winsock2.h>
#include <ws2tcpip.h>

#define SHUTDOWN_READ SD_RECEIVE
#define SHUTDOWN_WRITE SD_SEND
#define SHUTDOWN_BIDIRECTIONAL SD_BOTH
#define SETSOCKOPT_PTR_TYPE const char*
#define SEND_RET_TYPE int
#define SEND_FORMATTING_SPECIFIER "%d"

#define GET_SOCKET_ERROR() (WSAGetLastError())

static inline void PRINT_SOCKET_ERROR(int errorCode)
{
	void *lpMsgBuf = 0;
	HRESULT hresult = HRESULT_FROM_WIN32(errorCode);
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		0, hresult, 0 /*Default language*/, (LPTSTR)&lpMsgBuf, 0, 0);
	printf("Call failed! WSAGetLastError: %s(%d)\n", (char*)lpMsgBuf, errorCode);
	LocalFree(lpMsgBuf);
}

#endif
