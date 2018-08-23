#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "posix_sockets.h"
#include <string.h>
#include <errno.h>

#include "websocket_to_posix_proxy.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

uint64_t ntoh64(uint64_t x)
{
  return ntohl(x>>32) | ((uint64_t)ntohl(x&0xFFFFFFFFu) << 32);
}

#define POSIX_SOCKET_MSG_SOCKET 1
#define POSIX_SOCKET_MSG_SOCKETPAIR 2
#define POSIX_SOCKET_MSG_SHUTDOWN 3
#define POSIX_SOCKET_MSG_BIND 4
#define POSIX_SOCKET_MSG_CONNECT 5
#define POSIX_SOCKET_MSG_LISTEN 6
#define POSIX_SOCKET_MSG_ACCEPT 7
#define POSIX_SOCKET_MSG_GETSOCKNAME 8
#define POSIX_SOCKET_MSG_GETPEERNAME 9
#define POSIX_SOCKET_MSG_SEND 10
#define POSIX_SOCKET_MSG_RECV 11
#define POSIX_SOCKET_MSG_SENDTO 12
#define POSIX_SOCKET_MSG_RECVFROM 13
#define POSIX_SOCKET_MSG_SENDMSG 14
#define POSIX_SOCKET_MSG_RECVMSG 15
#define POSIX_SOCKET_MSG_GETSOCKOPT 16
#define POSIX_SOCKET_MSG_SETSOCKOPT 17

#define MAX_SOCKADDR_SIZE 256
#define MAX_OPTIONVALUE_SIZE 16

#define POSIX_SOCKET_DEBUG

struct SocketCallHeader
{
  int callId;
  int function;
};

void WebSocketMessageUnmaskPayload(uint8_t *payload, uint64_t payloadLength, uint32_t maskingKey)
{
  uint8_t maskingKey8[4];
  memcpy(maskingKey8, &maskingKey, 4);
  uint32_t *data_u32 = (uint32_t *)payload;
  uint32_t *end_u32 = (uint32_t *)((uintptr_t)(payload + payloadLength) & ~3u);

  while(data_u32 < end_u32)
    *data_u32++ ^= maskingKey;

  uint8_t *end = payload + payloadLength;
  uint8_t *data = (uint8_t *)data_u32;
  while(data < end)
  {
    *data ^= maskingKey8[(data-payload) % 4];
    ++data;
  }
}

void SendWebSocketMessage(int client_fd, void *buf, uint64_t numBytes)
{
  uint8_t headerData[sizeof(WebSocketMessageHeader) + 8/*possible extended length*/] = {};
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)headerData;
  header->opcode = 0x02;
  header->fin = 1;
  int headerBytes = 2;

  if (numBytes < 126)
    header->payloadLength = numBytes;
  else if (numBytes <= 65535)
  {
    header->payloadLength = 126;
    *(uint16_t*)(headerData+headerBytes) = htons((unsigned short)numBytes);
    headerBytes += 2;
  }
  else
  {
    header->payloadLength = 127;
    *(uint64_t*)(headerData+headerBytes) = hton64(numBytes);
    headerBytes += 8;
  }

#if 1
  printf("Sending %llu bytes message (%llu bytes of payload) to WebSocket\n", headerBytes + numBytes, numBytes);

  printf("Header:");
  for(int i = 0; i < headerBytes; ++i)
    printf(" %02X", headerData[i]);

  printf("\nPayload:");
  for(int i = 0; i < numBytes; ++i)
    printf(" %02X", ((unsigned char*)buf)[i]);
  printf("\n");
#endif

  send(client_fd, (const char*)headerData, headerBytes, 0); // header
  send(client_fd, (const char*)buf, (int)numBytes, 0); // payload
}

#define PRINT_ERRNO() do { \
  int Errno = errno; \
  printf("Call failed! errno: %s(%d)\n", strerror(Errno), Errno); \
  } while(0)

