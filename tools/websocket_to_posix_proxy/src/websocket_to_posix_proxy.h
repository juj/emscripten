#pragma once

#include <stdint.h>

uint64_t ntoh64(uint64_t x);
#define hton64 ntoh64

void WebSocketMessageUnmaskPayload(uint8_t *payload, size_t payloadLength, uint32_t maskingKey);
void ProcessWebSocketMessage(int client_fd, uint8_t *payload, size_t numBytes);

struct __attribute__ ((packed, aligned(1))) WebSocketMessageHeader
{
  unsigned opcode : 4;
  unsigned rsv : 3;
  unsigned fin : 1;
  unsigned payloadLength : 7;
  unsigned mask : 1;
};

