#include <emscripten/asmfs.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

void onFinished(int httpStatusResult, AsmfsFileHandle file, void *userData)
{
  printf("HTTP download finished with HTTP result %d\n", httpStatusResult);
  assert(file); // Not expecting this download to fail

  size_t sz = emscripten_asmfs_get_file_size(file);
  assert(sz == 6407);

  const char *filename = emscripten_asmfs_get_file_name(file);
  assert(!strcmp(filename, "/gears.png"));

  uint8_t *data = emscripten_asmfs_get_file_ptr(file);
  assert(data == 0); // The file was downloaded to JS side, so wasm data pointer should not be available.

  data = emscripten_asmfs_copy_file_to_wasm_heap(file);
  assert(data != 0); // The file should now be present in Wasm side, so pointer should be available.

  emscripten_asmfs_discard_file_from_wasm_heap(file);
  data = emscripten_asmfs_get_file_ptr(file);
  assert(data == 0); // The file should now again be evicted from Wasm side.

  emscripten_asmfs_dump_tree();
}

int main()
{
  emscripten_asmfs_async_file_fetch_to_js("gears.png", "/", onFinished, /*userData=*/0);
}
