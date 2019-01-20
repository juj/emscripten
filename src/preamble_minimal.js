// === Preamble library stuff ===

// Documentation for the public APIs defined in this file must be updated in:
//    site/source/docs/api_reference/preamble.js.rst
// A prebuilt local version of the documentation is available at:
//    site/build/text/docs/api_reference/preamble.js.txt
// You can also build docs locally as HTML or other formats in site/
// An online HTML version (which may be of a different version of Emscripten)
//    is up at http://kripken.github.io/emscripten-site/docs/api_reference/preamble.js.html

#if SEPARATE_ASM && ASSERTIONS && WASM == 0 && MODULARIZE
if (!({{{ASM_MODULE_NAME}}})) throw 'Must load asm.js Module in to variable {{{ASM_MODULE_NAME}}} before adding compiled output .js script to the DOM';
#endif

#include "preamble_safe_heap.js"

//========================================
// Runtime essentials
//========================================

/** @type {function(*, string=)} */
function assert(condition, text) {
  if (!condition) throw text;
}

function abort(what) {
  throw what;
}

#include "preamble_strings.js"

// Memory management

var PAGE_SIZE = 16384;
var WASM_PAGE_SIZE = 65536;
var ASMJS_PAGE_SIZE = 16777216;
var MIN_TOTAL_MEMORY = 16777216;

function alignUp(x, multiple) {
  if (x % multiple > 0) {
    x += multiple - (x % multiple);
  }
  return x;
}

function updateGlobalBuffer(buf) {
  buffer = buf;
}

#if USE_PTHREADS
if (!ENVIRONMENT_IS_PTHREAD) { // Pthreads have already initialized these variables in src/pthread-main.js, where they were passed to the thread worker at startup time
#endif

var STATIC_BASE = {{{ GLOBAL_BASE }}},
    STACK_BASE = {{{ getStackBase() }}},
    STACKTOP = STACK_BASE,
    STACK_MAX = {{{ getStackMax() }}},
    DYNAMIC_BASE = {{{ getDynamicBase() }}},
    DYNAMICTOP_PTR = {{{ makeStaticAlloc(4) }}};

#if ASSERTIONS
assert(STACK_BASE % 16 === 0, 'stack must start aligned');
assert(DYNAMIC_BASE % 16 === 0, 'heap must start aligned');
#endif

#if USE_PTHREADS
}
#endif

#if USE_PTHREADS
if (ENVIRONMENT_IS_PTHREAD) {
//#if SEPARATE_ASM != 0
  //importScripts('{{{ SEPARATE_ASM }}}'); // load the separated-out asm.js
//#endif
}
#endif

#if STACK_OVERFLOW_CHECK
// Initializes the stack cookie. Called at the startup of main and at the startup of each thread in pthreads mode.
function writeStackCookie() {
  assert((STACK_MAX & 3) == 0);
  HEAPU32[(STACK_MAX >> 2)-1] = 0x02135467;
  HEAPU32[(STACK_MAX >> 2)-2] = 0x89BACDFE;
}

function checkStackCookie() {
  if (HEAPU32[(STACK_MAX >> 2)-1] != 0x02135467 || HEAPU32[(STACK_MAX >> 2)-2] != 0x89BACDFE) {
    abort('Stack overflow! Stack cookie has been overwritten, expected hex dwords 0x89BACDFE and 0x02135467, but received 0x' + HEAPU32[(STACK_MAX >> 2)-2].toString(16) + ' ' + HEAPU32[(STACK_MAX >> 2)-1].toString(16));
  }
  // Also test the global address 0 for integrity.
  if (HEAP32[0] !== 0x63736d65 /* 'emsc' */) throw 'Runtime error: The application has corrupted its heap memory area (address zero)!';
}

function abortStackOverflow(allocSize) {
  abort('Stack overflow! Attempted to allocate ' + allocSize + ' bytes on the stack, but stack has only ' + (STACK_MAX - stackSave() + allocSize) + ' bytes available!');
}
#endif

