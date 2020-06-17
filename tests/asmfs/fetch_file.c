#include <emscripten/asmfs.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

uint64_t checkSumData(uint8_t *data, size_t numBytes)
{
  uint64_t checksum = 0;
  uint8_t *end = data + numBytes;
  while(data < end)
    checksum = checksum * 23413 + *data++;

  return checksum;
}

void onFinished(int httpStatusResult, AsmfsFileHandle file, void *userData)
{
  assert((uintptr_t)userData == 33);
  assert(httpStatusResult == 200);
  assert(file);

  size_t sz = emscripten_asmfs_get_file_size(file);
  assert(sz == 6407);

  const char *filename = emscripten_asmfs_get_file_name(file);
  assert(!strcmp(filename, "/gears.png"));

  uint8_t *data = emscripten_asmfs_get_file_ptr(file);
  assert(data == 0); // The file was downloaded to JS side, so wasm data pointer should not be available.

  data = emscripten_asmfs_copy_file_to_wasm_memory(file);
  assert(data != 0); // The file should now be present in Wasm side, so pointer should be available.
  assert(checkSumData(data, sz) == 16167567741171036664ull);

  emscripten_asmfs_discard_file_from_wasm_memory(file);
  data = emscripten_asmfs_get_file_ptr(file);
  assert(data == 0); // The file should now again be evicted from Wasm side.

  emscripten_asmfs_dump_tree();
}

int main()
{
  emscripten_asmfs_async_file_fetch("gears.png", "/", EMSCRIPTEN_ASMFS_JS_RESIDENT, onFinished, /*userData=*/(void*)33);
}
