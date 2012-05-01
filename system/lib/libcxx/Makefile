OBJECTS = \
  algorithm.bc \
  condition_variable.bc \
  future.bc \
  iostream.bc \
  memory.bc \
  random.bc \
  stdexcept.bc \
  system_error.bc \
  utility.bc \
  bind.bc \
  debug.bc \
  hash.bc \
  mutex.bc \
  string.bc \
  thread.bc \
  valarray.bc \
  chrono.bc \
  exception.bc \
  ios.bc \
  locale.bc \
  regex.bc \
  strstream.bc \
  typeinfo.bc

all: libcxx.bc

%.bc: %.cpp
	$(CXX) $< -std=c++0x -DEMSCRIPTEN -D__STDC__ -U_WIN32 -UWIN32 -U__i386__ -U__x86_64__ -U__i386 -U__x86_64 -U__SSE__ -U__SSE2__ -U__MMX__ -UX87_DOUBLE_ROUNDING -UHAVE_GCC_ASM_FOR_X87 -U__STRICT_ANSI__ -U__CYGWIN__ -U__APPLE__ -Xclang -isystemC:/Projects/emscripten/system/include -Xclang -isystemC:/Projects/emscripten/system/include/bsd -Xclang -isystemC:/Projects/emscripten/system/include/libc -Xclang -isystemC:/Projects/emscripten/system/include/libcxx -Xclang -isystemC:/Projects/emscripten/system/include/gfx -Xclang -isystemC:/Projects/emscripten/system/include/net -Xclang -isystemC:/Projects/emscripten/system/include/SDL -m32 -Xclang -triple=i386-pc-linux-gnu -c -nostdinc -Xclang -nobuiltininc -Xclang -nostdinc++ -Xclang -nostdsysteminc -emit-llvm -o $@

libcxx.bc: $(OBJECTS)
	$(CXXLINK) $(OBJECTS) -v -o libcxx.bc

