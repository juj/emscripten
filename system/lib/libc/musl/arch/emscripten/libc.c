#include <libc.h>

struct __libc * EMSCRIPTEN_KEEPALIVE emscripten_get_global_libc()
{
	return &libc;
}
