#include <GLES2/gl2.h>
#include <GLES3/gl3.h>

static GLuint _GL_ARRAY_BUFFER_BINDING = 0;
static GLuint _GL_ELEMENT_ARRAY_BUFFER_BINDING = 0;

GL_APICALL void GL_APIENTRY _emscripten_glBindBuffer(GLenum target, GLuint buffer);
GL_APICALL void GL_APIENTRY _emscripten_glVertexAttribDivisor(GLuint index, GLuint divisor);
GL_APICALL void GL_APIENTRY _emscripten_glEnableVertexAttribArray(GLuint index);
GL_APICALL void GL_APIENTRY _emscripten_glDisableVertexAttribArray(GLuint index);
GL_APICALL void GL_APIENTRY _emscripten_glUseProgram(GLuint program);
GL_APICALL void GL_APIENTRY _emscripten_glActiveTexture(GLenum texture);

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

static uint8_t _GL_VERTEX_ATTRIB_ARRAY_ENABLED[_GL_MAX_VERTEX_ATTRIBS];

GL_APICALL void GL_APIENTRY glEnableVertexAttribArray(GLuint index)
{
  if (_GL_VERTEX_ATTRIB_ARRAY_ENABLED[index]) return;
  _emscripten_glEnableVertexAttribArray(index);
  _GL_VERTEX_ATTRIB_ARRAY_ENABLED[index] = 1;
}

GL_APICALL void GL_APIENTRY glDisableVertexAttribArray(GLuint index)
{
  if (!_GL_VERTEX_ATTRIB_ARRAY_ENABLED[index]) return;
  _emscripten_glDisableVertexAttribArray(index);
  _GL_VERTEX_ATTRIB_ARRAY_ENABLED[index] = 0;
}

static GLuint _GL_CURRENT_PROGRAM = 0;

GL_APICALL void GL_APIENTRY glUseProgram(GLuint program)
{
  if (_GL_CURRENT_PROGRAM == program) return;
  _emscripten_glUseProgram(program);
  _GL_CURRENT_PROGRAM = program;
}

static GLenum _GL_ACTIVE_TEXTURE = 0;

GL_APICALL void GL_APIENTRY glActiveTexture(GLenum texture)
{
  if (_GL_ACTIVE_TEXTURE == texture) return;
  _emscripten_glActiveTexture(texture);
  _GL_ACTIVE_TEXTURE = texture;
}
