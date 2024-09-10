// This test verifies that if both the main thread and a pthread send() to
// the same socket in a synchronized fashion, that the sends do get correctly
// ordered in a sequential consistency fashion.

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <string.h>
#include <stdio.h>

#define SEND(msg) send(sock, msg, strlen(msg), 0)

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int sock;

EM_BOOL worker_work(double, void *) {
  pthread_mutex_lock(&lock);
  SEND("1");
  SEND("2");
  SEND("3\n");
  pthread_mutex_unlock(&lock);
  return EM_TRUE;
}

// Worker repeatedly sends a message "123"
void *worker_thread(void *) {
  emscripten_set_timeout_loop(worker_work, 0, 0);
  emscripten_exit_with_live_runtime();
  return 0;
}

// Main thread repeatedly sends a message "abc"
EM_BOOL main_thread(double, void *) {
  pthread_mutex_lock(&lock);

  static int num_writes = 10000;
  if (num_writes-- > 0) {
    // First write to socket
    SEND("a");
    emscripten_main_thread_process_queued_calls();
    SEND("b");
    emscripten_main_thread_process_queued_calls();
    SEND("c\n");
  }
  pthread_mutex_unlock(&lock);

  // And then read from the socket to see what we have received
  char buf[1024] = {};
  recv(sock, buf, sizeof(buf)-1, 0);
  if (strlen(buf) > 0) printf("%s", buf);
  return EM_TRUE;
}

void async_main(int, void*) {
  // Create a pthread that hammers messages to the socket.
  pthread_t thread;
  pthread_create(&thread, NULL, worker_thread, NULL);

  // Start a timeout loop that also hammers messages to the socket from the main thread.
  emscripten_set_timeout_loop(main_thread, 0, 0);
  emscripten_exit_with_live_runtime();
}

int main() {
  // Connect socket to a WebSocket echo server
  sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(8089)
  };
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  connect(sock, (sockaddr*)&addr, sizeof(addr));

  emscripten_set_socket_open_callback(0, async_main);
  return 0;
}
