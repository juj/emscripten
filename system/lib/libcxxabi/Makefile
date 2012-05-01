OBJECTS = \
  src/private_typeinfo.bc \
  $(NULL)
  #src/cxa_vector.bc \
  #src/cxa_virtual.bc \
  #src/temporary.bc \
  #src/cxa_guard.bc \
  #src/cxa_unexpected.bc \
  #src/cxa_exception.bc \
  #src/cxa_aux_runtime.bc \
  #src/exception.bc \
  #src/stdexcept.bc \
  #src/abort_message.bc \
  #src/cxa_personality.bc \
  #src/cxa_new_delete.bc \
  #src/cxa_handlers.bc \
  #src/cxa_exception_storage.bc \
  #src/typeinfo.bc \
  #src/cxa_demangle.bc

all: libcxxabi.bc

%.bc: %.cpp
	$(CXX) -I./include $< -std=c++0x -DEMSCRIPTEN -D__STDC__ -U_WIN32 -UWIN32 -U__i386__ -U__x86_64__ -U__i386 -U__x86_64 -U__SSE__ -U__SSE2__ -U__MMX__ -UX87_DOUBLE_ROUNDING -UHAVE_GCC_ASM_FOR_X87 -U__STRICT_ANSI__ -U__CYGWIN__ -U__APPLE__ -Xclang -isystemC:/Projects/emscripten/system/include -Xclang -isystemC:/Projects/emscripten/system/include/bsd -Xclang -isystemC:/Projects/emscripten/system/include/libc -Xclang -isystemC:/Projects/emscripten/system/include/libcxx -Xclang -isystemC:/Projects/emscripten/system/include/gfx -Xclang -isystemC:/Projects/emscripten/system/include/net -Xclang -isystemC:/Projects/emscripten/system/include/SDL -m32 -Xclang -triple=i386-pc-linux-gnu -c -nostdinc -Xclang -nobuiltininc -Xclang -nostdinc++ -Xclang -nostdsysteminc -emit-llvm -o $@

libcxxabi.bc: $(OBJECTS)
	$(CXXLINK) $(OBJECTS) -o libcxxabi.bc

