
// === Auto-generated postamble setup entry stuff ===

{{{ exportRuntime() }}}

function run() {
#if MEMORYPROFILER
  emscriptenMemoryProfiler.onPreloadComplete();
#endif

#if PROXY_TO_PTHREAD
    // User requested the PROXY_TO_PTHREAD option, so call a stub main which pthread_create()s a new thread
    // that will call the user's real main() for the application.
    var ret = _proxy_main();
#else
    var ret = _main();
#endif

#if STACK_OVERFLOW_CHECK
  checkStackCookie();
#endif
}

function initRuntime(asm) {
#if ASSERTIONS
  runtimeInitialized = true;
#endif

#if USE_PTHREADS
  if (ENVIRONMENT_IS_PTHREAD) return;
  // Pass the thread address inside the asm.js scope to store it for fast access that avoids the need for a FFI out.
  __register_pthread_ptr(PThread.mainThreadBlock, /*isMainBrowserThread=*/!ENVIRONMENT_IS_WORKER, /*isMainRuntimeThread=*/1);
  _emscripten_register_main_browser_thread_id(PThread.mainThreadBlock);
#endif

#if STACK_OVERFLOW_CHECK
  writeStackCookie();
#endif

  /*** RUN_GLOBAL_INITIALIZERS(); ***/

  {{{ getQuoted('ATINITS') }}}
}

#if WASM

// Initialize wasm (asynchronous)
var env = asmLibraryArg;
env['memory'] = wasmMemory;
env['table'] = new WebAssembly.Table({ 'initial': {{{ getQuoted('WASM_TABLE_SIZE') }}}
#if !ALLOW_TABLE_GROWTH
  , 'maximum': {{{ getQuoted('WASM_TABLE_SIZE') }}}
#endif
  , 'element': 'anyfunc' });
env['__memory_base'] = STATIC_BASE;
env['__table_base'] = 0;

var imports = {
  'env': env
#if WASM_BACKEND == 0
  , 'global': {
    'NaN': NaN,
    'Infinity': Infinity
  },
  'global.Math': Math,
  'asm2wasm': {
    'f64-rem': function(x, y) { return x % y; },
    'debugger': function() { debugger; }
  }
#endif
};

#if DECLARE_ASM_MODULE_EXPORTS
/*** ASM_MODULE_EXPORTS_DECLARES ***/
#endif

#if BINARYEN_ASYNC_COMPILATION
#if USE_PTHREADS
// In multithreaded BINARYEN_ASYNC_COMPILATION mode, we will use streaming WebAssembly compilation on the main thread, but pthreads
// will synchronously instantiate the content that is posted to them via the Worker pipe.
(ENVIRONMENT_IS_PTHREAD ? WebAssembly.instantiate(Module['wasm'], imports) : WebAssembly.instantiateStreaming(fetch('{{{ TARGET_BASENAME }}}.wasm'), imports)).then(function(output) {
  Module['wasm'] = output.module; // Store the compiled Wasm Module so that main thread can pass it on to pthreads to load.
#else
// In singlethreaded BINARYEN_ASYNC_COMPILATION mode, we will use streaming WebAssembly compilation.
WebAssembly.instantiateStreaming(fetch('{{{ TARGET_BASENAME }}}.wasm'), imports).then(function(output) {
#endif
#else
#if ASSERTIONS
// In synchronous Wasm compilation mode, Module['wasm'] should contain a typed array of the Wasm object data.
if (!Module['wasm']) throw 'Must load WebAssembly Module in to variable Module.wasm before adding compiled output .js script to the DOM';
#endif
WebAssembly.instantiate(Module['wasm'], imports).then(function(output) {
#endif
  var asm = output.instance.exports;
#if DECLARE_ASM_MODULE_EXPORTS == 0

#if ENVIRONMENT_MAY_BE_NODE
  for(var i in asm) (typeof process !== "undefined" ? global : this)[i] = Module[i] = asm[i];
#else
  for(var i in asm) this[i] = Module[i] = asm[i];
#endif

#else
  /*** ASM_MODULE_EXPORTS ***/
#endif

    initRuntime(asm);
    ready();
})
#if ASSERTIONS
.catch(function(error) {
  console.error(error);
})
#endif
;

#else

// Initialize asm.js (synchronous)
#if ASSERTIONS
#if USE_PTHREADS
if (!ENVIRONMENT_IS_PTHREAD) {
#endif
  if (!Module['mem']) throw 'Must load memory initializer as an ArrayBuffer in to variable Module.mem before adding compiled output .js script to the DOM';
#if USE_PTHREADS
}
#endif

#endif
initRuntime(asm);
ready();

#endif

{{GLOBAL_VARS}}
