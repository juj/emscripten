// Pthread Web Worker startup routine:
// This is the entry point file that is loaded first by each Web Worker
// that executes pthreads on the Emscripten application.

//#if PTHREADS_DEBUG == 2
console.error('Web worker pthread-main.js being parsed.');
//#endif

// Thread-local:
var threadInfoStruct = 0; // Info area for this thread in Emscripten HEAP (shared). If zero, this worker is not currently hosting an executing pthread.
var selfThreadId = 0; // The ID of this thread. 0 if not hosting a pthread.
var parentThreadId = 0; // The ID of the parent pthread that launched this thread.
var tempDoublePtr = 0; // A temporary memory area for global float and double marshalling operations.

// Thread-local: Each thread has its own allocated stack space.
var STACK_BASE = 0;
var STACKTOP = 0;
var STACK_MAX = 0;

// These are system-wide memory area parameters that are set at main runtime startup in main thread, and stay constant throughout the application.
var buffer; // All pthreads share the same Emscripten HEAP as SharedArrayBuffer with the main execution thread.
var DYNAMICTOP_PTR = 0;
var TOTAL_MEMORY = 0;
var STATICTOP = 0;
var staticSealed = true; // When threads are being initialized, the static memory area has been already sealed a long time ago.
var DYNAMIC_BASE = 0;

var ENVIRONMENT_IS_PTHREAD = true;

// Cannot use console.log or console.error in a web worker, since that would risk a browser deadlock! https://bugzilla.mozilla.org/show_bug.cgi?id=1049091
// Therefore implement custom logging facility for threads running in a worker, which queue the messages to main thread to print.
var Module = {};

//#if PTHREADS_DEBUG == 2
function debugDump(msg) {
  if (typeof dump === 'function') dump(msg + '\n');
  console.error(msg);
}
//#endif

this.addEventListener('error', function(e) {
  if (e.message.indexOf('SimulateInfiniteLoop') != -1) {
//#if PTHREADS_DEBUG == 2
    debugDump('Pthread ' + selfThreadId + ': threw SimulateInfiniteLoop to exit to event handler');
//#endif
    return e.preventDefault();
  } else {
    // Other errors propagate back to main browser thread's .onerror handler.
//#if PTHREADS_DEBUG
    var errorSource = ' in ' + e.filename + ':' + e.lineno + ':' + e.colno;
    debugDump('Pthread ' + selfThreadId + ' uncaught exception' + (e.filename || e.lineno || e.colno ? errorSource : '') + ': ' + e.message + '. Error object:');
    debugDump(e.error);
//#endif
  }
});

function threadPrint() {
  var text = Array.prototype.slice.call(arguments).join(' ');
  console.log(text);
}
function threadPrintErr() {
  var text = Array.prototype.slice.call(arguments).join(' ');
  console.error(text + '\n' + new Error().stack);
  console.error(new Error().stack);
}
function threadAlert() {
  var text = Array.prototype.slice.call(arguments).join(' ');
//#if PTHREADS_DEBUG
  debugDump('Pthread ' + selfThreadId + ' calling alert(): ' + text + '\n' + new Error().stack);
//#endif
  postMessage({cmd: 'alert', text: text, threadId: selfThreadId});
}
Module['print'] = threadPrint;
Module['printErr'] = threadPrintErr;
this.alert = threadAlert;

