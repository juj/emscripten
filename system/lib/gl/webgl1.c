#include "webgl1.h"

#include <emscripten.h>

#ifdef __EMSCRIPTEN_PTHREADS__

#include <emscripten/threading.h>

extern EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_do_create_context(const char *target, const EmscriptenWebGLContextAttributes *attributes);
extern EMSCRIPTEN_RESULT emscripten_webgl_do_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);
extern EMSCRIPTEN_RESULT emscripten_webgl_do_commit_frame(void);
extern EM_BOOL emscripten_supports_offscreencanvas(void);
extern EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_do_get_current_context(void);

static pthread_key_t currentActiveWebGLContext;
static pthread_key_t currentThreadOwnsItsWebGLContext;
static pthread_once_t tlsInit = PTHREAD_ONCE_INIT;

static void InitWebGLTls()
{
  pthread_key_create(&currentActiveWebGLContext, NULL);
  pthread_key_create(&currentThreadOwnsItsWebGLContext, NULL);
}

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_create_context(const char *target, const EmscriptenWebGLContextAttributes *attributes)
{
  if (!attributes)
  {
    EM_ASM(console.error('emscripten_webgl_create_context: attributes pointer is null!'));
    return 0;
  }
  pthread_once(&tlsInit, InitWebGLTls);

  if (attributes->proxyContextToMainThread == EMSCRIPTEN_WEBGL_CONTEXT_PROXY_ALWAYS ||
    (attributes->proxyContextToMainThread == EMSCRIPTEN_WEBGL_CONTEXT_PROXY_FALLBACK && !emscripten_supports_offscreencanvas()))
  {
    EmscriptenWebGLContextAttributes attrs = *attributes;
    attrs.renderViaOffscreenBackBuffer = EM_TRUE;
    return (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_III, &emscripten_webgl_do_create_context, target, &attrs);
  }
  else
  {
    return emscripten_webgl_do_create_context(target, attributes);
  }
}

EMSCRIPTEN_RESULT emscripten_webgl_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context)
{
  void *owningThread = *(void**)(context + 4);
  if (owningThread == pthread_self())
  {
    EMSCRIPTEN_RESULT r = emscripten_webgl_do_make_context_current(context);
    if (r == EMSCRIPTEN_RESULT_SUCCESS)
    {
      pthread_setspecific(currentActiveWebGLContext, (void*)context);
      pthread_setspecific(currentThreadOwnsItsWebGLContext, (void*)1);
      EM_ASM({
        GL.currentContext = $0;
        GLctxIsOnParentThread = false;
      }, context);
    }
    return r;
  }
  else
  {
    EMSCRIPTEN_RESULT r = emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_II, &emscripten_webgl_do_make_context_current, context);
    if (r == EMSCRIPTEN_RESULT_SUCCESS)
    {
      pthread_setspecific(currentActiveWebGLContext, (void*)context);
      pthread_setspecific(currentThreadOwnsItsWebGLContext, (void*)0);
      EM_ASM({
        GL.currentContext = $0;
        GLctxIsOnParentThread = true;
      }, context);
    }
    return r;
  }
}

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_get_current_context(void)
{
  return (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)pthread_getspecific(currentActiveWebGLContext);
//  return (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_I, &emscripten_webgl_do_get_current_context);
}

EMSCRIPTEN_RESULT EMSCRIPTEN_KEEPALIVE emscripten_webgl_commit_frame(void)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    return emscripten_webgl_do_commit_frame();
  else
    return (EMSCRIPTEN_RESULT)emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_I, &emscripten_webgl_do_commit_frame);
}

#define ASYNC_GL_FUNCTION_0(sig, ret, functionName) ret functionName(void) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName); }
#define ASYNC_GL_FUNCTION_1(sig, ret, functionName, t0) ret functionName(t0 p0) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0); }
#define ASYNC_GL_FUNCTION_2(sig, ret, functionName, t0, t1) ret functionName(t0 p0, t1 p1) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1); }
#define ASYNC_GL_FUNCTION_3(sig, ret, functionName, t0, t1, t2) ret functionName(t0 p0, t1 p1, t2 p2) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2); }
#define ASYNC_GL_FUNCTION_4(sig, ret, functionName, t0, t1, t2, t3) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3); }
#define ASYNC_GL_FUNCTION_5(sig, ret, functionName, t0, t1, t2, t3, t4) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4); }
#define ASYNC_GL_FUNCTION_6(sig, ret, functionName, t0, t1, t2, t3, t4, t5) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5); }
#define ASYNC_GL_FUNCTION_7(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6); }
#define ASYNC_GL_FUNCTION_8(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6, t7) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6, p7); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6, p7); }
#define ASYNC_GL_FUNCTION_9(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6, t7, t8) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6, p7, p8); else emscripten_async_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6, p7, p8); }