function establishStackSpaceInModule(stackBase, stackMax) {
  STACK_BASE = STACKTOP = stackBase;
  STACK_MAX = stackMax;
}

#if EMTERPRETIFY
function abortStackOverflowEmterpreter() {
  abort("Emterpreter stack overflow! Decrease the recursion level or increase EMT_STACK_MAX in tools/emterpretify.py (current value " + EMT_STACK_MAX + ").");
}
#endif

#if ABORTING_MALLOC
function abortOnCannotGrowMemory() {
#if WASM
  abort('Cannot enlarge memory arrays. Either (1) compile with  -s TOTAL_MEMORY=X  with X higher than the current value ' + TOTAL_MEMORY + ', (2) compile with  -s ALLOW_MEMORY_GROWTH=1  which allows increasing the size at runtime, or (3) if you want malloc to return NULL (0) instead of this abort, compile with  -s ABORTING_MALLOC=0 ');
#else
  abort('Cannot enlarge memory arrays. Either (1) compile with  -s TOTAL_MEMORY=X  with X higher than the current value ' + TOTAL_MEMORY + ', (2) compile with  -s ALLOW_MEMORY_GROWTH=1  which allows increasing the size at runtime but prevents some optimizations, (3) set Module.TOTAL_MEMORY to a higher value before the program runs, or (4) if you want malloc to return NULL (0) instead of this abort, compile with  -s ABORTING_MALLOC=0 ');
#endif
}
#endif

#if ALLOW_MEMORY_GROWTH
if (!Module['reallocBuffer']) Module['reallocBuffer'] = function(size) {
  var ret;
  try {
    var oldHEAP8 = HEAP8;
    ret = new ArrayBuffer(size);
    var temp = new Int8Array(ret);
    temp.set(oldHEAP8);
  } catch(e) {
    return false;
  }
  var success = _emscripten_replace_memory(ret);
  if (!success) return false;
  return ret;
};
#endif

#if ALLOW_MEMORY_GROWTH
var byteLength;
try {
  byteLength = Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);
  byteLength(new ArrayBuffer(4)); // can fail on older ie
} catch(e) { // can fail on older node/v8
  byteLength = function(buffer) { return buffer.byteLength; };
}
#endif

var TOTAL_STACK = {{{ TOTAL_STACK }}};
var TOTAL_MEMORY = {{{ TOTAL_MEMORY }}};
#if ASSERTIONS
if (TOTAL_MEMORY < TOTAL_STACK) err('TOTAL_MEMORY should be larger than TOTAL_STACK, was ' + TOTAL_MEMORY + '! (TOTAL_STACK=' + TOTAL_STACK + ')');
#endif

// Initialize the runtime's memory
#if ASSERTIONS
// check for full engine support (use string 'subarray' to avoid closure compiler confusion)
assert(typeof Int32Array !== 'undefined' && typeof Float64Array !== 'undefined' && Int32Array.prototype.subarray !== undefined && Int32Array.prototype.set !== undefined,
       'JS engine does not provide full typed array support');
#endif

#if IN_TEST_HARNESS

// Test runs in browsers should always be free from uncaught exceptions. If an uncaught exception is thrown, we fail browser test execution in the REPORT_RESULT() macro to output an error value.
if (ENVIRONMENT_IS_WEB) {
  window.addEventListener('error', function(e) {
    if (e.message.indexOf('SimulateInfiniteLoop') != -1) return;
    console.error('Page threw an exception ' + e);
    Module['pageThrewException'] = true;
  });
}

#if USE_PTHREADS
if (typeof SharedArrayBuffer === 'undefined' || typeof Atomics === 'undefined') {
  xhr = new XMLHttpRequest();
  xhr.open('GET', 'http://localhost:8888/report_result?skipped:%20SharedArrayBuffer%20is%20not%20supported!');
  xhr.send();
  setTimeout(function() { window.close() }, 2000);
}
#endif
#endif

