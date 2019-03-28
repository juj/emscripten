#if SEPARATE_ASM && ASSERTIONS && WASM == 0 && MODULARIZE
if (!({{{ASM_MODULE_NAME}}})) throw 'Must load asm.js Module in to variable {{{ASM_MODULE_NAME}}} before adding compiled output .js script to the DOM';
#endif

#include "runtime_safe_heap.js"

#if ASSERTIONS
/** @type {function(*, string=)} */
function assert(condition, text) {
  if (!condition) throw text;
}
#endif

function abort(what) {
  throw what;
}

var tempRet0 = 0;
var setTempRet0 = function(value) {
  tempRet0 = value;
}
var getTempRet0 = function() {
  return tempRet0;
}

function alignUp(x, multiple) {
  if (x % multiple > 0) {
    x += multiple - (x % multiple);
  }
  return x;
}

#include "runtime_strings.js"
#include "runtime_sab_polyfill.js"

#if USE_PTHREADS
var STATIC_BASE = {{{ GLOBAL_BASE }}};

if (!ENVIRONMENT_IS_PTHREAD) {
#endif

var GLOBAL_BASE = {{{ GLOBAL_BASE }}},
    TOTAL_STACK = {{{ TOTAL_STACK }}},
    TOTAL_MEMORY = {{{ TOTAL_MEMORY }}},
#if !USE_PTHREADS
    STATIC_BASE = {{{ GLOBAL_BASE }}},
#endif
    STACK_BASE = {{{ getQuoted('STACK_BASE') }}},
    STACKTOP = STACK_BASE,
    STACK_MAX = {{{ getQuoted('STACK_MAX') }}}
#if USES_DYNAMIC_ALLOC
    , DYNAMICTOP_PTR = {{{ makeStaticAlloc(4) }}}
#endif
    ;

#if WASM

#if ALLOW_MEMORY_GROWTH && WASM_MEM_MAX != -1
var wasmMaximumMemory = {{{ WASM_MEM_MAX }}};
#else
var wasmMaximumMemory = TOTAL_MEMORY;
#endif

var wasmMemory = new WebAssembly.Memory({
  'initial': TOTAL_MEMORY >> 16
#if USE_PTHREADS || !ALLOW_MEMORY_GROWTH || WASM_MEM_MAX != -1
  , 'maximum': wasmMaximumMemory >> 16
#endif
#if USE_PTHREADS
  , 'shared': true
#endif
  });

var buffer = wasmMemory.buffer;

#if USE_PTHREADS && ASSERTIONS
assert(buffer instanceof SharedArrayBuffer, 'requested a shared WebAssembly.Memory but the returned buffer is not a SharedArrayBuffer, indicating that while the browser has SharedArrayBuffer it does not have WebAssembly threads support - you may need to set a flag');
#endif

#else

#if USE_PTHREADS
var buffer = new SharedArrayBuffer(TOTAL_MEMORY);
#else
var buffer = new ArrayBuffer(TOTAL_MEMORY);
#endif

#endif

#if USE_PTHREADS
}
#endif

#if ASSERTIONS
var WASM_PAGE_SIZE = 65536;
#if USE_PTHREADS
if (!ENVIRONMENT_IS_PTHREAD) {
#endif
assert(STACK_BASE % 16 === 0, 'stack must start aligned to 16 bytes, STACK_BASE==' + STACK_BASE);
assert(TOTAL_MEMORY >= TOTAL_STACK, 'TOTAL_MEMORY should be larger than TOTAL_STACK, was ' + TOTAL_MEMORY + '! (TOTAL_STACK=' + TOTAL_STACK + ')');
assert(({{{ getQuoted('DYNAMIC_BASE') }}}) % 16 === 0, 'heap must start aligned to 16 bytes, DYNAMIC_BASE==' + {{{ getQuoted('DYNAMIC_BASE') }}});
assert(TOTAL_MEMORY % WASM_PAGE_SIZE === 0);
#if WASM_MEM_MAX != -1
assert({{{ WASM_MEM_MAX }}} % WASM_PAGE_SIZE == 0);
#endif
assert(buffer.byteLength === TOTAL_MEMORY);
#if USE_PTHREADS
}
#endif
#endif // ASSERTIONS

var HEAP8 = new Int8Array(buffer);
var HEAP16 = new Int16Array(buffer);
var HEAP32 = new Int32Array(buffer);
var HEAPU8 = new Uint8Array(buffer);
var HEAPU16 = new Uint16Array(buffer);
var HEAPU32 = new Uint32Array(buffer);
var HEAPF32 = new Float32Array(buffer);
var HEAPF64 = new Float64Array(buffer);

#if USE_PTHREADS
if (!ENVIRONMENT_IS_PTHREAD) {
#endif

#if !WASM || USE_PTHREADS
  HEAPU8.set(new Uint8Array(Module['mem']), GLOBAL_BASE);
#endif

#if USES_DYNAMIC_ALLOC
  HEAP32[DYNAMICTOP_PTR>>2] = {{{ getQuoted('DYNAMIC_BASE') }}};
#endif

#if USE_PTHREADS
}
#endif

#include "runtime_stack_check.js"
#include "runtime_assertions.js"

#if ASSERTIONS
var runtimeInitialized = false;

// This is always false in minimal_runtime - the runtime does not have a concept of exiting (keeping this variable here for now since it is referenced from generated code)
var runtimeExited = false;
#endif

{{{ unSign }}}
{{{ reSign }}}

#include "runtime_math.js"

var memoryInitializer = null;

#include "memoryprofiler.js"

// === Body ===
