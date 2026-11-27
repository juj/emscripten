// Copyright 2026 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <emscripten.h>

EM_JS_DEPS(deps, "$UTF16ToString,$stringToUTF16,$UTF32ToString,$stackSave,$stackRestore,$stackAlloc");

// The UTF-16 and UTF-32 functions take their limits in bytes.  A limit that is
// not a multiple of the code unit size leaves room for only part of the last
// code unit, so that code unit must be neither read nor written.
EM_JS(void, test, (), {
  var sp = stackSave();
  var ptr = stackAlloc(256);

  for (var i = 0; i < 40; ++i) HEAPU16[ptr / 2 + i] = 0x61 + i % 26;
  HEAPU16[ptr / 2 + 40] = 0;
  for (var ignoreNul of [false, true]) {
    out('UTF16ToString' + (ignoreNul ? ', ignoreNul' : ""));
    for (var n of [0, 1, 2, 3, 4, 5, 32, 33, 34, 35]) {
      out('  maxBytesToRead=' + n + ': ' + UTF16ToString(ptr, n, ignoreNul).length + ' code units');
    }
  }

  for (var i = 0; i < 40; ++i) HEAPU32[ptr / 4 + i] = 0x61 + i % 26;
  HEAPU32[ptr / 4 + 40] = 0;
  for (var ignoreNul of [false, true]) {
    out('UTF32ToString' + (ignoreNul ? ', ignoreNul' : ""));
    for (var n of [0, 1, 3, 4, 5, 7, 8, 9]) {
      out('  maxBytesToRead=' + n + ': ' + UTF32ToString(ptr, n, ignoreNul).length + ' code units');
    }
  }

  out('stringToUTF16');
  for (var n = 0; n <= 13; ++n) {
    HEAPU8.fill(0xAA, ptr, ptr + 32);
    var ret = stringToUTF16('hello', ptr, n);
    var written = 32;
    while (written > 0 && HEAPU8[ptr + written - 1] == 0xAA) --written;
    out('  maxBytesToWrite=' + n + ': returned ' + ret + ', wrote ' + written + ' bytes');
  }

  stackRestore(sp);
});

int main() {
  test();
  return 0;
}