#define RET_SYNC_GL_FUNCTION_0(sig, ret, functionName) ret functionName(void) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName); }
#define RET_SYNC_GL_FUNCTION_1(sig, ret, functionName, t0) ret functionName(t0 p0) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0); }
#define RET_SYNC_GL_FUNCTION_2(sig, ret, functionName, t0, t1) ret functionName(t0 p0, t1 p1) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1); }
#define RET_SYNC_GL_FUNCTION_3(sig, ret, functionName, t0, t1, t2) ret functionName(t0 p0, t1 p1, t2 p2) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2); }
#define RET_SYNC_GL_FUNCTION_4(sig, ret, functionName, t0, t1, t2, t3) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2, p3); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3); }
#define RET_SYNC_GL_FUNCTION_5(sig, ret, functionName, t0, t1, t2, t3, t4) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2, p3, p4); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4); }
#define RET_SYNC_GL_FUNCTION_6(sig, ret, functionName, t0, t1, t2, t3, t4, t5) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2, p3, p4, p5); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5); }
#define RET_SYNC_GL_FUNCTION_7(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6); }
#define RET_SYNC_GL_FUNCTION_8(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6, t7) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6, p7); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6, p7); }
#define RET_SYNC_GL_FUNCTION_9(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6, t7, t8) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) return emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6, p7, p8); else return (ret)emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6, p7, p8); }