void Socket(int client_fd, uint8_t *data, uint64_t numBytes) // int socket(int domain, int type, int protocol);
{
  struct MSG {
    SocketCallHeader header;
    int domain;
    int type;
    int protocol;
  };
  MSG *d = (MSG*)data;

  int ret = socket(d->domain, d->type, d->protocol);

#ifdef POSIX_SOCKET_DEBUG
  printf("socket(domain=%d,type=%d,protocol=%d)->%d\n", d->domain, d->type, d->protocol, ret);
  if (ret < 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Socketpair(int client_fd, uint8_t *data, uint64_t numBytes) // int socketpair(int domain, int type, int protocol, int socket_vector[2]);
{
  struct MSG {
    SocketCallHeader header;
    int domain;
    int type;
    int protocol;
  };
  MSG *d = (MSG*)data;

  int socket_vector[2];

#ifdef _MSC_VER
  printf("TODO implement socketpair() on Windows\n");
  int ret = -1;
#else
  int ret = socketpair(d->domain, d->type, d->protocol, socket_vector);
#endif

#ifdef POSIX_SOCKET_DEBUG
  printf("socketpair(domain=%d,type=%d,protocol=%d, socket_vector=[%d,%d])->%d\n", d->domain, d->type, d->protocol, socket_vector[0], socket_vector[1], ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
    int sv[2];
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  r.sv[0] = socket_vector[0];
  r.sv[1] = socket_vector[1];
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Shutdown(int client_fd, uint8_t *data, uint64_t numBytes) // int shutdown(int socket, int how);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    int how;
  };
  MSG *d = (MSG*)data;

  int ret = shutdown(d->socket, d->how);
#ifdef POSIX_SOCKET_DEBUG
  printf("shutdown(socket=%d,how=%d)->%d\n", d->socket, d->how, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Bind(int client_fd, uint8_t *data, uint64_t numBytes) // int bind(int socket, const struct sockaddr *address, socklen_t address_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*socklen_t*/ address_len;
    uint8_t address[MAX_SOCKADDR_SIZE];
  };
  MSG *d = (MSG*)data;

  int ret = bind(d->socket, (sockaddr*)d->address, d->address_len);
#ifdef POSIX_SOCKET_DEBUG
  printf("bind(socket=%d,address=%p,address_len=%d)->%d\n", d->socket, d->address, d->address_len, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Connect(int client_fd, uint8_t *data, uint64_t numBytes) // int connect(int socket, const struct sockaddr *address, socklen_t address_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*socklen_t*/ address_len;
    uint8_t address[];
  };
  MSG *d = (MSG*)data;

  int actualAddressLen = MIN(d->address_len, (uint32_t)numBytes - sizeof(MSG));

  int ret = connect(d->socket, (sockaddr*)d->address, actualAddressLen);
#ifdef POSIX_SOCKET_DEBUG
  printf("connect(socket=%d,address=%p,address_len=%d)->%d\n", d->socket, d->address, d->address_len, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Listen(int client_fd, uint8_t *data, uint64_t numBytes) // int listen(int socket, int backlog);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    int backlog;
  };
  MSG *d = (MSG*)data;

  int ret = listen(d->socket, d->backlog);
#ifdef POSIX_SOCKET_DEBUG
  printf("listen(socket=%d,backlog=%d)->%d\n", d->socket, d->backlog, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Accept(int client_fd, uint8_t *data, uint64_t numBytes) // int accept(int socket, struct sockaddr *address, socklen_t *address_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*socklen_t*/ address_len;
  };
  MSG *d = (MSG*)data;

  uint8_t address[MAX_SOCKADDR_SIZE];

  socklen_t addressLen = (socklen_t)d->address_len;
  int ret = accept(d->socket, (sockaddr*)address, &addressLen);

#ifdef POSIX_SOCKET_DEBUG
  printf("accept(socket=%d,address=%p,address_len=%u)->%d\n", d->socket, address, d->address_len, ret);
  if (ret < 0) PRINT_ERRNO();
#endif

  struct Result {
    int callId;
    int ret;
    int errno_;
    int address_len;
    uint8_t address[];
  };

  int actualAddressLen = MIN(addressLen, (socklen_t)d->address_len);
  int resultSize = sizeof(Result) + actualAddressLen;
  Result *r = (Result*)malloc(resultSize);
  r->callId = d->header.callId;
  r->ret = ret;
  r->errno_ = (ret < 0) ? errno : 0;
  r->address_len = addressLen;
  memcpy(r->address, address, actualAddressLen);
  SendWebSocketMessage(client_fd, r, resultSize);
  free(r);
}

void Getsockname(int client_fd, uint8_t *data, uint64_t numBytes) // int getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*socklen_t*/ address_len;
  };
  MSG *d = (MSG*)data;

  uint8_t address[MAX_SOCKADDR_SIZE];

  socklen_t addressLen = (socklen_t)d->address_len;
  int ret = getsockname(d->socket, (sockaddr*)address, &addressLen);

#ifdef POSIX_SOCKET_DEBUG
  printf("getsockname(socket=%d,address=%p,address_len=%u)->%d\n", d->socket, address, d->address_len, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct Result {
    int callId;
    int ret;
    int errno_;
    int address_len;
    uint8_t address[];
  };
  int actualAddressLen = MIN(addressLen, (socklen_t)d->address_len);
  int resultSize = sizeof(Result) + actualAddressLen;
  Result *r = (Result*)malloc(resultSize);
  r->callId = d->header.callId;
  r->ret = ret;
  r->errno_ = (ret != 0) ? errno : 0;
  r->address_len = addressLen;
  memcpy(r->address, address, actualAddressLen);
  SendWebSocketMessage(client_fd, r, resultSize);
  free(r);
}

void Getpeername(int client_fd, uint8_t *data, uint64_t numBytes) // int getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*socklen_t*/ address_len;
  };
  MSG *d = (MSG*)data;

  uint8_t address[MAX_SOCKADDR_SIZE];

  socklen_t addressLen = (socklen_t)d->address_len;
  int ret = getpeername(d->socket, (sockaddr*)address, &addressLen);

