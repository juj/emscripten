#ifdef __EMSCRIPTEN_PTHREADS__

#include <emscripten/threading.h>
#include <emscripten.h>
#include <string.h>
#include <stdlib.h>

#include "webgl1.h"
#include "webgl2.h"

extern EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_do_create_context(const char *target, const EmscriptenWebGLContextAttributes *attributes);
extern EMSCRIPTEN_RESULT emscripten_webgl_do_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);
extern EMSCRIPTEN_RESULT emscripten_webgl_do_commit_frame(void);
extern EM_BOOL emscripten_supports_offscreencanvas(void);
extern EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_do_get_current_context(void);

pthread_key_t currentActiveWebGLContext;
pthread_key_t currentThreadOwnsItsWebGLContext;
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

static void *memdup(const void *ptr, size_t sz)
{
  void *dup = malloc(sz);
  memcpy(dup, ptr, sz);
  return dup;
}

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
//VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glBufferData, GLenum, GLsizeiptr, const void *, GLenum);
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glBufferData(target, size, data, usage);
  else
  {
    if (size < 256*1024) // run small buffer sizes asynchronously by copying
    {
      void *ptr = memdup(data, size);
      emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIII, &emscripten_glBufferData, ptr, target, size, ptr, usage);
    }
    else // larger ones run synchronously
    {
      emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VIIII, &emscripten_glBufferData, target, size, data, usage);
    }
  }
}

//VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glBufferSubData, GLenum, GLintptr, GLsizeiptr, const void *);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glBufferSubData(target, offset, size, data);
  else
  {
    if (size < 256*1024) // run small buffer sizes asynchronously by copying
    {
      void *ptr = memdup(data, size);
      emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIII, &emscripten_glBufferSubData, ptr, target, offset, size, ptr);
    }
    else // larger ones run synchronously
    {
      emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VIIII, &emscripten_glBufferSubData, target, offset, size, data);
    }
  }
}

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
// TODO: The following #define FULL_ES2 does not yet exist, we'll need to compile this file twice, for FULL_ES2 mode and without
#if FULL_ES2
VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glDrawElements, GLenum, GLsizei, GLenum, const void *);
#else
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glDrawElements, GLenum, GLsizei, GLenum, const void *);
#endif
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

static ssize_t ImageSize(int width, int height, GLenum format, GLenum type)
{
  int numChannels;
  switch(format)
  {
    case GL_ALPHA: case GL_LUMINANCE: case GL_DEPTH_COMPONENT: case GL_RED: case GL_RED_INTEGER: numChannels = 1; break;
    case GL_RG: case GL_RG_INTEGER: numChannels = 2; break;
    case GL_RGB: case 0x8C40/*GL_SRGB_EXT*/: case GL_RGB_INTEGER: numChannels = 3; break;
    case GL_RGBA: case 0x8C42/*GL_SRGB_ALPHA_EXT*/: case GL_RGBA_INTEGER: numChannels = 4; break;
    default: return -1;
  }
  int sizePerPixel;
  switch(type)
  {
    case GL_UNSIGNED_BYTE: case GL_BYTE: sizePerPixel = numChannels; break;
    case GL_UNSIGNED_SHORT: case 0x8D61/*GL_HALF_FLOAT_OES*/: case GL_HALF_FLOAT: case GL_SHORT: case GL_UNSIGNED_SHORT_5_6_5: case GL_UNSIGNED_SHORT_4_4_4_4: case GL_UNSIGNED_SHORT_5_5_5_1: sizePerPixel = numChannels*2; break;
    case GL_UNSIGNED_INT: case GL_FLOAT: case GL_INT: case GL_UNSIGNED_INT_5_9_9_9_REV: case GL_UNSIGNED_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_10F_11F_11F_REV: case GL_UNSIGNED_INT_24_8: sizePerPixel = numChannels*4; break;
    default: return -1;
  }
  return width*height*sizePerPixel;
}

//VOID_SYNC_GL_FUNCTION_9(EM_FUNC_SIG_VIIIIIIIII, void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
  else
  {
    ssize_t sz = ImageSize(width, height, format, type);
    if (!pixels || (sz >= 0 && sz < 256*1024)) // run small buffer sizes asynchronously by copying
    {
      void *ptr = pixels ? memdup(pixels, sz) : 0;
      emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIIIIIIII, &emscripten_glTexImage2D, ptr, target, level, internalformat, width, height, border, format, type, ptr);
    }
    else // larger ones run synchronously
    {
      emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VIIIIIIIII, &emscripten_glTexImage2D, target, level, internalformat, width, height, border, format, type, pixels);
    }
  }
}

ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIIF, void, glTexParameterf, GLenum, GLenum, GLfloat);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glTexParameterfv, GLenum, GLenum, const GLfloat *);
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glTexParameteri, GLenum, GLenum, GLint);
VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glTexParameteriv, GLenum, GLenum, const GLint *);
//VOID_SYNC_GL_FUNCTION_9(EM_FUNC_SIG_VIIIIIIIII, void, glTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
  else
  {
    ssize_t sz = ImageSize(width, height, format, type);
    if (!pixels || (sz >= 0 && sz < 256*1024)) // run small buffer sizes asynchronously by copying
    {
      void *ptr = pixels ? memdup(pixels, sz) : 0;
      emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIIIIIIII, &emscripten_glTexSubImage2D, ptr, target, level, xoffset, yoffset, width, height, format, type, ptr);
    }
    else // larger ones run synchronously
    {
      emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VIIIIIIIII, &emscripten_glTexSubImage2D, target, level, xoffset, yoffset, width, height, format, type, pixels);
    }
  }
}

ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VIF, void, glUniform1f, GLint, GLfloat);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform1fv, GLint, GLsizei, const GLfloat *);
void glUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform1fv(location, count, value);
  else
  {
    void *ptr = memdup(value, sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform1fv, ptr, location, count, (GLfloat*)ptr);
  }
}

