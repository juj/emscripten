//"use strict";

// XXX FIXME Hardcoded '4' in many places, here and in library_SDL, for RGBA

var LibraryGL = {
  $GL: {
    hashtable: function(name) {
      if (!this._hashtables) {
        this._hashtables = {};
      }
      if (!(name in this._hashtables)) {
        this._hashtables[name] = {
          table: {},
          counter: 0,
          add: function(obj) {
            var id = this.counter++;
            this.table[id] = obj;
            return id;
          },
          get: function(id) {
#if ASSERTIONS
            assert(id < this.counter, "Invalid id " + id + " for the hashtable " + name);
#endif
            return this.table[id];
          },
          remove: function(id) {
#if ASSERTIONS
            assert(id < this.counter, "Invalid id " + id + " for the hashtable " + name);
#endif
            delete this.table[id];
          }
        };
      }
      return this._hashtables[name];
    },
  },

  glGetString: function(name_) {
    switch(name_) {
      case Module.ctx.VENDOR:
      case Module.ctx.RENDERER:
      case Module.ctx.VERSION:
        return allocate(intArrayFromString(Module.ctx.getParameter(name_)), 'i8', ALLOC_NORMAL);
      case 0x1F03: // Extensions
        return allocate(intArrayFromString(Module.ctx.getSupportedExtensions().join(' ')), 'i8', ALLOC_NORMAL);
      default:
        throw 'Failure: Invalid glGetString value: ' + name_;
    }
  },

  glGetIntegerv: function(name_, p) {
    switch(name_) {
      case Module.ctx.MAX_TEXTURE_SIZE:
        {{{ makeSetValue('p', '0', 'Module.ctx.getParameter(name_)', 'i32') }}};
        break;
      default:
        throw 'Failure: Invalid glGetIntegerv value: ' + name_;
    }
  },

  glGenTextures__deps: ['$GL'],
  glGenTextures: function(n, textures) {
    for (var i = 0; i < n; i++) {
      var id = GL.hashtable("texture").add(Module.ctx.createTexture());
      {{{ makeSetValue('textures', 'i', 'id', 'i32') }}};
    }
  },

  glDeleteTextures: function(n, textures) {
    for (var i = 0; i < n; i++) {
      var id = {{{ makeGetValue('textures', 'i', 'i32') }}};
      Module.ctx.deleteTexture(GL.hashtable("texture").get(id));
      GL.hashtable("texture").remove(id);
    }
  },

  glTexImage2D: function(target, level, internalformat, width, height, border, format, type, pixels) {
    if (pixels) {
      pixels = new Uint8Array(Array_copy(pixels, pixels + width*height*4)); // TODO: optimize
    }
    Module.ctx.texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
  },

  glTexSubImage2D: function(target, level, xoffset, yoffset, width, height, format, type, pixels) {
    if (pixels) {
      pixels = new Uint8Array(Array_copy(pixels, pixels + width*height*4)); // TODO: optimize
    }
    Module.ctx.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
  },

  glBindTexture: function(target, texture) {
    Module.ctx.bindTexture(target, GL.hashtable("texture").get(texture));
  },

  glGenBuffers__deps: ['$GL'],
  glGenBuffers: function(n, buffers) {
    for (var i = 0; i < n; i++) {
      var id = GL.hashtable("buffer").add(Module.ctx.createBuffer());
      {{{ makeSetValue('buffers', 'i', 'id', 'i32') }}};
    }
  },

  glDeleteBuffers: function(n, buffers) {
    for (var i = 0; i < n; i++) {
      var id = {{{ makeGetValue('buffers', 'i', 'i32') }}};
      Module.ctx.deleteBuffer(GL.hashtable("buffer").get(id));
      GL.hashtable("buffer").remove(id);
    }
  },

  glBufferData: function(target, size, data, usage) {
    var floatArray = new Float32Array(TypedArray_copy(data, size));
    Module.ctx.bufferData(target, floatArray, usage);
  },

  glBindAttribLocation_deps: ['$GL'],
  glGetUniformLocation: function(program, name) {
    name = Pointer_stringify(name);
    return GL.hashtable("uniform").add(
             Module.ctx.getUniformLocation(GL.hashtable("program").get(program), name));
  },

  glUniform1f: function(Location, v0) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform1f(Location, v0);
  },

  glUniform2f: function(Location, v0, v1) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform2f(Location, v0, v1);
  },

  glUniform3f: function(Location, v0, v1, v2) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform3f(Location, v0, v1, v2);
  },

  glUniform4f: function(Location, v0, v1, v2, v3) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform4f(Location, v0, v1, v2, v3);
  },

  glUniform1i: function(Location, v0) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform1i(Location, v0);
  },

  glUniform2i: function(Location, v0, v1) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform2i(Location, v0, v1);
  },

  glUniform3i: function(Location, v0, v1, v2) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform3i(Location, v0, v1, v2);
  },

  glUniform4i: function(Location, v0, v1, v2, v3) {
    Location = GL.hashtable("uniform").get(Location);
    Module.ctx.uniform4i(Location, v0, v1, v2, v3);
  },

  glUniform1fv: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform1fv(Location, value);
  },

  glUniform2fv: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 2;
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform2fv(Location, value);
  },

  glUniform3fv: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 3;
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform3fv(Location, value);
  },

  glUniform4fv: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 4;
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform4fv(Location, value);
  },

  glUniform1fi: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    value = new Uint32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform1fi(Location, value);
  },

  glUniform2fi: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 2;
    value = new Uint32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform2fi(Location, value);
  },

  glUniform3fi: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 3;
    value = new Uint32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform3fi(Location, value);
  },

  glUniform4fi: function(Location, count, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 4;
    value = new Uint32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniform4fi(Location, value);
  },

  glUniformMatrix2fv: function(Location, count, transpose, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 4;
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniformMatrix2fv(Location, transpose, value);
  },

  glUniformMatrix3fv: function(Location, count, transpose, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 9;
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniformMatrix3fv(Location, transpose, value);
  },

  glUniformMatrix4fv: function(Location, count, transpose, value) {
    Location = GL.hashtable("uniform").get(Location);
    count *= 16;
    value = new Float32Array(TypedArray_copy(value, count*4)); // TODO: optimize
    Module.ctx.uniformMatrix4fv(Location, transpose, value);
  },

  glBindBuffer: function(target, buffer) {
    Module.ctx.bindBuffer(target, GL.hashtable("buffer").get(buffer));
  },

  glVertexAttrib1f: function(index, v0) {
    Module.ctx.vertexAttrib1f(index, v0);
  },

  glVertexAttrib2f: function(index, v0, v1) {
    Module.ctx.vertexAttrib2f(index, v0, v1);
  },

  glVertexAttrib3f: function(index, v0, v1, v2) {
    Module.ctx.vertexAttrib3f(index, v0, v1, v2);
  },

  glVertexAttrib4f: function(index, v0, v1, v2, v3) {
    Module.ctx.vertexAttrib4f(index, v0, v1, v2, v3);
  },

  glVertexAttrib1fv: function(index, v) {
    v = new Float32Array(TypedArray_copy(v, 1*4)); // TODO: optimize
    Module.ctx.vertexAttrib1fv(index, v);
  },

  glVertexAttrib2fv: function(index, v) {
    v = new Float32Array(TypedArray_copy(v, 2*4)); // TODO: optimize
    Module.ctx.vertexAttrib2fv(index, v);
  },

  glVertexAttrib3fv: function(index, v) {
    v = new Float32Array(TypedArray_copy(v, 3*4)); // TODO: optimize
    Module.ctx.vertexAttrib3fv(index, v);
  },

  glVertexAttrib4fv: function(index, v) {
    v = new Float32Array(TypedArray_copy(v, 4*4)); // TODO: optimize
    Module.ctx.vertexAttrib4fv(index, v);
  },

  glVertexAttribPointer: function(index, size, type, normalized, stride, pointer) {
    Module.ctx.vertexAttribPointer(index, size, type, normalized, stride, pointer);
  },

  glEnableVertexAttribArray: function(index) {
    Module.ctx.enableVertexAttribArray(index);
  },

  glDisableVertexAttribArray: function(index) {
    Module.ctx.disableVertexAttribArray(index);
  },

  glDrawArrays: function(mode, first, count) {
    Module.ctx.drawArrays(mode, first, count);
  },

  glGetAttribLocation: function(program, name) {
    program = GL.hashtable("program").get(program);
    name = Pointer_stringify(name);
    Module.ctx.getAttribLocation(program, name);
  },

  glCreateShader_deps: ['$GL'],
  glCreateShader: function(shaderType) {
    var shader = Module.ctx.createShader(shaderType);
    return GL.hashtable("shader").add(shader);
  },

  glShaderSource_deps: ['$GL'],
  glShaderSource: function(shader, count, string, length) {
    var source = "";
    for (var i = 0; i < count; ++i) {
      var frag = string[i];
      if (length) {
        var len = {{{ makeGetValue('length', 'i', 'i32') }}};
        if (len < 0) {
          frag = Pointer_stringify({{{ makeGetValue('string', 'i', 'i32') }}});
        } else {
          frag = Pointer_stringify({{{ makeGetValue('string', 'i', 'i32') }}}, len);
        }
      } else {
        frag = Pointer_stringify({{{ makeGetValue('string', 'i', 'i32') }}});
      }
      if (source.length) {
        source += "\n";
      }
      source += frag;
    }
    Module.ctx.shaderSource(GL.hashtable("shader").get(shader), source);
  },

  glCompileShader_deps: ['$GL'],
  glCompileShader: function(shader) {
    Module.ctx.compileShader(GL.hashtable("shader").get(shader));
  },

  glGetShaderInfoLog_deps: ['$GL'],
  glGetShaderInfoLog: function(shader, maxLength, length, infoLog) {
    var log = Module.ctx.getShaderInfoLog(GL.hashtable("shader").get(shader));
    log.slice(0, maxLength - 1);
    writeStringToMemory(log, infoLog);
    if (length) {
      {{{ makeSetValue('length', 'i', 'log.length', 'i32') }}}
    }
  },

  glCreateProgram_deps: ['$GL'],
  glCreateProgram: function() {
    return GL.hashtable("program").add(Module.ctx.createProgram());
  },

  glAttachShader_deps: ['$GL'],
  glAttachShader: function(program, shader) {
    Module.ctx.attachShader(GL.hashtable("program").get(program),
                            GL.hashtable("shader").get(shader));
  },

  glLinkProgram_deps: ['$GL'],
  glLinkProgram: function(program) {
    Module.ctx.linkProgram(GL.hashtable("program").get(program));
  },

  glGetProgramInfoLog_deps: ['$GL'],
  glGetProgramInfoLog: function(program, maxLength, length, infoLog) {
    var log = Module.ctx.getProgramInfoLog(GL.hashtable("program").get(program));
    log = log.substr(0, maxLength - 1);
    writeStringToMemory(log, infoLog);
    if (length) {
      {{{ makeSetValue('length', 'i', 'log.length', 'i32') }}}
    }
  },

  glUseProgram_deps: ['$Gl'],
  glUseProgram: function(program) {
    Module.ctx.useProgram(GL.hashtable("program").get(program));
  },

  glBindAttribLocation_deps: ['$GL'],
  glBindAttribLocation: function(program, index, name) {
    name = Pointer_stringify(name);
    Module.ctx.bindAttribLocation(GL.hashtable("program").get(program), index, name);
  },

};


