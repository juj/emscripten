/*
 * Copyright 2020 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS unsigned int
#define EMSCRIPTEN_ASMFS_WASM_RESIDENT 1
#define EMSCRIPTEN_ASMFS_JS_RESIDENT 2
#define EMSCRIPTEN_ASMFS_INDEXEDDB_RESIDENT 4
#define EMSCRIPTEN_ASMFS_LOCALSTORAGE_RESIDENT 8
#define EMSCRIPTEN_ASMFS_SESSIONSTORAGE_RESIDENT 16

typedef void *AsmfsFileHandle;

typedef void (*asmfs_fetch_finished_callback)(int httpStatusResult, AsmfsFileHandle fileHandle, void *userData);

// Starts an asynchronous download of the given URL, and places the resulting ArrayBuffer to the given destination location in the filesystem, backed by JS side memory.
extern void emscripten_asmfs_async_file_fetch(const char *url, const char *dst, EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS residencyFlags, asmfs_fetch_finished_callback cb, void *userData);

extern uint8_t *emscripten_asmfs_get_file_ptr(AsmfsFileHandle fileHandle);

extern size_t emscripten_asmfs_get_file_size(AsmfsFileHandle fileHandle);

extern const char *emscripten_asmfs_get_file_name(AsmfsFileHandle fileHandle);

extern void emscripten_asmfs_dump_tree(void);

extern size_t emscripten_asmfs_copy_file_chunk_to_wasm_memory(AsmfsFileHandle fileHandle, size_t srcOffset, void *dst, size_t dstLength);

// Brings the file from JavaScript side residency over to WebAssembly heap.
// Note: If a copy of the file already exists in the wasm heap, it is freed, and
// the file contents are re-copied(!) to the Wasm heap again from JavaScript side
// backing. This is to support the use case where the file is brought to Wasm side,
// modified, but the modifications are to be discarded/replaced from the JS side
// copy. 
extern uint8_t *emscripten_asmfs_copy_file_to_wasm_memory(AsmfsFileHandle fileHandle);

extern void emscripten_asmfs_copy_file_to_js_memory(AsmfsFileHandle fileHandle);

extern void emscripten_asmfs_discard_file_from_wasm_memory(AsmfsFileHandle fileHandle);

extern EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS emscripten_asmfs_get_file_residency(AsmfsFileHandle fileHandle);

#ifdef __cplusplus
}
#endif