ASYNC_GL_FUNCTION_2(EM_FUNC_SIG_VII, void, glUniform1i, GLint, GLint);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform1iv, GLint, GLsizei, const GLint *);
void glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform1iv(location, count, value);
  else
  {
    void *ptr = memdup(value, sizeof(GLint)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform1iv, ptr, location, count, (GLint*)ptr);
  }
}
ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIFF, void, glUniform2f, GLint, GLfloat, GLfloat);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform2fv, GLint, GLsizei, const GLfloat *);
void glUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform2fv(location, count, value);
  else
  {
    void *ptr = memdup(value, 2*sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform2fv, ptr, location, count, (GLfloat*)ptr);
  }
}

ASYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform2i, GLint, GLint, GLint);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform2iv, GLint, GLsizei, const GLint *);
void glUniform2iv(GLint location, GLsizei count, const GLint *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform2iv(location, count, value);
  else
  {
    void *ptr = memdup(value, 2*sizeof(GLint)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform2iv, ptr, location, count, (GLint*)ptr);
  }
}
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIFFF, void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform3fv, GLint, GLsizei, const GLfloat *);
void glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform3fv(location, count, value);
  else
  {
    void *ptr = memdup(value, 3*sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform3fv, ptr, location, count, (GLfloat*)ptr);
  }
}

ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniform3i, GLint, GLint, GLint, GLint);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform3iv, GLint, GLsizei, const GLint *);
void glUniform3iv(GLint location, GLsizei count, const GLint *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform3iv(location, count, value);
  else
  {
    void *ptr = memdup(value, 3*sizeof(GLint)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform3iv, ptr, location, count, (GLint*)ptr);
  }
}
ASYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIFFFF, void, glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform4fv, GLint, GLsizei, const GLfloat *);
void glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform4fv(location, count, value);
  else
  {
    void *ptr = memdup(value, 4*sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform4fv, ptr, location, count, (GLfloat*)ptr);
  }
}

ASYNC_GL_FUNCTION_5(EM_FUNC_SIG_VIIIII, void, glUniform4i, GLint, GLint, GLint, GLint, GLint);
//VOID_SYNC_GL_FUNCTION_3(EM_FUNC_SIG_VIII, void, glUniform4iv, GLint, GLsizei, const GLint *);
void glUniform4iv(GLint location, GLsizei count, const GLint *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniform4iv(location, count, value);
  else
  {
    void *ptr = memdup(value, 4*sizeof(GLint)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIII, &emscripten_glUniform4iv, ptr, location, count, (GLint*)ptr);
  }
}

//VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniformMatrix2fv, GLint, GLsizei, GLboolean, const GLfloat *);
void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniformMatrix2fv(location, count, transpose, value);
  else
  {
    void *ptr = memdup(value, 2*2*sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIII, &emscripten_glUniformMatrix2fv, ptr, location, count, transpose, (GLfloat*)ptr);
  }
}

//VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniformMatrix3fv, GLint, GLsizei, GLboolean, const GLfloat *);
void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniformMatrix3fv(location, count, transpose, value);
  else
  {
    void *ptr = memdup(value, 3*3*sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIII, &emscripten_glUniformMatrix3fv, ptr, location, count, transpose, (GLfloat*)ptr);
  }
}

//VOID_SYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat *);
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
  if (pthread_getspecific(currentThreadOwnsItsWebGLContext))
    emscripten_glUniformMatrix4fv(location, count, transpose, value);
  else
  {
    void *ptr = memdup(value, 4*4*sizeof(GLfloat)*count);
    emscripten_async_queue_on_thread(*(void**)(pthread_getspecific(currentActiveWebGLContext) + 4), EM_FUNC_SIG_VIIII, &emscripten_glUniformMatrix4fv, ptr, location, count, transpose, (GLfloat*)ptr);
  }
}
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

// TODO: The following #define FULL_ES2 does not yet exist, we'll need to compile this file twice, for FULL_ES2 mode and without
#if FULL_ES2
VOID_SYNC_GL_FUNCTION_6(EM_FUNC_SIG_VIIIIII, void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
#else
ASYNC_GL_FUNCTION_6(EM_FUNC_SIG_VIIIIII, void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
#endif
ASYNC_GL_FUNCTION_4(EM_FUNC_SIG_VIIII, void, glViewport, GLint, GLint, GLsizei, GLsizei);

#endif // ~__EMSCRIPTEN_PTHREADS__