#ifdef POSIX_SOCKET_DEBUG
  printf("getpeername(socket=%d,address=%p,address_len=%u)->%d\n", d->socket, address, d->address_len, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct Result {
    int callId;
    int ret;
    int errno_;
    int address_len;
    uint8_t address[];
  };
  int actualAddressLen = MIN(addressLen, (socklen_t)d->address_len);
  int resultSize = sizeof(Result) + actualAddressLen;
  Result *r = (Result*)malloc(resultSize);
  r->callId = d->header.callId;
  r->ret = ret;
  r->errno_ = (ret != 0) ? errno : 0;
  r->address_len = addressLen;
  memcpy(r->address, address, actualAddressLen);
  SendWebSocketMessage(client_fd, r, resultSize);
  free(r);
}

void Send(int client_fd, uint8_t *data, uint64_t numBytes) // ssize_t/int send(int socket, const void *message, size_t length, int flags);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*size_t*/ length;
    int flags;
    uint8_t message[];
  };
  MSG *d = (MSG*)data;

  SEND_RET_TYPE ret = send(d->socket, (const char *)d->message, d->length, d->flags);

#ifdef POSIX_SOCKET_DEBUG
  printf("send(socket=%d,message=%p,length=%zd,flags=%d)->%d\n", d->socket, d->message, d->length, d->flags, ret);
  if (ret < 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int/*ssize_t/int*/ ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = (int)ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Recv(int client_fd, uint8_t *data, uint64_t numBytes) // ssize_t/int recv(int socket, void *buffer, size_t length, int flags);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*size_t*/ length;
    int flags;
  };
  MSG *d = (MSG*)data;

  uint8_t *buffer = (uint8_t*)malloc(d->length);
  SEND_RET_TYPE ret = recv(d->socket, (char *)buffer, d->length, d->flags);

#ifdef POSIX_SOCKET_DEBUG
  printf("recv(socket=%d,buffer=%p,length=%zd,flags=%d)->%d\n", d->socket, buffer, d->length, d->flags, ret);
  if (ret < 0) PRINT_ERRNO();
