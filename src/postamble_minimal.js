
// === Auto-generated postamble setup entry stuff ===

{{{ exportRuntime() }}}

asm['runPostSets']();

HEAPU8.set(new Uint8Array(Module['mem']), GLOBAL_BASE);

{{GLOBAL_VARS}}

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

ready();
