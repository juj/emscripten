#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <emscripten.h>

double prevTime = -1.0;
int frame = 0;
bool blockerExecuted = false;

void final(void*) {
  assert(frame == 20);
#ifdef REPORT_RESULT
  REPORT_RESULT(0);
#endif
}

void looper() {
  if (blockerExecuted == false) {
#ifdef REPORT_RESULT
    REPORT_RESULT(1);
#endif
  }

  frame++;
  double curTime = emscripten_get_now();
  double timeSincePrevious = curTime - prevTime;
  prevTime = curTime;
  printf("frame: %d. dt: %g. absolute: %g\n", frame, timeSincePrevious, curTime);
  if (frame > 1 && timeSincePrevious <= 14.5) // should be 16, but browser jitter
  {
    printf("Abort: main loop tick was called too quickly (%f ms > 16) after the previous frame!\n", timeSincePrevious);
#ifdef REPORT_RESULT
    REPORT_RESULT(1);
#endif
    emscripten_cancel_main_loop();
    exit(0);
  }

  if (frame == 20) {
    emscripten_cancel_main_loop();
    emscripten_async_call(final, (void*)0, 1000);
  }
}

void main_loop_blocker(void*) {
  blockerExecuted = true;
  emscripten_pause_main_loop();
  emscripten_resume_main_loop();
}

#include <emscripten/pthread_proxy_main.h>
int emscripten_main(int argc, char **argv) {
  prevTime = emscripten_get_now();
  emscripten_push_uncounted_main_loop_blocker(main_loop_blocker, NULL);
  emscripten_set_main_loop(looper, 60, 1);
  return 0;
}