#define VOID_SYNC_GL_FUNCTION_0(sig, ret, functionName) ret functionName(void) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName); }
#define VOID_SYNC_GL_FUNCTION_1(sig, ret, functionName, t0) ret functionName(t0 p0) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0); }
#define VOID_SYNC_GL_FUNCTION_2(sig, ret, functionName, t0, t1) ret functionName(t0 p0, t1 p1) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1); }
#define VOID_SYNC_GL_FUNCTION_3(sig, ret, functionName, t0, t1, t2) ret functionName(t0 p0, t1 p1, t2 p2) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2); }
#define VOID_SYNC_GL_FUNCTION_4(sig, ret, functionName, t0, t1, t2, t3) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3); }
#define VOID_SYNC_GL_FUNCTION_5(sig, ret, functionName, t0, t1, t2, t3, t4) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4); }
#define VOID_SYNC_GL_FUNCTION_6(sig, ret, functionName, t0, t1, t2, t3, t4, t5) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5); }
#define VOID_SYNC_GL_FUNCTION_7(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6); }
#define VOID_SYNC_GL_FUNCTION_8(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6, t7) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6, p7); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6, p7); }
#define VOID_SYNC_GL_FUNCTION_9(sig, ret, functionName, t0, t1, t2, t3, t4, t5, t6, t7, t8) ret functionName(t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) { if (pthread_getspecific(currentThreadOwnsItsWebGLContext)) emscripten_##functionName(p0, p1, p2, p3, p4, p5, p6, p7, p8); else emscripten_sync_run_in_main_runtime_thread(sig, &emscripten_##functionName, p0, p1, p2, p3, p4, p5, p6, p7, p8); }

ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glActiveTexture, GLenum);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glAttachShader, GLuint, GLuint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glBindAttribLocation, GLuint, GLuint, const GLchar*);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glBindBuffer, GLenum, GLuint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glBindFramebuffer, GLenum, GLuint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glBindRenderbuffer, GLenum, GLuint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glBindTexture, GLenum, GLuint);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VFFFF, void, glBlendColor, GLfloat, GLfloat, GLfloat, GLfloat);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glBlendEquation, GLenum);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glBlendEquationSeparate, GLenum, GLenum);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glBlendFunc, GLenum, GLenum);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glBlendFuncSeparate, GLenum, GLenum, GLenum, GLenum);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glBufferData, GLenum, GLsizeiptr, const void *, GLenum);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glBufferSubData, GLenum, GLintptr, GLsizeiptr, const void *);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLenum, glCheckFramebufferStatus, GLenum);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glClear, GLbitfield);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VFFFF, void, glClearColor, GLfloat, GLfloat, GLfloat, GLfloat);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VF, void, glClearDepthf, GLfloat);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glClearStencil, GLint);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glColorMask, GLboolean, GLboolean, GLboolean, GLboolean);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glCompileShader, GLuint);
VOID_SYNC_GL_FUNCTION_8(EM_FUNC_SIG_VIIIIIIII, void, glCompressedTexImage2D, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
VOID_SYNC_GL_FUNCTION_9(EM_FUNC_SIG_VIIIIIIIII, void, glCompressedTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *);
ASYNC_GL_FUNCTION_8(EM_FUNC_SIG_VIIIIIIII, void, glCopyTexImage2D, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
ASYNC_GL_FUNCTION_8(EM_FUNC_SIG_VIIIIIIII, void, glCopyTexSubImage2D, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
RET_SYNC_GL_FUNCTION_0(EM_FUNC_SIG_I, GLuint, glCreateProgram);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLuint, glCreateShader, GLenum);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glCullFace, GLenum);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glDeleteBuffers, GLsizei, const GLuint *);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glDeleteFramebuffers, GLsizei, const GLuint *);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glDeleteProgram, GLuint);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glDeleteRenderbuffers, GLsizei, const GLuint *);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glDeleteShader, GLuint);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glDeleteTextures, GLsizei, const GLuint *);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glDepthFunc, GLenum);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glDepthMask, GLboolean);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VFF, void, glDepthRangef, GLfloat, GLfloat);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glDetachShader, GLuint, GLuint);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glDisable, GLenum);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glDisableVertexAttribArray, GLuint);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glDrawArrays, GLenum, GLint, GLsizei);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glDrawElements, GLenum, GLsizei, GLenum, const void *); // TODO: THIS IS ASYNC IF FULL_ES2=0
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glEnable, GLenum);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glEnableVertexAttribArray, GLuint);
VOID_SYNC_GL_FUNCTION_0(EM_FUNC_SIG_V, void, glFinish);
VOID_SYNC_GL_FUNCTION_0(EM_FUNC_SIG_V, void, glFlush); // TODO: THIS COULD POTENTIALLY BE ASYNC
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glFramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint);
ASYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIIIII, void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glFrontFace, GLenum);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGenBuffers, GLsizei, GLuint *);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glGenerateMipmap, GLenum);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGenFramebuffers, GLsizei, GLuint *);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGenRenderbuffers, GLsizei, GLuint *);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGenTextures, GLsizei, GLuint *);
VOID_SYNC_GL_FUNCTION_7(EM_FUNC_SIG_VIIIIIII, void, glGetActiveAttrib, GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *);
VOID_SYNC_GL_FUNCTION_7(EM_FUNC_SIG_VIIIIIII, void, glGetActiveUniform, GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glGetAttachedShaders, GLuint, GLsizei, GLsizei *, GLuint *);
RET_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_III, GLint, glGetAttribLocation, GLuint, const GLchar *);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGetBooleanv, GLenum, GLboolean *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetBufferParameteriv, GLenum, GLenum, GLint *);
RET_SYNC_GL_FUNCTION_0(EM_FUNC_SIG_I, GLenum, glGetError);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGetFloatv, GLenum, GLfloat *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glGetFramebufferAttachmentParameteriv, GLenum, GLenum, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glGetIntegerv, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetProgramiv, GLuint, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei *, GLchar *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetRenderbufferParameteriv, GLenum, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetShaderiv, GLuint, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei *, GLchar *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glGetShaderPrecisionFormat, GLenum, GLenum, GLint *, GLint *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glGetShaderSource, GLuint, GLsizei, GLsizei *, GLchar *);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, const GLubyte *, glGetString, GLenum);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetTexParameterfv, GLenum, GLenum, GLfloat *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetTexParameteriv, GLenum, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetUniformfv, GLuint, GLint, GLfloat *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetUniformiv, GLuint, GLint, GLint *);
RET_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_III, GLint, glGetUniformLocation, GLuint, const GLchar *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetVertexAttribfv, GLuint, GLenum, GLfloat *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetVertexAttribiv, GLuint, GLenum, GLint *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glGetVertexAttribPointerv, GLuint, GLenum, void **);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glHint, GLenum, GLenum);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsBuffer, GLuint);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsEnabled, GLenum);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsFramebuffer, GLuint);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsProgram, GLuint);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsRenderbuffer, GLuint);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsShader, GLuint);
RET_SYNC_GL_FUNCTION_1(EM_FUNC_SIG_II, GLboolean, glIsTexture, GLuint);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VF, void, glLineWidth, GLfloat);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glLinkProgram, GLuint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glPixelStorei, GLenum, GLint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VFF, void, glPolygonOffset, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_7(EM_FUNC_SIG_VIIIIIII, void, glReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
ASYNC_GL_FUNCTION_0(EM_FUNC_SIG_V, void, glReleaseShaderCompiler);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glRenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glSampleCoverage, GLfloat, GLboolean);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glScissor, GLint, GLint, GLsizei, GLsizei);
VOID_SYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIIIII, void, glShaderBinary, GLsizei, const GLuint *, GLenum, const void *, GLsizei);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glShaderSource, GLuint, GLsizei, const GLchar *const*, const GLint *);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glStencilFunc, GLenum, GLint, GLuint);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glStencilFuncSeparate, GLenum, GLenum, GLint, GLuint);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glStencilMask, GLuint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glStencilMaskSeparate, GLenum, GLuint);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glStencilOp, GLenum, GLenum, GLenum);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glStencilOpSeparate, GLenum, GLenum, GLenum, GLenum);
VOID_SYNC_GL_FUNCTION_9(EM_FUNC_SIG_VIIIIIIIII, void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIIF, void, glTexParameterf, GLenum, GLenum, GLfloat);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glTexParameterfv, GLenum, GLenum, const GLfloat *);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glTexParameteri, GLenum, GLenum, GLint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glTexParameteriv, GLenum, GLenum, const GLint *);
VOID_SYNC_GL_FUNCTION_9(EM_FUNC_SIG_VIIIIIIIII, void, glTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VIF, void, glUniform1f, GLint, GLfloat);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform1fv, GLint, GLsizei, const GLfloat *);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glUniform1i, GLint, GLint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform1iv, GLint, GLsizei, const GLint *);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIFF, void, glUniform2f, GLint, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform2fv, GLint, GLsizei, const GLfloat *);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform2i, GLint, GLint, GLint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform2iv, GLint, GLsizei, const GLint *);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIFFF, void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform3fv, GLint, GLsizei, const GLfloat *);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniform3i, GLint, GLint, GLint, GLint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform3iv, GLint, GLsizei, const GLint *);
ASYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIFFFF, void, glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform4fv, GLint, GLsizei, const GLfloat *);
ASYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIIIII, void, glUniform4i, GLint, GLint, GLint, GLint, GLint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform4iv, GLint, GLsizei, const GLint *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniformMatrix2fv, GLint, GLsizei, GLboolean, const GLfloat *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniformMatrix3fv, GLint, GLsizei, GLboolean, const GLfloat *);
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat *);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glUseProgram, GLuint);
ASYNC_GL_FUNCTION_1(EM_FUNC_SIG_VI, void, glValidateProgram, GLuint);
ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VIF, void, glVertexAttrib1f, GLuint, GLfloat);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glVertexAttrib1fv, GLuint, const GLfloat *);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIFF, void, glVertexAttrib2f, GLuint, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glVertexAttrib2fv, GLuint, const GLfloat *);
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIFFF, void, glVertexAttrib3f, GLuint, GLfloat, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glVertexAttrib3fv, GLuint, const GLfloat *);
ASYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIFFFF, void, glVertexAttrib4f, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
VOID_SYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glVertexAttrib4fv, GLuint, const GLfloat *);
VOID_SYNC_GL_FUNCTION_6(EM_FUNC_SIG_VIIIIII, void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *); // TODO: THIS CAN BE ASYNC WHEN FULL_ES2=0
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glViewport, GLint, GLint, GLsizei, GLsizei);

#endif // ~__EMSCRIPTEN_PTHREADS__