#if USE_PTHREADS
#if !WASM
if (!ENVIRONMENT_IS_PTHREAD) var buffer = new SharedArrayBuffer(TOTAL_MEMORY);
// Currently SharedArrayBuffer does not have a slice() operation, so polyfill it in.
// Adapted from https://github.com/ttaubert/node-arraybuffer-slice, (c) 2014 Tim Taubert <tim@timtaubert.de>
// arraybuffer-slice may be freely distributed under the MIT license.
(function (undefined) {
  "use strict";
  function clamp(val, length) {
    val = (val|0) || 0;
    if (val < 0) return Math.max(val + length, 0);
    return Math.min(val, length);
  }
  if (typeof SharedArrayBuffer !== 'undefined' && !SharedArrayBuffer.prototype.slice) {
    SharedArrayBuffer.prototype.slice = function (from, to) {
      var length = this.byteLength;
      var begin = clamp(from, length);
      var end = length;
      if (to !== undefined) end = clamp(to, length);
      if (begin > end) return new ArrayBuffer(0);
      var num = end - begin;
      var target = new ArrayBuffer(num);
      var targetArray = new Uint8Array(target);
      var sourceArray = new Uint8Array(this, begin, num);
      targetArray.set(sourceArray);
      return target;
    };
  }
})();

if (typeof Atomics === 'undefined') {
  // Polyfill singlethreaded atomics ops from http://lars-t-hansen.github.io/ecmascript_sharedmem/shmem.html#Atomics.add
  // No thread-safety needed since we don't have multithreading support.
  Atomics = {};
  Atomics['add'] = function(t, i, v) { var w = t[i]; t[i] += v; return w; }
  Atomics['and'] = function(t, i, v) { var w = t[i]; t[i] &= v; return w; }
  Atomics['compareExchange'] = function(t, i, e, r) { var w = t[i]; if (w == e) t[i] = r; return w; }
  Atomics['exchange'] = function(t, i, v) { var w = t[i]; t[i] = v; return w; }
  Atomics['wait'] = function(t, i, v, o) { if (t[i] != v) return 'not-equal'; else return 'timed-out'; }
  Atomics['wake'] = function(t, i, c) { return 0; }
  Atomics['wakeOrRequeue'] = function(t, i1, c, i2, v) { return 0; }
  Atomics['isLockFree'] = function(s) { return true; }
  Atomics['load'] = function(t, i) { return t[i]; }
  Atomics['or'] = function(t, i, v) { var w = t[i]; t[i] |= v; return w; }
  Atomics['store'] = function(t, i, v) { t[i] = v; return v; }
  Atomics['sub'] = function(t, i, v) { var w = t[i]; t[i] -= v; return w; }
  Atomics['xor'] = function(t, i, v) { var w = t[i]; t[i] ^= v; return w; }
}

#else
if (!ENVIRONMENT_IS_PTHREAD) {
#if ALLOW_MEMORY_GROWTH
  Module['wasmMemory'] = new WebAssembly.Memory({ 'initial': TOTAL_MEMORY / WASM_PAGE_SIZE , 'maximum': {{{ WASM_MEM_MAX }}} / WASM_PAGE_SIZE, 'shared': true });
#else
  Module['wasmMemory'] = new WebAssembly.Memory({ 'initial': TOTAL_MEMORY / WASM_PAGE_SIZE , 'maximum': TOTAL_MEMORY / WASM_PAGE_SIZE, 'shared': true });
#endif
  var buffer = Module['wasmMemory'].buffer;
  assert(buffer instanceof SharedArrayBuffer, 'requested a shared WebAssembly.Memory but the returned buffer is not a SharedArrayBuffer, indicating that while the browser has SharedArrayBuffer it does not have WebAssembly threads support - you may need to set a flag');
}

#endif // !WASM
#else // USE_PTHREADS