#endif

  int receivedBytes = MAX(ret, 0);

  struct Result {
    int callId;
    int/*ssize_t/int*/ ret;
    int errno_;
    uint8_t data[];
  };
  int resultSize = sizeof(Result) + receivedBytes;
  Result *r = (Result *)malloc(resultSize);
  r->callId = d->header.callId;
  r->ret = (int)ret;
  r->errno_ = (ret != 0) ? errno : 0;
  memcpy(r->data, buffer, receivedBytes);
  free(buffer);
  SendWebSocketMessage(client_fd, r, resultSize);
  free(r);
}

void Sendto(int client_fd, uint8_t *data, uint64_t numBytes) // ssize_t/int sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*size_t*/ length;
    int flags;
    uint32_t/*socklen_t*/ dest_len;
    uint8_t dest_addr[MAX_SOCKADDR_SIZE];
    uint8_t message[];
  };
  MSG *d = (MSG*)data;

  SEND_RET_TYPE ret = sendto(d->socket, (const char *)d->message, d->length, d->flags, (sockaddr*)d->dest_addr, d->dest_len);

#ifdef POSIX_SOCKET_DEBUG
  printf("sendto(socket=%d,message=%p,length=%zd,flags=%d,dest_addr=%p,dest_len=%d)->%d\n", d->socket, d->message, d->length, d->flags, d->dest_addr, d->dest_len, ret);
  if (ret < 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int/*ssize_t/int*/ ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = (int)ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void Recvfrom(int client_fd, uint8_t *data, uint64_t numBytes) // ssize_t/int recvfrom(int socket, void *buffer, size_t length, int flags, struct sockaddr *address, socklen_t *address_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    uint32_t/*size_t*/ length;
    int flags;
    uint32_t/*socklen_t*/ address_len;
  };
  MSG *d = (MSG*)data;

  uint8_t address[MAX_SOCKADDR_SIZE];
  uint8_t *buffer = (uint8_t *)malloc(d->length);

  socklen_t address_len = (socklen_t)d->address_len;
  int ret = recvfrom(d->socket, (char *)buffer, d->length, d->flags, (sockaddr*)address, &address_len);

#ifdef POSIX_SOCKET_DEBUG
  printf("recvfrom(socket=%d,buffer=%p,length=%zd,flags=%d,address=%p,address_len=%u)->%d\n", d->socket, buffer, d->length, d->flags, address, d->address_len, ret);
  if (ret < 0) PRINT_ERRNO();
#endif

  int receivedBytes = MAX(ret, 0);
  int actualAddressLen = MIN(address_len, (socklen_t)d->address_len);

  struct Result {
    int callId;
    int/*ssize_t/int*/ ret;
    int errno_;
    int data_len;
    int address_len; // N.B. this is the reported address length of the sender, that may be larger than what is actually serialized to this message.
    uint8_t data_and_address[];
  };
  int resultSize = sizeof(Result) + receivedBytes + actualAddressLen;
  Result *r = (Result *)malloc(resultSize);
  r->callId = d->header.callId;
  r->ret = (int)ret;
  r->errno_ = (ret != 0) ? errno : 0;
  r->data_len = receivedBytes;
  r->address_len = d->address_len; // How many bytes would have been needed to fit the whole sender address, not the actual size provided
  memcpy(r->data_and_address, buffer, receivedBytes);
  memcpy(r->data_and_address + receivedBytes, address, actualAddressLen);
  SendWebSocketMessage(client_fd, r, resultSize);
  free(r);
}

void Sendmsg(int client_fd, uint8_t *data, uint64_t numBytes) // ssize_t/int sendmsg(int socket, const struct msghdr *message, int flags);
{
	printf("TODO implement sendmsg()\n");
#ifdef POSIX_SOCKET_DEBUG
//  printf("sendmsg(socket=%d,message=%p,flags=%d)\n", d->socket, d->message, d->flags);
//  if (ret < 0) PRINT_ERRNO();
#endif

  // TODO
}

