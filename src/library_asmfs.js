mergeInto(LibraryManager.library, {
  /* Filesystem tree ASMFS['pathname'] = file_entry, where
    file_entry = {
      ptr: pointer to wasm heap for wasm side file entry,
      data: a typed array containing the data for this file
    }
  */
  $ASMFS: {},

  // Given an URL http://foo.com/file.png?query=val/foo,
  // returns the filename part, "file.png"
  _extract_basename: function(url) {
    var search = url.indexOf('?');
    if (search >= 0) {
      url = url.substr(0, search);
    }
    return url.substr(url.lastIndexOf('/') + 1);
  },

  // Appends the filename from an URL to the given maybePath name, if it represents a directory.
  // _append_filename('/', 'http://foo.com/file.png?query=val/foo') -> '/file.png');
  // _append_filename('/custom.png', 'http://foo.com/file.png?query=val/foo') -> '/custom.png');
  _append_filename__deps: ['_extract_basename'],
  _append_filename: function path(maybePath, url) {
    return (maybePath[maybePath.length-1] == '/') ? maybePath + __extract_basename(url) : maybePath;

  },

  emscripten_asmfs_copy_file_chunk_to_wasm_memory: function(fileHandle, srcOffset, dst, dstLength) {
#if ASSERTIONS
    assert(dst || dstLength == 0);
#endif
    var data = ASMFS[fileHandle];
    var buf = data.buffer;
    var srcLength = data.length - srcOffset;
    var copyLength = Math.min(copyLength, dstLength);
    HEAPU8.set((!srcOffset && srcLength <= dstLength)
      ? buf // Avoid temp garbage generation when we don't need to create a new view to the source file.
      : new Uint8Array(buf, srcOffset, copyLength), dst);
    return copyLength;
  },

  emscripten_asmfs_get_file_size_js__deps: ['$ASMFS'],
  emscripten_asmfs_get_file_size_js: function(fileHandle) {
    return ASMFS[fileHandle].length;
  },

  emscripten_asmfs_async_fetch_file_js__deps: ['_append_filename', '$ASMFS'],
  emscripten_asmfs_async_fetch_file_js: function(url, fileHandle, onComplete, userData) {
    fetch(UTF8ToString(url))
    .then(response => {
      response.arrayBuffer().then(data => {
        if (response.ok) ASMFS[fileHandle] = new Uint8Array(data);
        {{{ makeDynCall('vii') }}}(onComplete, response.status, userData);
      });
    });
/*
#if 0
    url = UTF8ToString(url);
    var dst = __append_filename(UTF8ToString(destination), url);
    fetch(url)
    .then(response => response.body)
    .then(body => {
      body.getReader().read().then(data => {
        ASMFS[dst] = data.value;
        console.dir(ASMFS);
        dynCall_vii(onComplete, 0, userData);
      })
    });
#endif
*/
  },

  emscripten_asmfs_get_file_size_js: function(path) {
    path = UTF8ToString(path);
    var file = ASMFS[path];
    return file && file.length;
  }
});
