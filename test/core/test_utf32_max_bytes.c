// Copyright 2026 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <emscripten.h>

EM_JS_DEPS(deps, "$stringToUTF32,$stackSave,$stackRestore,$stackAlloc");

// stringToUTF32() must not write more than maxBytesToWrite bytes, including
// the null terminator.
EM_JS(void, test, (), {
  var sp = stackSave();
  var ptr = stackAlloc(64);

  for (var n = 0; n <= 13; ++n) {
    HEAPU8.fill(0xAA, ptr, ptr + 64);
    var ret = stringToUTF32('hello', ptr, n);
    var written = 64;
    while (written > 0 && HEAPU8[ptr + written - 1] == 0xAA) --written;
    out('maxBytesToWrite=' + n + ': returned ' + ret + ', wrote ' + written + ' bytes');
  }

  stackRestore(sp);
});

int main() {
  test();
  return 0;
}