// Simple pass-through functions
[[0, 'shadeModel fogi fogfv getError'],
 [1, 'clearDepth depthFunc enable disable frontFace cullFace clear'],
 [2, 'pixelStorei'],
 [3, 'texParameteri texParameterf'],
 [4, 'viewport clearColor scissor']].forEach(function(data) {
  var num = data[0];
  var names = data[1];
  var args = range(num).map(function(i) { return 'x' + i }).join(', ');
  var stub = '(function(' + args + ') { ' + (num > 0 ? 'Module.ctx.NAME(' + args + ')' : '') + ' })';
  names.split(' ').forEach(function(name_) {
    var cName = 'gl' + name_[0].toUpperCase() + name_.substr(1);
#if ASSERTIONS
    assert(!(cName in LibraryGL), "Cannot reimplement the existing function " + cName);
#endif
    LibraryGL[cName] = eval(stub.replace('NAME', name_));
    //print(cName + ': ' + LibraryGL[cName]);
  });
});

var LibraryGLUT = {
  $GLUT: {
    initTime: null,
    idleFunc: null,
    keyboardFunc: null,
    reshapeFunc: null,
    lastX: 0,
    lastY: 0,

    onMousemove: function(event) {
      GLUT.lastX = event.clientX;
      GLUT.lastY = event.clientY;
    },

    onKeypress: function(event) {
      if (GLUT.keyboardFunc) {
        var key = null;
        switch (event.keyCode) {
          case event.DOM_VK_F1: key = 1 /* GLUT_KEY_F1 */; break;
          case event.DOM_VK_F2: key = 2 /* GLUT_KEY_F2 */; break;
          case event.DOM_VK_F3: key = 3 /* GLUT_KEY_F3 */; break;
          case event.DOM_VK_F4: key = 4 /* GLUT_KEY_F4 */; break;
          case event.DOM_VK_F5: key = 5 /* GLUT_KEY_F5 */; break;
          case event.DOM_VK_F6: key = 6 /* GLUT_KEY_F6 */; break;
          case event.DOM_VK_F7: key = 7 /* GLUT_KEY_F7 */; break;
          case event.DOM_VK_F8: key = 8 /* GLUT_KEY_F8 */; break;
          case event.DOM_VK_F9: key = 9 /* GLUT_KEY_F9 */; break;
          case event.DOM_VK_F10: key = 10 /* GLUT_KEY_F10 */; break;
          case event.DOM_VK_F11: key = 11 /* GLUT_KEY_F11 */; break;
          case event.DOM_VK_F12: key = 12 /* GLUT_KEY_F12 */; break;
          case event.DOM_VK_LEFT: key = 100 /* GLUT_KEY_LEFT */; break;
          case event.DOM_VK_UP: key = 101 /* GLUT_KEY_UP */; break;
          case event.DOM_VK_RIGHT: key = 102 /* GLUT_KEY_RIGHT */; break;
          case event.DOM_VK_DOWN: key = 103 /* GLUT_KEY_DOWN */; break;
          case event.DOM_VK_PAGE_UP: key = 104 /* GLUT_KEY_PAGE_UP */; break;
          case event.DOM_VK_PAGE_DOWN: key = 105 /* GLUT_KEY_PAGE_DOWN */; break;
          case event.DOM_VK_HOME: key = 106 /* GLUT_KEY_HOME */; break;
          case event.DOM_VK_END: key = 107 /* GLUT_KEY_END */; break;
          case event.DOM_VK_INSERT: key = 108 /* GLUT_KEY_INSERT */; break;
          default: return;
        };
        if (key !== null) {
          FUNCTION_TABLE[GLUT.keyboardFunc](key, GLUT.lastX, GLUT.lastY);
        }
      }
    },
  },

  glutInit__deps: ['$GLUT'],
  glutInit: function(argcp, argv) {
    // Ignore arguments
    GLUT.initTime = Date.now();
    window.addEventListener("keypress", GLUT.onKeypress, true);
    window.addEventListener("mousemove", GLUT.onMousemove, true);
  },

  glutInitWindowSize: function(width, height) {
    Module['canvas'].width = width;
    Module['canvas'].height = height;
  },

  glutGet: function(type) {
    switch (type) {
      case 700: /* GLUT_ELAPSED_TIME */
        var now = Date.now();
        return now - GLUT.initTime;
      default:
        throw "glutGet(" + type + ") not implemented yet";
    }
  },

  glutDisplayFunc: function(func) {
    var RAF = window['setTimeout'];
    if (window['requestAnimationFrame']) {
      RAF = window['requestAnimationFrame'];
    } else if (window['mozRequestAnimationFrame']) {
      RAF = window['mozRequestAnimationFrame'];
    } else if (window['webkitRequestAnimationFrame']) {
      RAF = window['webkitRequestAnimationFrame'];
    } else if (window['msRequestAnimationFrame']) {
      RAF = window['msRequestAnimationFrame'];
    }
    RAF.apply(window, [function() {
      if (GLUT.reshapeFunc) {
        FUNCTION_TABLE[GLUT.reshapeFunc](Module['canvas'].width,
                                         Module['canvas'].height);
      }
      if (GLUT.idleFunc) {
        FUNCTION_TABLE[GLUT.idleFunc]();
      }
      FUNCTION_TABLE[func]();
      _glutDisplayFunc(func);
    }]);
  },

  glutIdleFunc: function(func) {
    GLUT.idleFunc = func;
  },

  glutSpecialFunc: function(func) {
    GLUT.keyboardFunc = func;
  },

  glutReshapeFunc: function(func) {
    GLUT.reshapeFunc = func;
  },

  glutCreateWindow: function(name) {
    try {
      var ctx = Module.canvas.getContext('experimental-webgl');
      if (!ctx) throw 'Could not create canvas :(';
      Module.ctx = ctx;
      // Set the background of the canvas to black, because glut gives us a
      // window which has a black background by default.
      Module.canvas.style.backgroundColor = "black";
    } catch (e) {
      Module.print('(canvas not available)');
    }
  },

  glutInitDisplayMode: function(mode) {},
  glutMainLoop: function() {},
  glutSwapBuffers: function() {},
  glutPostRedisplay: function() {},
};

mergeInto(LibraryManager.library, LibraryGL);
mergeInto(LibraryManager.library, LibraryGLUT);

