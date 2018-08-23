#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <vector>

#include "sha1.h"
#include "websocket_to_posix_proxy.h"

static const unsigned char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void base64_encode(void *dst, const void *src, size_t len)
{
  assert(dst != src);
  unsigned int *d = (unsigned int *)dst;
  const unsigned char *s = (const unsigned char*)src;
  const unsigned char *end = s + len;
  while(s < end)
  {
    uint32_t e = *s++ << 16;
    if (s < end) e |= *s++ << 8;
    if (s < end) e |= *s++;
    *d++ = b64[e >> 18] | (b64[(e >> 12) & 0x3F] << 8) | (b64[(e >> 6) & 0x3F] << 16) | (b64[e & 0x3F] << 24);
  }
  for (int i = 0; i < (3 - (len % 3)) % 3; i++) ((char *)d)[-1-i] = '=';
}

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int GetHttpHeader(const char *headers, const char *header, char *out)
{
  const char *pos = strstr(headers, header);
  if (!pos) return 0;
  pos += strlen(header);
  const char *end = pos;
  while(*end != '\r') ++end;
  memcpy(out, pos, end-pos);
  out[end-pos] = '\0';
  return end-pos;
}

void SendHandshake(int fd, const char *request)
{
  char key[128];
  GetHttpHeader(request, "Sec-WebSocket-Key: ", key);
  const char webSocketGlobalGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  strcat(key, webSocketGlobalGuid);

  char sha1[21];
  printf("hashing key: \"%s\"\n", key);
  SHA1(sha1, key, strlen(key));

  char handshakeMsg[] = 
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: 0000000000000000000000000000\r\n"
    "\r\n";

  base64_encode(strstr(handshakeMsg, "Sec-WebSocket-Accept: ") + strlen("Sec-WebSocket-Accept: "), sha1, 20);

  int err = send(fd, handshakeMsg, strlen(handshakeMsg), 0);
  if (err < 0) on_error("Client write failed\n");
  printf("Sent handshake:\n%s\n", handshakeMsg);
}

bool WebSocketHasFullHeader(uint8_t *data, size_t obtainedNumBytes)
{
  if (obtainedNumBytes < 2) return false;
  size_t expectedNumBytes = 2;
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
  if (header->mask) expectedNumBytes += 4;
  switch(header->payloadLength)
  {
    case 127: return expectedNumBytes += 8; break;
    case 126: return expectedNumBytes += 2; break;
    default: break;
  }
  return obtainedNumBytes >= expectedNumBytes;
}

size_t WebSocketFullMessageSize(uint8_t *data, size_t obtainedNumBytes)
{
  assert(WebSocketHasFullHeader(data, obtainedNumBytes));

  size_t expectedNumBytes = 2;
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
  if (header->mask) expectedNumBytes += 4;
  switch(header->payloadLength)
  {
    case 127: return expectedNumBytes += 8 + ntoh64(*(uint64_t*)(data+2)); break;
    case 126: return expectedNumBytes += 2 + ntohs(*(uint16_t*)(data+2)); break;
    default: expectedNumBytes += header->payloadLength; break;
  }
  return expectedNumBytes;
}

// Tests the structure integrity of the websocket message length.
bool WebSocketValidateMessageSize(uint8_t *data, size_t obtainedNumBytes)
{
  size_t expectedNumBytes = WebSocketFullMessageSize(data, obtainedNumBytes);

  if (expectedNumBytes != obtainedNumBytes)
  {
    printf("Corrupt WebSocket message size! (got %zd bytes, expected %zd bytes)\n", obtainedNumBytes, expectedNumBytes);
    printf("Received data:");
    for(int i = 0; i < obtainedNumBytes; ++i)
      printf(" %02X", data[i]);
    printf("\n");
  }
  return expectedNumBytes == obtainedNumBytes;
}

uint64_t WebSocketMessagePayloadLength(uint8_t *data, size_t numBytes)
{
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
  switch(header->payloadLength)
  {
    case 127: return ntoh64(*(uint64_t*)(data+2));
    case 126: return ntohs(*(uint16_t*)(data+2));
    default: return header->payloadLength;
  }
}

uint32_t WebSocketMessageMaskingKey(uint8_t *data, size_t numBytes)
{
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
  if (!header->mask) return 0;
  switch(header->payloadLength)
  {
    case 127: return *(uint32_t*)(data+10);
    case 126: return *(uint32_t*)(data+4);
    default: return *(uint32_t*)(data+2);
  }
}

uint8_t *WebSocketMessageData(uint8_t *data, size_t numBytes)
{
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
  data += 2; // Two bytes of fixed size header
  if (header->mask) data += 4; // If there is a masking key present in the header, that takes up 4 bytes
  switch(header->payloadLength)
  {
    case 127: return data + 8; // 64-bit length
    case 126: return data + 2; // 16-bit length
    default: return data; // 7-bit length that was embedded in fixed size header.
  }
}

