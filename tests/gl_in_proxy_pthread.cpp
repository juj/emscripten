#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <assert.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/threading.h>
#include <bits/errno.h>
#include <stdlib.h>

#ifdef TEST_OFFSCREEN_CANVAS
#define EMSCRIPTEN_PTHREAD_TRANSFERRED_CANVASES "#canvas"
#endif

#include <emscripten/pthread_proxy_main.h>
int emscripten_main(int argc, char **argv)
{
  EmscriptenWebGLContextAttributes attr;
  emscripten_webgl_init_context_attributes(&attr);
  attr.explicitSwapControl = EM_TRUE;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attr);
  printf("Created context with handle %u\n", (unsigned int)ctx);
  emscripten_webgl_make_context_current(ctx);

  double color = 0;
  for(int i = 0; i < 100; ++i)
  {
    color += 0.01;
    glClearColor(color, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EMSCRIPTEN_RESULT r = emscripten_webgl_commit_frame();
    assert(r == EMSCRIPTEN_RESULT_SUCCESS);

    double now = emscripten_get_now();
    while(emscripten_get_now() - now < 16) /*no-op*/;
  }

  emscripten_webgl_make_context_current(0);
  emscripten_webgl_destroy_context(ctx);
  printf("Thread quit\n");
#ifdef REPORT_RESULT
  REPORT_RESULT(1);
#endif
  exit(0);
}
