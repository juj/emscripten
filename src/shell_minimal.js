// Copyright 2010 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#if SIDE_MODULE == 0
// The Module object: Our interface to the outside world. We import
// and export values on it. There are various ways Module can be used:
// 1. Not defined. We create it here
// 2. A function parameter, function(Module) { ..generated code.. }
// 3. pre-run appended it, var Module = {}; ..generated code..
// 4. External script tag defines var Module.
// We need to check if Module already exists (e.g. case 3 above).
// Substitution will be replaced with actual code on later stage of the build,
// this way Closure Compiler will not mangle it (e.g. case 4. above).
// Note that if you want to run closure, and also to use Module
// after the generated code, you will need to define   var Module = {};
// before the code. Then that object will be used in the code, and you
// can continue to use Module afterwards as well.
#if USE_CLOSURE_COMPILER
// if (!Module)` is crucial for Closure Compiler here as it will otherwise replace every `Module` occurrence with a string
var Module;
if (!Module) Module = "__EMSCRIPTEN_PRIVATE_MODULE_EXPORT_NAME_SUBSTITUTION__";
#else
var Module = typeof {{{ EXPORT_NAME }}} !== 'undefined' ? {{{ EXPORT_NAME }}} : {};
#endif // USE_CLOSURE_COMPILER
#endif // SIDE_MODULE

// Redefine this in a --pre-js to override behavior
function locateFile(url, prefix) {
  return url;
}

function out(text) {
  console.log(text);
}

function err(text) {
  console.error(text);
}

// --pre-jses are emitted after the Module integration code, so that they can
// refer to Module (if they choose; they can also define Module)
// {{PRE_JSES}}

// Determine the runtime environment we are in. You can customize this by
// setting the ENVIRONMENT setting at compile time (see settings.js).

#if ENVIRONMENT
var ENVIRONMENT_IS_WEB = {{{ ENVIRONMENT === 'web' }}};
var ENVIRONMENT_IS_WORKER = {{{ ENVIRONMENT === 'worker' }}};
var ENVIRONMENT_IS_NODE = {{{ ENVIRONMENT === 'node' }}};
var ENVIRONMENT_IS_SHELL = {{{ ENVIRONMENT === 'shell' }}};
#else // ENVIRONMENT
var ENVIRONMENT_IS_WEB = false;
var ENVIRONMENT_IS_WORKER = false;
var ENVIRONMENT_IS_NODE = false;
var ENVIRONMENT_IS_SHELL = false;
ENVIRONMENT_IS_WEB = typeof window === 'object';
ENVIRONMENT_IS_WORKER = typeof importScripts === 'function';
ENVIRONMENT_IS_NODE = typeof process === 'object' && typeof require === 'function' && !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_WORKER;
ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;
#endif // ENVIRONMENT

#if ASSERTIONS
if (Module['ENVIRONMENT']) {
  throw new Error('Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -s ENVIRONMENT=web or -s ENVIRONMENT=node)');
}
#endif

// Three configurations we can be running in:
// 1) We could be the application main() thread running in the main JS UI thread. (ENVIRONMENT_IS_WORKER == false and ENVIRONMENT_IS_PTHREAD == false)
// 2) We could be the application main() thread proxied to worker. (with Emscripten -s PROXY_TO_WORKER=1) (ENVIRONMENT_IS_WORKER == true, ENVIRONMENT_IS_PTHREAD == false)
// 3) We could be an application pthread running in a worker. (ENVIRONMENT_IS_WORKER == true and ENVIRONMENT_IS_PTHREAD == true)
#if USE_PTHREADS

/*
var ENVIRONMENT_IS_PTHREAD;
if (!ENVIRONMENT_IS_PTHREAD) ENVIRONMENT_IS_PTHREAD = false; // ENVIRONMENT_IS_PTHREAD=true will have been preset in pthread-main.js. Make it false in the main runtime thread.
var PthreadWorkerInit; // Collects together variables that are needed at initialization time for the web workers that host pthreads.
if (!ENVIRONMENT_IS_PTHREAD) PthreadWorkerInit = {};
*/

