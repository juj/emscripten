mergeInto(LibraryManager.library, {
  $ASMFS: {},

  emscripten_asmfs_copy_file_chunk_to_wasm_memory: function(fileHandle, srcOffset, dst, dstLength) {
#if ASSERTIONS
    assert(dst || dstLength == 0);
#endif
    var data = ASMFS[fileHandle];
    var srcLength = data.length - srcOffset;
    var copyLength = Math.min(srcLength, dstLength);
    HEAPU8.set((!srcOffset && srcLength <= dstLength)
      ? data // Avoid temp garbage generation when we don't need to create a new view to the source file.
      : new Uint8Array(data.buffer, srcOffset, copyLength), dst);
    return copyLength;
  },

  emscripten_asmfs_copy_file_to_js_memory: function(fileHandle) {
    var contentLength = HEAPU32[(fileHandle + 4/*todo:offset size*/) >> 2];
    var ptr = HEAPU32[(fileHandle + 8/*todo:offset data*/) >> 2];
    ASMFS[fileHandle] = HEAPU8.slice(ptr, ptr + contentLength);
  },

  emscripten_asmfs_get_file_size_js__deps: ['$ASMFS'],
  emscripten_asmfs_get_file_size_js: function(fileHandle) {
    return ASMFS[fileHandle].length;
  },

  emscripten_asmfs_async_fetch_file_to_js__deps: ['$ASMFS'],
  emscripten_asmfs_async_fetch_file_to_js: function(url, fileHandle, onComplete, userData) {
    fetch(url)
    .then(response => {
      response.arrayBuffer().then(data => {
        if (response.ok) ASMFS[fileHandle] = new Uint8Array(data);
        {{{ makeDynCall('viii') }}}(onComplete, response.status, fileHandle, userData);
      });
    });
  },

  // Directly stream a file download into the Wasm heap, without first downloading a full copy
  // of the file bytes as ArrayBuffer in JS side. If the file is large, this avoids temp memory
  // usage pressure. (E.g. downloading a 100MB file in to Wasm heap routing via JS would otherwise
  // create a 100MB temp ArrayBuffer in JS first, before the data reaches wasm memory)
  emscripten_asmfs_async_fetch_file_to_wasm__deps: ['$ASMFS'],
  emscripten_asmfs_async_fetch_file_to_wasm: function(url, fileHandle, onComplete, userData) {
    var dataPtr, httpStatus;
    fetch(url)
    .then(response => {
      var contentLength = response.headers.get('Content-Length');
      dataPtr = _malloc(contentLength);
      HEAPU32[(fileHandle + 4/*todo:offset size*/) >> 2] = contentLength;
      HEAPU32[(fileHandle + 8/*todo:offset data*/) >> 2] = dataPtr;
      httpStatus = response.status;
      return response.body;
    }).then(body => {
      var reader = body.getReader();
      function onChunkDownloaded(data) {
        if (data.value)
        {
          // Stream the chunk contents directly to wasm heap:
          HEAPU8.set(data.value, dataPtr);
          dataPtr += data.value.length;
        }
        if (data.done) {
          {{{ makeDynCall('viii') }}}(onComplete, httpStatus, fileHandle, userData);
        } else {
          reader.read().then(onChunkDownloaded);
        }
      }
      reader.read().then(onChunkDownloaded);
    });
  },

  emscripten_asmfs_async_fetch_file_js__deps: ['$ASMFS', 'emscripten_asmfs_async_fetch_file_to_js', 'emscripten_asmfs_async_fetch_file_to_wasm'],
  emscripten_asmfs_async_fetch_file_js: function(url, fileHandle, residencyFlags, onComplete, userData) {
    url = UTF8ToString(url);
    if (residencyFlags & 1/*EMSCRIPTEN_ASMFS_WASM_RESIDENT*/) {
      _emscripten_asmfs_async_fetch_file_to_wasm(url, fileHandle, onComplete, userData);
    } else if (residencyFlags & 2/*EMSCRIPTEN_ASMFS_JS_RESIDENT*/) {
      _emscripten_asmfs_async_fetch_file_to_js(url, fileHandle, onComplete, userData);
    }
  },
});
