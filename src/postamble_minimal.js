
// === Auto-generated postamble setup entry stuff ===

{{{ exportRuntime() }}}

function run() {
#if STACK_OVERFLOW_CHECK
  writeStackCookie();
#endif

  ensureInitRuntime();

#if PROXY_TO_PTHREAD
    // User requested the PROXY_TO_PTHREAD option, so call a stub main which pthread_create()s a new thread
    // that will call the user's real main() for the application.
    var ret = Module['_proxy_main']();
#else
    var ret = Module['_main']();
#endif

#if STACK_OVERFLOW_CHECK
  checkStackCookie();
#endif
}

#if WASM

// Initialize wasm (asynchronous)
var env = Module.asmLibraryArg;
env['memory'] = Module['wasmMemory'];
env['table'] = new WebAssembly.Table({ 'initial': Module['wasmTableSize'], 'maximum': Module['wasmMaxTableSize'], 'element': 'anyfunc' });
env['__memory_base'] = STATIC_BASE;
env['__table_base'] = 0;

var imports = {
  'global': {
    'NaN': NaN,
    'Infinity': Infinity
  },
  'global.Math': Math,
  'env': env,
  'asm2wasm': {
    'f64-rem': function(x, y) { return x % y; },
    'debugger': function() { debugger; }
  }
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
  ready();
});

#else

// Initialize asm.js (synchronous)
#if ASSERTIONS
if (!Module['mem']) throw 'Must load memory initializer as an ArrayBuffer in to variable Module.mem before adding compiled output .js script to the DOM';
#endif
HEAPU8.set(new Uint8Array(Module['mem']), GLOBAL_BASE);
ready();

#endif

{{GLOBAL_VARS}}
