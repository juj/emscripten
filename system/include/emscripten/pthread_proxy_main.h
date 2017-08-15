/* pthread_proxy_main.h: Include <emscripten/pthread_proxy_main.h> and implement emscripten_main() to automatically proxy execution
  of the application to a web worker. This allows running synchronously blocking loops and operations in the application.
*/
#pragma once

int emscripten_main(int argc, char **argv);

#if __EMSCRIPTEN_PTHREADS__

#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct __main_args
{
  int argc;
  char **argv;
} __main_args;

void *__emscripten_thread_main(void *param)
{
  emscripten_register_main_runtime_thread_id((void*)pthread_self());
  emscripten_set_thread_name(pthread_self(), "Application main thread"); // This is the main runtime thread for the application.
  __main_args *args = (__main_args*)param;
  int ret = emscripten_main(args->argc, args->argv);
//  pthread_exit(ret);
  return 0;
}

static volatile __main_args args;

#ifndef EMSCRIPTEN_PTHREAD_STACK_SIZE
#define EMSCRIPTEN_PTHREAD_STACK_SIZE (128*1024)
#endif

int main(int argc, char **argv)
{
  emscripten_register_main_browser_thread_id((void*)pthread_self());

  if (emscripten_has_threading_support())
  {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, (EMSCRIPTEN_PTHREAD_STACK_SIZE));
#ifdef EMSCRIPTEN_PTHREAD_TRANSFERRED_CANVASES
    // If user has defined EMSCRIPTEN_PTHREAD_TRANSFERRED_CANVASES, then transfer those canvases over to the pthread.
    emscripten_pthread_attr_settransferredcanvases(&attr, (EMSCRIPTEN_PTHREAD_TRANSFERRED_CANVASES));
#else
    // Otherwise by default, transfer whatever is set to Module.canvas.
    char defaultCanvasID[32] = {};
    EM_ASM_INT({ if (Module['canvas']) stringToUTF8('#' + Module['canvas'].id, $0, 32); }, defaultCanvasID);
    if (defaultCanvasID[0]) emscripten_pthread_attr_settransferredcanvases(&attr, defaultCanvasID);
#endif
    args.argc = argc;
    args.argv = argv;
    pthread_t thread;
    printf("pthread_create!\n");
    int rc = pthread_create(&thread, &attr, __emscripten_thread_main, (void*)&args);
    pthread_attr_destroy(&attr);
    if (rc)
    {
      emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK | EM_LOG_JS_STACK | EM_LOG_DEMANGLE, "Failed to create pthread for main()!\n");
      // Proceed by running main() on the main browser thread as a fallback.
      emscripten_register_main_runtime_thread_id((void*)pthread_self());
      return emscripten_main(argc, argv);
    }
    EM_ASM(Module['noExitRuntime'] = true;);
    return 0;
  }
  else
  {
    emscripten_register_main_runtime_thread_id((void*)pthread_self());
    emscripten_log(EM_LOG_CONSOLE | EM_LOG_C_STACK | EM_LOG_JS_STACK | EM_LOG_DEMANGLE, "Multithreading not available, attempting to run on main thread.\n");
    return emscripten_main(argc, argv);
  }
}

#else

// Not building with multithreading enabled, just route directly to the Emscripten specific main function.
int main(int argc, char **argv)
{
  emscripten_register_main_browser_thread_id((void*)pthread_self());
  emscripten_register_main_runtime_thread_id((void*)pthread_self());
  return emscripten_main(argc, argv);
}

#endif // ~__EMSCRIPTEN_PTHREADS__