void Recvmsg(int client_fd, uint8_t *data, uint64_t numBytes) // ssize_t/int recvmsg(int socket, struct msghdr *message, int flags);
{
	printf("TODO implement recvmsg()\n");
#ifdef POSIX_SOCKET_DEBUG
//  printf("recvmsg(socket=%d,message=%p,flags=%d)\n", d->socket, d->message, d->flags);
//  if (ret < 0) PRINT_ERRNO();
#endif
}

void Getsockopt(int client_fd, uint8_t *data, uint64_t numBytes) // int getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    int level;
    int option_name;
    uint32_t/*socklen_t*/ option_len;
  };
  MSG *d = (MSG*)data;

  uint8_t option_value[MAX_OPTIONVALUE_SIZE];

  socklen_t option_len = (socklen_t)d->option_len;
  int ret = getsockopt(d->socket, d->level, d->option_name, (char*)option_value, &option_len);

#ifdef POSIX_SOCKET_DEBUG
  printf("getsockopt(socket=%d,level=%d,option_name=%d,option_value=%p,option_len=%u)->%d\n", d->socket, d->level, d->option_name, option_value, d->option_len, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct Result {
    int callId;
    int ret;
    int errno_;
    int option_len;
    uint8_t option_value[];
  };

  int actualOptionLen = MIN(option_len, (socklen_t)d->option_len);
  int resultSize = sizeof(Result) + actualOptionLen;
  Result *r = (Result*)malloc(resultSize);
  r->callId = d->header.callId;
  r->ret = ret;
  r->errno_ = (ret != 0) ? errno : 0;
  r->option_len = option_len;
  memcpy(r->option_value, option_value, actualOptionLen);
  SendWebSocketMessage(client_fd, r, resultSize);
  free(r);
}

void Setsockopt(int client_fd, uint8_t *data, uint64_t numBytes) // int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
{
  struct MSG {
    SocketCallHeader header;
    int socket;
    int level;
    int option_name;
    int option_len;
    uint8_t option_value[];
  };
  MSG *d = (MSG*)data;
  int actualOptionLen = MIN(d->option_len, (int)(numBytes - sizeof(MSG)));

  int ret = setsockopt(d->socket, d->level, d->option_name, (const char *)d->option_value, actualOptionLen);

#ifdef POSIX_SOCKET_DEBUG
  printf("setsockopt(socket=%d,level=%d,option_name=%d,option_value=%p,option_len=%d)->%d\n", d->socket, d->level, d->option_name, d->option_value, d->option_len, ret);
  if (ret != 0) PRINT_ERRNO();
#endif

  struct {
    int callId;
    int ret;
    int errno_;
  } r;
  r.callId = d->header.callId;
  r.ret = ret;
  r.errno_ = (ret != 0) ? errno : 0;
  SendWebSocketMessage(client_fd, &r, sizeof(r));
}

void ProcessWebSocketMessage(int client_fd, uint8_t *payload, uint64_t numBytes)
{
  if (numBytes < sizeof(SocketCallHeader))
  {
    printf("Received too small sockets call message! size: %d bytes, expected at least %d bytes\n", (int)numBytes, (int)sizeof(SocketCallHeader));
    return;
  }
  SocketCallHeader *header = (SocketCallHeader*)payload;
  switch(header->function)
  {
    case POSIX_SOCKET_MSG_SOCKET: Socket(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_SOCKETPAIR: Socketpair(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_SHUTDOWN: Shutdown(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_BIND: Bind(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_CONNECT: Connect(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_LISTEN: Listen(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_ACCEPT: Accept(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_GETSOCKNAME: Getsockname(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_GETPEERNAME: Getpeername(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_SEND: Send(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_RECV: Recv(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_SENDTO: Sendto(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_RECVFROM: Recvfrom(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_SENDMSG: Sendmsg(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_RECVMSG: Recvmsg(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_GETSOCKOPT: Getsockopt(client_fd, payload, numBytes); break;
    case POSIX_SOCKET_MSG_SETSOCKOPT: Setsockopt(client_fd, payload, numBytes); break;
    default:
      printf("Unknown POSIX_SOCKET_MSG %u received!\n", header->function);
      break;
	}
}
