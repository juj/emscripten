
// === Auto-generated postamble setup entry stuff ===

{{{ exportRuntime() }}}

function run() {
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

#if WASM

// Initialize wasm (asynchronous)
var env = Module.asmLibraryArg;
env['memory'] = Module['wasmMemory'];
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

#if ASSERTIONS
if (!Module['wasm']) throw 'Must load WebAssembly Module in to variable Module.wasm before adding compiled output .js script to the DOM';
#endif

/*** ASM_MODULE_EXPORTS_DECLARES ***/

WebAssembly.instantiate(Module['wasm'], imports).then(function(output) {
  var asm = output.instance.exports;
#if DECLARE_ASM_MODULE_EXPORTS == 0
  for(var i in asm) this[i] = Module[i] = asm[i];
#else
  /*** ASM_MODULE_EXPORTS ***/
#endif

#if ASSERTIONS
  runtimeInitialized = true;
#endif

#if USE_PTHREADS
  if (!ENVIRONMENT_IS_PTHREAD) {
    // Pass the thread address inside the asm.js scope to store it for fast access that avoids the need for a FFI out.
    __register_pthread_ptr(PThread.mainThreadBlock, /*isMainBrowserThread=*/!ENVIRONMENT_IS_WORKER, /*isMainRuntimeThread=*/1);
    _emscripten_register_main_browser_thread_id(PThread.mainThreadBlock);
#endif

#if STACK_OVERFLOW_CHECK
    writeStackCookie();
#endif

    for(var i in __ATINIT__) __ATINIT__[i].func();

    ready();

#if USE_PTHREADS
  }
#endif

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
if (!Module['mem']) throw 'Must load memory initializer as an ArrayBuffer in to variable Module.mem before adding compiled output .js script to the DOM';
#endif
HEAPU8.set(new Uint8Array(Module['mem']), GLOBAL_BASE);
ready();

#endif

{{GLOBAL_VARS}}