this.onmessage = function(e) {
  try {
//#if PTHREADS_DEBUG == 2
/*
    if (e.data.target != 'setimmediate') { // Logging this message would spam too much.
      debugDump('Pthread ' + selfThreadId + ' onmessage: ' + (e.data.cmd || e.data.target));
      debugDump(e.data);
    }
*/    
//#endif
  if (e.data.cmd === 'load') { // Preload command that is called once per worker to parse and load the Emscripten code.
    // Initialize the thread-local field(s):
    tempDoublePtr = e.data.tempDoublePtr;

    // Initialize the global "process"-wide fields:
    buffer = e.data.buffer;
    Module['TOTAL_MEMORY'] = TOTAL_MEMORY = e.data.TOTAL_MEMORY;
    STATICTOP = e.data.STATICTOP;
    DYNAMIC_BASE = e.data.DYNAMIC_BASE;
    DYNAMICTOP_PTR = e.data.DYNAMICTOP_PTR;

    PthreadWorkerInit = e.data.PthreadWorkerInit;
//#if PTHREADS_DEBUG
    debugDump('Web Worker load: importing ' + e.data.urlOrBlob);
//#endif
    if (typeof e.data.urlOrBlob === 'string') {
      importScripts(e.data.urlOrBlob);
    } else {
      var objectUrl = URL.createObjectURL(e.data.urlOrBlob);
      importScripts(objectUrl);
      URL.revokeObjectURL(objectUrl);
    }
    if (typeof FS !== 'undefined') FS.createStandardStreams();
    postMessage({ cmd: 'loaded' });
  } else if (e.data.cmd === 'objectTransfer') {
    PThread.receiveObjectTransfer(e.data);
  } else if (e.data.cmd === 'run') { // This worker was idle, and now should start executing its pthread entry point.
    threadInfoStruct = e.data.threadInfoStruct;
    __register_pthread_ptr(threadInfoStruct, /*isMainBrowserThread=*/0, /*isMainRuntimeThread=*/0); // Pass the thread address inside the asm.js scope to store it for fast access that avoids the need for a FFI out.
    selfThreadId = e.data.selfThreadId;
    parentThreadId = e.data.parentThreadId;
//#if PTHREADS_DEBUG
    debugDump('Web Worker run: threadInfoStruct: ' + threadInfoStruct + ', selfThreadId: ' + selfThreadId + ', parentThreadId: ' + parentThreadId);
//#endif
    assert(threadInfoStruct);
    assert(selfThreadId);
    assert(parentThreadId);
    // TODO: Emscripten runtime has these variables twice(!), once outside the asm.js module, and a second time inside the asm.js module.
    //       Review why that is? Can those get out of sync?
    STACK_BASE = STACKTOP = e.data.stackBase;
    STACK_MAX = STACK_BASE + e.data.stackSize;
//#if PTHREADS_DEBUG == 2
    debugDump('Web Worker run: STACK_BASE: ' + STACK_BASE + ', Stack size: ' + e.data.stackSize + ', STACK_MAX: ' + STACK_MAX);
//#endif
    assert(STACK_BASE != 0);
    assert(STACK_MAX > STACK_BASE);
    Runtime.establishStackSpace(e.data.stackBase, e.data.stackBase + e.data.stackSize);
// TODO: Preprocess pthread-main.js
//#if STACK_OVERFLOW_CHECK
    if (typeof writeStackCookie !== 'undefined') writeStackCookie();
//#endif
//#if STACK_OVERFLOW_CHECK
    if (typeof writeStackCookie === 'function') writeStackCookie();
//#endif

    PThread.receiveObjectTransfer(e.data);
    PThread.setThreadStatus(_pthread_self(), 1/*EM_THREAD_STATUS_RUNNING*/);

    var result = 0;
    try {
//#if PTHREADS_DEBUG == 2
      debugDump('Web Worker calling pthread main');
//#endif
      // HACK: Some code in the wild has instead signatures of form 'void *ThreadMain()', which seems to be ok in native code.
      // To emulate supporting both in test suites, use the following form. This is brittle!
      if (typeof Module['asm']['dynCall_ii'] !== 'undefined') {
        result = Module['asm'].dynCall_ii(e.data.start_routine, e.data.arg); // pthread entry points are always of signature 'void *ThreadMain(void *arg)'
      } else {
        result = Module['asm'].dynCall_i(e.data.start_routine); // as a hack, try signature 'i' as fallback.
      }
//#if STACK_OVERFLOW_CHECK
      if (typeof checkStackCookie === 'function') checkStackCookie();
//#endif

    } catch(e) {
//#if PTHREADS_DEBUG == 2
      debugDump('Web Worker main() threw an exception: ' + e);
      debugDump(e.stack);
//#endif
      if (e === 'Canceled!') {
        PThread.threadCancel();
        return;
      } else if (e === 'SimulateInfiniteLoop') {
        return;
      } else {
        Atomics.store(HEAPU32, (threadInfoStruct + 4 /*{{{ C_STRUCTS.pthread.threadExitCode }}}*/ ) >> 2, (e instanceof ExitStatus) ? e.status : -2 /*A custom entry specific to Emscripten denoting that the thread crashed.*/);
        Atomics.store(HEAPU32, (threadInfoStruct + 0 /*{{{ C_STRUCTS.pthread.threadStatus }}}*/ ) >> 2, 1); // Mark the thread as no longer running.
        _emscripten_futex_wake(threadInfoStruct + 0 /*{{{ C_STRUCTS.pthread.threadStatus }}}*/, 0x7FFFFFFF/*INT_MAX*/); // Wake all threads waiting on this thread to finish.
        if (!(e instanceof ExitStatus)) throw e;
      }
    }
    // The thread might have finished without calling pthread_exit(). If so, then perform the exit operation ourselves.
    // (This is a no-op if explicit pthread_exit() had been called prior.)
    if (!Module['noExitRuntime']) {
//#if PTHREADS_DEBUG == 2
      debugDump('pthread ' + selfThreadId + ' exited, no Module["noExitRuntime"] set, so quitting the thread.');
//#endif
      PThread.threadExit(result);
    }
    else {
//#if PTHREADS_DEBUG == 2
      console.log('pthread noExitRuntime: not quitting.');
//#endif
    }
  } else if (e.data.cmd === 'cancel') { // Main thread is asking for a pthread_cancel() on this thread.
    if (threadInfoStruct && PThread.thisThreadCancelState == 0/*PTHREAD_CANCEL_ENABLE*/) {
//#if PTHREADS_DEBUG == 2
      debugDump('pthread ' + selfThreadId + ' is cancelling on request.');
//#endif
      PThread.threadCancel();
    } 
//#if PTHREADS_DEBUG == 2
    else debugDump('pthread ' + selfThreadId + ' requested to be cancelled, but threadInfoStruct: ' + threadInfoStruct + ' no longer present, or PThread.thisThreadCancelState: ' + PThread.thisThreadCancelState + ' does not allow cancelling.');
//#endif      
  } else if (e.data.target === 'setimmediate') {
    // no-op
  } else if (e.data.cmd === 'processThreadQueue') {
    if (threadInfoStruct) { // If this thread is actually running?
      _emscripten_current_thread_process_queued_calls();
    }
//#if PTHREADS_DEBUG == 2
    else debugDump('pthread ' + selfThreadId + ' requested to process its thread queue, but but threadInfoStruct: ' + threadInfoStruct + ' no longer present, so ignoring.');
//#endif
  } else {
    Module['printErr']('pthread-main.js received unknown command ' + e.data.cmd);
    debugDump(e.data);
  }

  } catch(e) {
    debugDump('pthread-main.js onmessage() captured an uncaught exception: ' + e);
    debugDump(e.stack);
    throw e;
  }
}
