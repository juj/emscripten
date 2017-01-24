#include <GLES2/gl2.h>
#include <GLES3/gl3.h>

static GLuint _GL_ARRAY_BUFFER_BINDING = 0;
static GLuint _GL_ELEMENT_ARRAY_BUFFER_BINDING = 0;

extern "C" {

GL_APICALL void GL_APIENTRY _emscripten_glBindBuffer(GLenum target, GLuint buffer);
GL_APICALL void GL_APIENTRY _emscripten_glVertexAttribDivisor(GLuint index, GLuint divisor);
GL_APICALL void GL_APIENTRY _emscripten_glEnableVertexAttribArray(GLuint index);
GL_APICALL void GL_APIENTRY _emscripten_glDisableVertexAttribArray(GLuint index);
GL_APICALL void GL_APIENTRY _emscripten_glUseProgram(GLuint program);
GL_APICALL void GL_APIENTRY _emscripten_glActiveTexture(GLenum texture);
GL_APICALL void GL_APIENTRY _emscripten_glDrawArrays(GLenum mode, GLint first, GLsizei count);
GL_APICALL void GL_APIENTRY _emscripten_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
GL_APICALL void GL_APIENTRY _emscripten_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices);
GL_APICALL void GL_APIENTRY _emscripten_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
GL_APICALL void GL_APIENTRY _emscripten_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);
GL_APICALL void GL_APIENTRY _emscripten_glDrawArraysIndirect(GLenum mode, const void *indirect);
GL_APICALL void GL_APIENTRY _emscripten_glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect);
GL_APICALL void GL_APIENTRY _emscripten_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);

GL_APICALL void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
  if (target == GL_ARRAY_BUFFER)
  {
    if (_GL_ARRAY_BUFFER_BINDING == buffer) return;
    _emscripten_glBindBuffer(GL_ARRAY_BUFFER, buffer);
    _GL_ARRAY_BUFFER_BINDING = buffer;
  }
  else if (target == GL_ELEMENT_ARRAY_BUFFER)
  {
    if (_GL_ELEMENT_ARRAY_BUFFER_BINDING == buffer) return;
    _emscripten_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    _GL_ELEMENT_ARRAY_BUFFER_BINDING = buffer;
  }
  else
  {
    _emscripten_glBindBuffer(target, buffer);
  }
}

#define _GL_MAX_VERTEX_ATTRIBS 64

static uint8_t _GL_VERTEX_ATTRIB_ARRAY_DIVISOR[_GL_MAX_VERTEX_ATTRIBS];

GL_APICALL void GL_APIENTRY glVertexAttribDivisor(GLuint index, GLuint divisor)
{
  if (_GL_VERTEX_ATTRIB_ARRAY_DIVISOR[index] == (uint8_t)divisor) return;
  _emscripten_glVertexAttribDivisor(index, divisor);
  _GL_VERTEX_ATTRIB_ARRAY_DIVISOR[index] = (uint8_t)divisor;
}

static uint32_t _GL_VERTEX_ATTRIB_ARRAY_ENABLED;
static uint32_t _GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED;
static uint32_t _GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED;

static inline void applyGlEnableVertexAttribArrays()
{
  _GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED &= ~_GL_VERTEX_ATTRIB_ARRAY_ENABLED;
  _GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED &= _GL_VERTEX_ATTRIB_ARRAY_ENABLED;

  _GL_VERTEX_ATTRIB_ARRAY_ENABLED |= _GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED;
  _GL_VERTEX_ATTRIB_ARRAY_ENABLED &= ~_GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED;

  int index = 0;
  while(_GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED)
  {
    if ((_GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED & 1)) _emscripten_glEnableVertexAttribArray(index);
    ++index;
    _GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED >>= 1;
  }

  index = 0;
  while(_GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED)
  {
    if ((_GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED & 1)) _emscripten_glDisableVertexAttribArray(index);
    ++index;
    _GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED >>= 1;
  }
}

GL_APICALL void GL_APIENTRY glEnableVertexAttribArray(GLuint index)
{
  _GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED &= ~(1U << index);
  _GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED |= (1U << index);
}

GL_APICALL void GL_APIENTRY glDisableVertexAttribArray(GLuint index)
{
  _GL_VERTEX_ATTRIB_ARRAY_pending_ENABLED &= ~(1U << index);
  _GL_VERTEX_ATTRIB_ARRAY_pending_DISABLED |= (1U << index);
}

static GLuint _GL_CURRENT_PROGRAM = 0;

GL_APICALL void GL_APIENTRY glUseProgram(GLuint program)
{
  if (program != _GL_CURRENT_PROGRAM)
  {
    _emscripten_glUseProgram(program);
    _GL_CURRENT_PROGRAM = program;
  }
}

static GLenum _GL_ACTIVE_TEXTURE = 0;

GL_APICALL void GL_APIENTRY glActiveTexture(GLenum texture)
{
  if (_GL_ACTIVE_TEXTURE == texture) return;
  _emscripten_glActiveTexture(texture);
  _GL_ACTIVE_TEXTURE = texture;
}

GL_APICALL void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawArrays(mode, first, count);
}

GL_APICALL void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawElements(mode, count, type, indices);
}

GL_APICALL void GL_APIENTRY glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawRangeElements(mode, start, end, count, type, indices);
}

GL_APICALL void GL_APIENTRY glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawArraysInstanced(mode, first, count, instancecount);
}

GL_APICALL void GL_APIENTRY glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawElementsInstanced(mode, count, type, indices, instancecount);
}

GL_APICALL void GL_APIENTRY glDrawArraysIndirect(GLenum mode, const void *indirect)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawArraysIndirect(mode, indirect);
}

GL_APICALL void GL_APIENTRY glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
  applyGlEnableVertexAttribArrays();
  _emscripten_glDrawElementsIndirect(mode, type, indirect);
}

}
