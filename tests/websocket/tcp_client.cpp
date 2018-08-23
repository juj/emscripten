/*
    C ECHO client example using sockets
*/
#include <stdio.h> //printf
#include <stdlib.h>
#include <string.h>    //strlen
#include <sys/socket.h>    //socket
#include <arpa/inet.h> //inet_addr
#include <unistd.h>

#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/threading.h>
 
EMSCRIPTEN_WEBSOCKET_T bridgeSocket = 0;

extern "C" {
EMSCRIPTEN_WEBSOCKET_T emscripten_init_websocket_to_posix_socket_bridge(const char *bridgeUrl);
}

int main(int argc , char *argv[])
{
  bridgeSocket = emscripten_init_websocket_to_posix_socket_bridge("ws://localhost:8080");
  // Synchronously wait until connection has been established.
  uint16_t readyState = 0;
  do {
    emscripten_websocket_get_ready_state(bridgeSocket, &readyState);
    emscripten_thread_sleep(100);
  } while(readyState == 0);

  struct sockaddr_in server;
  char message[1000] , server_reply[2000];
   
  // Create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
  {
    printf("Could not create socket");
    exit(1);
  }
  printf("Socket created: %d\n", sock);

  server.sin_addr.s_addr = inet_addr("127.0.0.1");
  server.sin_family = AF_INET;
  server.sin_port = htons( 8888 );

  //Connect to remote server
  if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
  {
    perror("connect failed. Error");
    return 1;
  }
#if 1
   
  puts("Connected\n");
   
  //keep communicating with server
  for(int i = 0; i < 10; ++i)
  {
    printf("Enter message : ");

    strcpy(message, "hell");
//        char data[5] = "hell";
//        scanf("%s" , message);
     
    //Send some data
    if( send(sock , message , strlen(message) , 0) < 0)
    {
      puts("Send failed");
      return 1;
    }
     
    //Receive a reply from the server
    if( recv(sock , server_reply , 2000 , 0) < 0)
    {
      puts("recv failed");
      break;
    }
     
    puts("Server reply :");
    puts(server_reply);
  }

  close(sock);
#endif
  return 0;
}