if (typeof ENVIRONMENT_IS_PTHREAD === 'undefined') {
  // ENVIRONMENT_IS_PTHREAD=true will have been preset in pthread-main.js. Make it false in the main runtime thread. 
  // N.B. this line needs to appear without 'var' keyword to avoid 'var hoisting' from occurring. (https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/var)
  ENVIRONMENT_IS_PTHREAD = false;
  var PthreadWorkerInit = {}; // Collects together variables that are needed at initialization time for the web workers that host pthreads.
} else {
  var buffer = {{{EXPORT_NAME}}}.buffer;
  var tempDoublePtr = {{{EXPORT_NAME}}}.tempDoublePtr;
  var TOTAL_MEMORY = {{{EXPORT_NAME}}}.TOTAL_MEMORY;
  var STATICTOP = {{{EXPORT_NAME}}}.STATICTOP;
  var DYNAMIC_BASE = {{{EXPORT_NAME}}}.DYNAMIC_BASE;
  var DYNAMICTOP_PTR = {{{EXPORT_NAME}}}.DYNAMICTOP_PTR;
  var PthreadWorkerInit = {{{EXPORT_NAME}}}.PthreadWorkerInit;
  var STACK_BASE = {{{EXPORT_NAME}}}.STACK_BASE;
  var STACKTOP = {{{EXPORT_NAME}}}.STACKTOP;
  var STACK_MAX = {{{EXPORT_NAME}}}.STACK_MAX;
}


var currentScriptUrl = typeof _scriptDir !== 'undefined' ? _scriptDir : ((typeof document !== 'undefined' && document.currentScript) ? document.currentScript.src : undefined);
#endif // USE_PTHREADS

#if ENVIRONMENT_MAY_BE_NODE
if (ENVIRONMENT_IS_NODE) {
#if ENVIRONMENT
#if ASSERTIONS
  if (!(typeof process === 'object' && typeof require === 'function')) throw new Error('not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)');
#endif
#endif

  // Expose functionality in the same simple way that the shells work
  // Note that we pollute the global namespace here, otherwise we break in node
  var nodeFS;
  var nodePath;

  Module['read'] = function shell_read(filename, binary) {
    var ret;
#if SUPPORT_BASE64_EMBEDDING
    ret = tryParseAsDataURI(filename);
    if (!ret) {
#endif
      if (!nodeFS) nodeFS = require('fs');
      if (!nodePath) nodePath = require('path');
      filename = nodePath['normalize'](filename);
      ret = nodeFS['readFileSync'](filename);
#if SUPPORT_BASE64_EMBEDDING
    }
#endif
    return binary ? ret : ret.toString();
  };

  Module['readBinary'] = function readBinary(filename) {
    var ret = Module['read'](filename, true);
    if (!ret.buffer) {
      ret = new Uint8Array(ret);
    }
    assert(ret.buffer);
    return ret;
  };

  if (process['argv'].length > 1) {
    Module['thisProgram'] = process['argv'][1].replace(/\\/g, '/');
  }

#if MODULARIZE
  // MODULARIZE will export the module in the proper place outside, we don't need to export here
#else
  if (typeof module !== 'undefined') {
    module['exports'] = Module;
  }
#endif

  // Currently node will swallow unhandled rejections, but this behavior is
  // deprecated, and in the future it will exit with error status.
  process['on']('unhandledRejection', abort);

  Module['inspect'] = function () { return '[Emscripten Module object]'; };
} else
#endif // ENVIRONMENT_MAY_BE_NODE
#if ENVIRONMENT_MAY_BE_SHELL
if (ENVIRONMENT_IS_SHELL) {

#if ENVIRONMENT
#if ASSERTIONS
  if ((typeof process === 'object' && typeof require === 'function') || typeof window === 'object' || typeof importScripts === 'function') throw new Error('not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)');
#endif
#endif

  if (typeof read != 'undefined') {
    Module['read'] = function shell_read(f) {
#if SUPPORT_BASE64_EMBEDDING
      var data = tryParseAsDataURI(f);
      if (data) {
        return intArrayToString(data);
      }
#endif
      return read(f);
    };
  }

  Module['readBinary'] = function readBinary(f) {
    var data;
#if SUPPORT_BASE64_EMBEDDING
    data = tryParseAsDataURI(f);
    if (data) {
      return data;
    }
#endif
    if (typeof readbuffer === 'function') {
      return new Uint8Array(readbuffer(f));
    }
    data = read(f, 'binary');
    assert(typeof data === 'object');
    return data;
  };
} else
#endif // ENVIRONMENT_MAY_BE_SHELL

{{BODY}}

// {{MODULE_ADDITIONS}}
