// Copyright 2010 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#if SIDE_MODULE == 0
#if USE_CLOSURE_COMPILER
// if (!Module)` is crucial for Closure Compiler here as it will otherwise replace every `Module` occurrence with a string
var Module;
if (!Module) Module = "__EMSCRIPTEN_PRIVATE_MODULE_EXPORT_NAME_SUBSTITUTION__";
#else
var Module = {{{ EXPORT_NAME }}};
#endif // USE_CLOSURE_COMPILER
#endif // SIDE_MODULE

// Redefine these in a --pre-js to override behavior
function out(text) {
  console.log(text);
}

function err(text) {
  console.error(text);
}

function ready() {
  run();
}
// --pre-jses are emitted after the Module integration code, so that they can
// refer to Module (if they choose; they can also define Module)
// {{PRE_JSES}}

#if USE_PTHREADS

var ENVIRONMENT_IS_PTHREAD;
if (!ENVIRONMENT_IS_PTHREAD) ENVIRONMENT_IS_PTHREAD = false; // ENVIRONMENT_IS_PTHREAD=true will have been preset in pthread-main.js. Make it false in the main runtime thread.
var PthreadWorkerInit; // Collects together variables that are needed at initialization time for the web workers that host pthreads.
if (!ENVIRONMENT_IS_PTHREAD) PthreadWorkerInit = {};

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

{{BODY}}

// {{MODULE_ADDITIONS}}
