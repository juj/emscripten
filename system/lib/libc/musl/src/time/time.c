#include <time.h>
#include <sys/time.h>

#ifdef __EMSCRIPTEN__
// XXX Emscripten: Custom implementation to route to Date.now()
extern double __emscripten_date_now();
time_t time(time_t *t)
{
	int d = (int)(__emscripten_date_now() / 1000.0);
	if (t) *t = (time_t)d;
	return (time_t)d;
}
#else
#include "syscall.h"

int __clock_gettime(clockid_t, struct timespec *);

time_t time(time_t *t)
{
	struct timespec ts;
	__clock_gettime(CLOCK_REALTIME, &ts);
	if (t) *t = ts.tv_sec;
	return ts.tv_sec;
}
#endif
