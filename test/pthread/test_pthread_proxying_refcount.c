#include <assert.h>
#include <emscripten/console.h>
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/proxying.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>

int em_task_queue_zombie_count(void);

// Proxying queues accessed from the worker thread.
em_proxying_queue* queues[2];

_Atomic int should_execute = 0;
_Atomic int executed[2] = {};
_Atomic int processed = 0;

EMSCRIPTEN_KEEPALIVE
void register_processed(void) {
  processed++;
}

void set_flag(void* arg) { *(_Atomic int*)arg = 1; }

// Delay setting the flag until the next turn of the event loop so it can be set
// after the proxying queue is destroyed.
void task(void* arg) { emscripten_async_call(set_flag, arg, 0); }

void* execute_and_free_queue(void* arg) {
  // Wait until we are signaled to execute the queue.
  while (!should_execute) {
    sched_yield();
  }

  // Execute the proxied work then free the empty queues.
  for (int i = 0; i < 2; i++) {
    emscripten_proxy_execute_queue(queues[i]);
    em_proxying_queue_destroy(queues[i]);
  }

  // Exit with a live runtime so the queued work notification is received and we
  // try to execute the queue again, even though we already executed all its
  // work and we are now just waiting for the notifications to be received so we
  // can free it.
  emscripten_exit_with_live_runtime();
}

void nop(void* arg) {}

int main() {
  emscripten_console_log("start");
  for (int i = 0; i < 2; i++) {
    queues[i] = em_proxying_queue_create();
    assert(queues[i]);
  }

  // Create the worker and send it tasks.
  pthread_t worker;
  pthread_create(&worker, NULL, execute_and_free_queue, NULL);
  for (int i = 0; i < 2; i++) {
    emscripten_proxy_async(queues[i], worker, task, &executed[i]);
  }
  should_execute = 1;

  // Wait for the tasks to be executed. The queues will have been destroyed
  // after this.
  while (!executed[0] || !executed[1]) {
    sched_yield();
  }

  // Cull the zombies! (by forcing a new task queue to be allocated)
  em_proxying_queue* culler = em_proxying_queue_create();
  emscripten_proxy_async(culler, pthread_self(), nop, NULL);

  assert(em_task_queue_zombie_count() == 0);

  em_proxying_queue_destroy(culler);

  emscripten_console_log("done");
}
