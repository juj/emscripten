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

#elif defined(_MSC_VER)

#include <winsock2.h>
#include <ws2tcpip.h>

#define SHUTDOWN_READ SD_RECEIVE
#define SHUTDOWN_WRITE SD_SEND
#define SHUTDOWN_BIDIRECTIONAL SD_BOTH
#define SETSOCKOPT_PTR_TYPE const char*
#define SEND_RET_TYPE int
#define SEND_FORMATTING_SPECIFIER "%d"

#endif