void CloseWebSocket(int client_fd)
{
  printf("Closing WebSocket connection %d\n", client_fd);
  shutdown(client_fd, SHUT_RDWR);
}

const char *WebSocketOpcodeToString(int opcode)
{
  static const char *opcodes[] = { "continuation frame (0x0)", "text frame (0x1)", "binary frame (0x2)", "reserved(0x3)", "reserved(0x4)", "reserved(0x5)",
    "reserved(0x6)", "reserved(0x7)", "connection close (0x8)", "ping (0x9)", "pong (0xA)", "reserved(0xB)", "reserved(0xC)", "reserved(0xD)", "reserved(0xE)", "reserved(0xF)" };
  return opcodes[opcode];
}

void DumpWebSocketMessage(uint8_t *data, size_t numBytes)
{
  bool goodMessageSize = WebSocketValidateMessageSize(data, numBytes);
  if (!goodMessageSize)
    return;

  WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
  uint64_t payloadLength = WebSocketMessagePayloadLength(data, numBytes);
  uint8_t *payload = WebSocketMessageData(data, numBytes);

  printf("Received: FIN: %d, opcode: %s, mask: 0x%08X, payload length: %llu bytes, unmasked payload:", header->fin, WebSocketOpcodeToString(header->opcode),
    WebSocketMessageMaskingKey(data, numBytes), payloadLength);
  for(uint64_t i = 0; i < payloadLength; ++i)
  {
    if (i%16 == 0) printf("\n");
    if (i%8==0) printf(" ");
    printf(" %02X", payload[i]);
    if (i >= 63 && payloadLength > 64)
    {
      printf("\n   ... (%llu more bytes)", payloadLength-i);
      break;
    }
  }
  printf("\n");
}

int main (int argc, char *argv[]) {
  if (argc < 2) on_error("Usage: %s [port]\n", argv[0]);

  int port = atoi(argv[1]);

  int server_fd, client_fd, err;
  struct sockaddr_in server, client;
  char buf[BUFFER_SIZE];

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) on_error("Could not create socket\n");

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

  err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
  if (err < 0) on_error("Could not bind socket\n");

  err = listen(server_fd, 128);
  if (err < 0) on_error("Could not listen on socket\n");

  printf("Server is listening on %d\n", port);

  while (1) {
    socklen_t client_len = sizeof(client);
    client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

    if (client_fd < 0) on_error("Could not establish new connection\n");

    // Waiting for connection upgrade handshake
    int read = recv(client_fd, buf, BUFFER_SIZE, 0);

    if (!read) break; // done reading
    if (read < 0) on_error("Client read failed\n");

    printf("Received connection\n");
#if 0
    printf("Received:");
    for(int i = 0; i < read; ++i)
    {
      printf(" %02X", buf[i]);
    }
    printf("\n");
    printf("In text:\n%s\n", buf);
#endif
    SendHandshake(client_fd, buf);

//    printf("Handshake received, entering message loop:\n");

    std::vector<uint8_t> fragmentData;

    bool connectionAlive = true;
    while (connectionAlive) {
      int read = recv(client_fd, buf, BUFFER_SIZE, 0);

      if (!read) break; // done reading
      if (read < 0) on_error("Client read failed\n");

#if 0
      printf("Received:");
      for(int i = 0; i < read; ++i)
      {
        printf(" %02X", ((unsigned char*)buf)[i]);
      }
      printf("\n");
#endif

      fragmentData.insert(fragmentData.end(), buf, buf+read);
      bool hasFullHeader = WebSocketHasFullHeader(&fragmentData[0], fragmentData.size());
      if (!hasFullHeader) continue;
      size_t neededBytes = WebSocketFullMessageSize(&fragmentData[0], fragmentData.size());
      if (fragmentData.size() < neededBytes)
        continue;

      WebSocketMessageHeader *header = (WebSocketMessageHeader *)&fragmentData[0];
      uint64_t payloadLength = WebSocketMessagePayloadLength(&fragmentData[0], neededBytes);
      uint8_t *payload = WebSocketMessageData(&fragmentData[0], neededBytes);

      // Unmask payload
      if (header->mask)
        WebSocketMessageUnmaskPayload(payload, payloadLength, WebSocketMessageMaskingKey(&fragmentData[0], neededBytes));

      DumpWebSocketMessage(&fragmentData[0], neededBytes);

      switch(header->opcode)
      {
      case 0x02: /*binary message*/ ProcessWebSocketMessage(client_fd, payload, payloadLength); break;
      case 0x08: CloseWebSocket(client_fd); connectionAlive = false; break;
      default:
        printf("Unknown WebSocket opcode received %x!\n", header->opcode);
        break;
      }

      fragmentData.erase(fragmentData.begin(), fragmentData.begin() + neededBytes);
    }
  }

  return 0;
}