// Allocate a buffer
#if WASM
#if ASSERTIONS
assert(TOTAL_MEMORY % WASM_PAGE_SIZE === 0);
#endif // ASSERTIONS
#if ALLOW_MEMORY_GROWTH
#if WASM_MEM_MAX != -1
#if ASSERTIONS
assert({{{ WASM_MEM_MAX }}} % WASM_PAGE_SIZE == 0);
#endif
Module['wasmMemory'] = new WebAssembly.Memory({ 'initial': TOTAL_MEMORY / WASM_PAGE_SIZE, 'maximum': {{{ WASM_MEM_MAX }}} / WASM_PAGE_SIZE });
#else
Module['wasmMemory'] = new WebAssembly.Memory({ 'initial': TOTAL_MEMORY / WASM_PAGE_SIZE });
#endif // BINARYEN_MEM_MAX
#else
Module['wasmMemory'] = new WebAssembly.Memory({ 'initial': TOTAL_MEMORY / WASM_PAGE_SIZE, 'maximum': TOTAL_MEMORY / WASM_PAGE_SIZE });
#endif // ALLOW_MEMORY_GROWTH
var buffer = Module['wasmMemory'].buffer;
#else
var buffer = new ArrayBuffer(TOTAL_MEMORY);
#endif

#if ASSERTIONS
assert(buffer.byteLength === TOTAL_MEMORY);
#endif // ASSERTIONS

#endif // USE_PTHREADS

#if ALLOW_MEMORY_GROWTH
function updateGlobalBufferViews() {
  HEAP8 = new Int8Array(buffer);
  HEAP16 = new Int16Array(buffer);
  HEAP32 = new Int32Array(buffer);
  HEAPU8 = new Uint8Array(buffer);
  HEAPU16 = new Uint16Array(buffer);
  HEAPU32 = new Uint32Array(buffer);
  HEAPF32 = new Float32Array(buffer);
  HEAPF64 = new Float64Array(buffer);
// TODO: #if EXPORT_HEAP, then assign Module['HEAP*'] = HEAP*
}
var HEAP,
/** @type {Int8Array} */
  HEAP8,
/** @type {Uint8Array} */
  HEAPU8,
/** @type {Int16Array} */
  HEAP16,
/** @type {Uint16Array} */
  HEAPU16,
/** @type {Int32Array} */
  HEAP32,
/** @type {Uint32Array} */
  HEAPU32,
/** @type {Float32Array} */
  HEAPF32,
/** @type {Float64Array} */
  HEAPF64;

updateGlobalBufferViews();
#else
var HEAP8 = new Int8Array(buffer);
var HEAP16 = new Int16Array(buffer);
var HEAP32 = new Int32Array(buffer);
var HEAPU8 = new Uint8Array(buffer);
var HEAPU16 = new Uint16Array(buffer);
var HEAPU32 = new Uint32Array(buffer);
var HEAPF32 = new Float32Array(buffer);
var HEAPF64 = new Float64Array(buffer);
#endif

#if USES_DYNAMIC_ALLOC
HEAP32[DYNAMICTOP_PTR>>2] = DYNAMIC_BASE;
#endif

function getTotalMemory() {
  return TOTAL_MEMORY;
}

// Endianness check (note: assumes compiler arch was little-endian)
#if STACK_OVERFLOW_CHECK
#if USE_PTHREADS
if (!ENVIRONMENT_IS_PTHREAD) {
#endif
  HEAP32[0] = 0x63736d65; /* 'emsc' */
#if USE_PTHREADS
} else {
  if (HEAP32[0] !== 0x63736d65) throw 'Runtime error: The application has corrupted its heap memory area (address zero)!';
}
#endif // USE_PTHREADS
#endif // STACK_OVERFLOW_CHECK
#if ASSERTIONS
HEAP16[1] = 0x6373;
if (HEAPU8[2] !== 0x73 || HEAPU8[3] !== 0x63) throw 'Runtime error: expected the system to be little-endian!';
#endif // ASSERTIONS

var __ATINIT__    = []; // functions called during startup

