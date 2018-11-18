
// === Auto-generated postamble setup entry stuff ===

Module['asm'] = asm;

{{{ exportRuntime() }}}

if (Module['asm']['runPostSets']) {
  Module['asm']['runPostSets']();
}

#if MEM_INIT_IN_WASM == 0
#if MEM_INIT_METHOD == 2
#if USE_PTHREADS
if (memoryInitializer && !ENVIRONMENT_IS_PTHREAD) (function(s) {
#else
if (memoryInitializer) (function(s) {
#endif
  var i, n = s.length;
#if ASSERTIONS
  n -= 4;
  var crc, bit, table = new Int32Array(256);
  for (i = 0; i < 256; ++i) {
    for (crc = i, bit = 0; bit < 8; ++bit)
      crc = (crc >>> 1) ^ ((crc & 1) * 0xedb88320);
    table[i] = crc >>> 0;
  }
  crc = -1;
  crc = table[(crc ^ n) & 0xff] ^ (crc >>> 8);
  crc = table[(crc ^ (n >>> 8)) & 0xff] ^ (crc >>> 8);
  for (i = 0; i < s.length; ++i) {
    crc = table[(crc ^ s.charCodeAt(i)) & 0xff] ^ (crc >>> 8);
  }
  assert(crc === 0, "memory initializer checksum");
#endif
  for (i = 0; i < n; ++i) {
    HEAPU8[GLOBAL_BASE + i] = s.charCodeAt(i);
  }
})(memoryInitializer);
#else
#if USE_PTHREADS
if (memoryInitializer && !ENVIRONMENT_IS_PTHREAD) {
#else
if (memoryInitializer) {
#endif
  if (!isDataURI(memoryInitializer)) {
    memoryInitializer = locateFile(memoryInitializer);
  }
  if (ENVIRONMENT_IS_NODE || ENVIRONMENT_IS_SHELL) {
    var data = Module['readBinary'](memoryInitializer);
    HEAPU8.set(data, GLOBAL_BASE);
  } else {
//    addRunDependency('memory initializer');
    var applyMemoryInitializer = function(data) {
      if (data.byteLength) data = new Uint8Array(data);
#if ASSERTIONS
      for (var i = 0; i < data.length; i++) {
        assert(HEAPU8[GLOBAL_BASE + i] === 0, "area for memory initializer should not have been touched before it's loaded");
      }
#endif
      HEAPU8.set(data, GLOBAL_BASE);
      // Delete the typed array that contains the large blob of the memory initializer request response so that
      // we won't keep unnecessary memory lying around. However, keep the XHR object itself alive so that e.g.
      // its .status field can still be accessed later.
      if (Module['memoryInitializerRequest']) delete Module['memoryInitializerRequest'].response;
//      removeRunDependency('memory initializer');
    }
    function doBrowserLoad() {
      Module['readAsync'](memoryInitializer, applyMemoryInitializer, function() {
        throw 'could not load memory initializer ' + memoryInitializer;
      });
    }
#if SUPPORT_BASE64_EMBEDDING
    var memoryInitializerBytes = tryParseAsDataURI(memoryInitializer);
    if (memoryInitializerBytes) {
      applyMemoryInitializer(memoryInitializerBytes.buffer);
    } else
#endif
    if (Module['memoryInitializerRequest']) {
      // a network request has already been created, just use that
      function useRequest() {
        var request = Module['memoryInitializerRequest'];
        var response = request.response;
        if (request.status !== 200 && request.status !== 0) {
#if SUPPORT_BASE64_EMBEDDING
          var data = tryParseAsDataURI(Module['memoryInitializerRequestURL']);
          if (data) {
            response = data.buffer;
          } else {
#endif
            // If you see this warning, the issue may be that you are using locateFile and defining it in JS. That
            // means that the HTML file doesn't know about it, and when it tries to create the mem init request early, does it to the wrong place.
            // Look in your browser's devtools network console to see what's going on.
            console.warn('a problem seems to have happened with Module.memoryInitializerRequest, status: ' + request.status + ', retrying ' + memoryInitializer);
            doBrowserLoad();
            return;
#if SUPPORT_BASE64_EMBEDDING
          }
#endif
        }
        applyMemoryInitializer(response);
      }
      if (Module['memoryInitializerRequest'].response) {
        setTimeout(useRequest, 0); // it's already here; but, apply it asynchronously
      } else {
        Module['memoryInitializerRequest'].addEventListener('load', useRequest); // wait for it
      }
    } else {
      // fetch it from the network ourselves
      doBrowserLoad();
    }
  }
}
#endif
#endif // MEM_INIT_IN_WASM == 0

#if CYBERDWARF
  Module['cyberdwarf'] = _cyberdwarf_Debugger(cyberDWARFFile);
#endif

#if MODULARIZE
#if MODULARIZE_INSTANCE == 0
// Modularize mode returns a function, which can be called to
// create instances. The instances provide a then() method,
// must like a Promise, that receives a callback. The callback
// is called when the module is ready to run, with the module
// as a parameter. (Like a Promise, it also returns the module
// so you can use the output of .then(..)).
Module['then'] = function(func) {
  // We may already be ready to run code at this time. if
  // so, just queue a call to the callback.
  if (Module['calledRun']) {
    func(Module);
  } else {
    // we are not ready to call then() yet. we must call it
    // at the same time we would call onRuntimeInitialized.
    var old = Module['onRuntimeInitialized'];
    Module['onRuntimeInitialized'] = function() {
      if (old) old();
      func(Module);
    };
  }
  return Module;
};
#endif
#endif

var calledMain = false;

#if HAS_MAIN
Module['callMain'] = function callMain() {

  ensureInitRuntime();

#if PROXY_TO_PTHREAD
    // User requested the PROXY_TO_PTHREAD option, so call a stub main which pthread_create()s a new thread
    // that will call the user's real main() for the application.
    var ret = Module['_proxy_main']();
#else
    var ret = Module['_main']();
#endif
  calledMain = true;
}
#endif // HAS_MAIN

{{GLOBAL_VARS}}

/** @type {function(Array=)} */
function run() {
#if STACK_OVERFLOW_CHECK
  writeStackCookie();
#endif

  if (Module['calledRun']) return; // run may have just been called through dependencies being fulfilled just in this very frame

  function doRun() {
    if (Module['calledRun']) return; // run may have just been called while the async setStatus time below was happening
    Module['calledRun'] = true;

    if (ABORT) return;

    ensureInitRuntime();

    if (Module['onRuntimeInitialized']) Module['onRuntimeInitialized']();

    Module['callMain']();
  }

  doRun();
#if STACK_OVERFLOW_CHECK
  checkStackCookie();
#endif
}
Module['run'] = run;

function abort(what) {
  throw what;
}

#if USE_PTHREADS
// XXXXXX HACKS
Module._emscripten_futex_wake = _emscripten_futex_wake;
#endif
