/*
 * Copyright 2013 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <emscripten.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

EM_JS_DEPS(deps, "$callUserCallback");

bool test_complete = false;

EMSCRIPTEN_KEEPALIVE
void finish() {
  printf("finish\n");
  test_complete = true;
  emscripten_force_exit(0);
}

void cleanup() {
  // If the test failed, then delete test files from IndexedDB so that the test
  // runner will not leak test state to subsequent tests that reuse this same
  // file.
  printf("cleaning up test files\n");
  unlink("/working1/empty.txt");
  unlink("/working1/waka.txt");
  unlink("/working1/moar.txt");
  rmdir("/working1/dir");
  EM_ASM(FS.syncfs(function(){})); // And persist deleted changes
}

EMSCRIPTEN_KEEPALIVE
void test() {
  int fd, res;
  struct stat st;

#if FIRST
  printf("running test FIRST half ..\n");

  // Run cleanup first in case a previous test failed half way through.
  cleanup();

  // for each file, we first make sure it doesn't currently exist
  // (we delete it at the end of !FIRST).  We then test an empty
  // file plus two files each with a small amount of content

  // the empty file
  res = stat("/working1/empty.txt", &st);
  assert(res == -1 && errno == ENOENT && "stat /working1/empty.txt succeeded, even though it should have failed");

  fd = open("/working1/empty.txt", O_RDWR | O_CREAT, 0666);
  assert(fd != -1 && "Unable to open /working1/empty.txt with O_RDWR | O_CREAT");
  res = close(fd);
  assert(res == 0 && "Unable to close /working1/empty.txt with O_RDWR | O_CREAT");

  // a file whose contents are just 'az'
  res = stat("/working1/waka.txt", &st);
  assert(res == -1 && errno == ENOENT && "stat /working1/waka.txt succeeded, even though it should have failed");
  fd = open("/working1/waka.txt", O_RDWR | O_CREAT, 0666);
  assert(fd != -1 && "Unable to open /working1/waka.txt with O_RDWR | O_CREAT");
  res = write(fd, "az", 2);
  assert(res == 2 && "Unable to write two bytes to /working1/waka.txt");
  res = close(fd);
  assert(res == 0 && "Unable to close /working1/waka.txt");

  // a file whose contents are random-ish string set by the test_browser.py file
  res = stat("/working1/moar.txt", &st);
  assert(res == -1 && errno == ENOENT && "stat /working1/moar.txt succeeded, even though it should have failed");
  fd = open("/working1/moar.txt", O_RDWR | O_CREAT, 0666);
  assert(fd != -1 && "Unable to open /working1/moar.txt with O_RDWR | O_CREAT");
  res = write(fd, SECRET, strlen(SECRET));
  assert(res == strlen(SECRET) && "Unable to write several bytes to /working1/moar.txt");
  res = close(fd);
  assert(res == 0 && "Unable to close /working1/moar.txt");

  // a directory
  res = stat("/working1/dir", &st);
  assert(res == -1 && errno == ENOENT && "stat /working1/dir.txt succeeded, even though it should have failed");
  res = mkdir("/working1/dir", 0777);
  assert(res == 0 && "mkdir /working1/dir failed, even though it should have succeeded");

#else
  printf("running test SECOND half ..\n");

  // does the empty file exist?
  fd = open("/working1/empty.txt", O_RDONLY);
  assert(fd != -1 && "SECOND: Unable to open /working1/empty.txt");
  res = close(fd);
  assert(res == 0 && "SECOND: Unable to close /working1/empty.txt");
  res = unlink("/working1/empty.txt");
  assert(res == 0 && "SECOND: Unable to unlink /working1/empty.txt");

  // does the 'az' file exist, and does it contain 'az'?
  fd = open("/working1/waka.txt", O_RDONLY);
  assert(fd != -1 && "SECOND: Unable to open /working1/waka.txt");
  {
    char bf[4];
    int bytes_read = read(fd,&bf[0],sizeof(bf));
    assert(bytes_read >= 0 && "SECOND: read on /working1/waka.txt returned a negative value");
    assert(bytes_read > 0 && "SECOND: read on /working1/waka.txt returned a zero");
    assert(bytes_read == 2 && "SECOND: reading /working1/waka.txt did not produce exactly two bytes");
    assert(bf[0] == 'a' && bf[1] == 'z' && "SECOND: reading /working1/waka.txt did not produce expected characters");
  }
  res = close(fd);
  assert(res == 0 && "SECOND: Unable to close /working1/waka.txt");
  res = unlink("/working1/waka.txt");
  assert(res == 0 && "SECOND: Unable to unlink /working1/waka.txt");

  // does the random-ish file exist and does it contain SECRET?
  fd = open("/working1/moar.txt", O_RDONLY);
  assert(fd != -1 && "SECOND: Unable to open /working1/moar.txt");
  {
    char bf[256];
    int bytes_read = read(fd,&bf[0],sizeof(bf));
    assert(bytes_read >= 0 && "SECOND: read on /working1/moar.txt returned a negative value");
    assert(bytes_read > 0 && "SECOND: read on /working1/moar.txt returned a zero");
    assert(bytes_read == strlen(SECRET) && "SECOND: reading /working1/moar.txt did not produce expected number of bytes");
    bf[strlen(SECRET)] = 0;
    assert(strcmp(bf, SECRET) == 0 && "SECOND: reading /working1/moar.txt did not produce expected characters");
  }
  res = close(fd);
  assert(res == 0 && "SECOND: Unable to close /working1/moar.txt");
  res = unlink("/working1/moar.txt");
  assert(res == 0 && "SECOND: Unable to unlink /working1/moar.txt");

  // does the directory exist?
  res = stat("/working1/dir", &st);
  assert(res == 0 && "SECOND: Unable to stat /working1/dir");
  assert(S_ISDIR(st.st_mode) "SECOND: stat /working1/dir did not report a directory");
  res = rmdir("/working1/dir");
  assert(res == 0 && "SECOND: Unable to rmdir /working1/dir");

#endif

  printf("done test ..\n");

#if EXTRA_WORK && !FIRST
  EM_ASM(
    for (var i = 0; i < 100; i++) {
      FS.syncfs(function (err) {
        assert(!err && 'FS.syncfs failed in pass EXTRA_WORK && !FIRST!');
        console.log('extra work');
      });
    }
  );
#endif

#ifdef IDBFS_AUTO_PERSIST
  finish();
#else
  // sync from memory state to persisted and then
  // run 'finish'
  EM_ASM({
    // Ensure IndexedDB is closed at exit.
    var orig = Module['onExit'];
    Module['onExit'] = (status) => {
      assert(Object.keys(IDBFS.dbs).length == 0);
      orig(status);
    };
    FS.syncfs((err) => {
      assert(!err && 'FS.syncfs failed in pass !IDBFS_AUTO_PERSIST');
      callUserCallback(_finish);
    });
    });
#endif
}

int main() {
  EM_ASM(
    FS.mkdir('/working1');
    FS.mount(IDBFS, {
#ifdef IDBFS_AUTO_PERSIST
      autoPersist: true
#endif
    }, '/working1');

#if !FIRST
    // syncfs(true, f) should not break on already-existing directories:
    FS.mkdir('/working1/dir');
#endif

    // sync from persisted state into memory and then
    // run the 'test' function
    FS.syncfs(true, function (err) {
      assert(!err && 'FS.syncfs failed in main');
      callUserCallback(_test);
    });
  );

  emscripten_exit_with_live_runtime();
  return 0;
}