#if ASSERTIONS
var runtimeInitialized = false;
var runtimeExited = false;

#if USE_PTHREADS
if (ENVIRONMENT_IS_PTHREAD) runtimeInitialized = true; // The runtime is hosted in the main thread, and bits shared to pthreads via SharedArrayBuffer. No need to init again in pthread.
#endif
#endif

function ensureInitRuntime() {
#if STACK_OVERFLOW_CHECK
  checkStackCookie();
#endif
#if USE_PTHREADS
  if (ENVIRONMENT_IS_PTHREAD) return; // PThreads reuse the runtime from the main thread.
#endif
#if ASSERTIONS
  if (runtimeInitialized) return;
  runtimeInitialized = true;
#endif
#if USE_PTHREADS
  // Pass the thread address inside the asm.js scope to store it for fast access that avoids the need for a FFI out.
  __register_pthread_ptr(PThread.mainThreadBlock, /*isMainBrowserThread=*/!ENVIRONMENT_IS_WORKER, /*isMainRuntimeThread=*/1);
  _emscripten_register_main_browser_thread_id(PThread.mainThreadBlock);
#endif

  for(var i in __ATINIT__) __ATINIT__[i].func();
}

{{{ unSign }}}
{{{ reSign }}}

#if POLYFILL_OLD_MATH_FUNCTIONS
// check for imul support, and also for correctness ( https://bugs.webkit.org/show_bug.cgi?id=126345 )
if (!Math.imul || Math.imul(0xffffffff, 5) !== -5) Math.imul = function imul(a, b) {
  var ah  = a >>> 16;
  var al = a & 0xffff;
  var bh  = b >>> 16;
  var bl = b & 0xffff;
  return (al*bl + ((ah*bl + al*bh) << 16))|0;
};

#if PRECISE_F32
#if PRECISE_F32 == 1
if (!Math.fround) {
  var froundBuffer = new Float32Array(1);
  Math.fround = function(x) { froundBuffer[0] = x; return froundBuffer[0] };
}
#else // 2
if (!Math.fround) Math.fround = function(x) { return x };
#endif
#else
#if SIMD
if (!Math.fround) Math.fround = function(x) { return x };
#endif
#endif

if (!Math.clz32) Math.clz32 = function(x) {
  var n = 32;
  var y = x >> 16; if (y) { n -= 16; x = y; }
  y = x >> 8; if (y) { n -= 8; x = y; }
  y = x >> 4; if (y) { n -= 4; x = y; }
  y = x >> 2; if (y) { n -= 2; x = y; }
  y = x >> 1; if (y) return n - 2;
  return n - x;
};

if (!Math.trunc) Math.trunc = function(x) {
  return x < 0 ? Math.ceil(x) : Math.floor(x);
};
#else // POLYFILL_OLD_MATH_FUNCTIONS
#if ASSERTIONS
assert(Math.imul, 'This browser does not support Math.imul(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
assert(Math.fround, 'This browser does not support Math.fround(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
assert(Math.clz32, 'This browser does not support Math.clz32(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
assert(Math.trunc, 'This browser does not support Math.trunc(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
#endif
#endif // LEGACY_VM_SUPPORT

var Math_abs = Math.abs;
var Math_cos = Math.cos;
var Math_sin = Math.sin;
var Math_tan = Math.tan;
var Math_acos = Math.acos;
var Math_asin = Math.asin;
var Math_atan = Math.atan;
var Math_atan2 = Math.atan2;
var Math_exp = Math.exp;
var Math_log = Math.log;
var Math_sqrt = Math.sqrt;
var Math_ceil = Math.ceil;
var Math_floor = Math.floor;
var Math_pow = Math.pow;
var Math_imul = Math.imul;
var Math_fround = Math.fround;
var Math_round = Math.round;
var Math_min = Math.min;
var Math_max = Math.max;
var Math_clz32 = Math.clz32;
var Math_trunc = Math.trunc;

var memoryInitializer = null;

// === Body ===
