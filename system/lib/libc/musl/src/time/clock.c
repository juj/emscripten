#include <time.h>
#include <limits.h>

#ifdef __EMSCRIPTEN__
extern double __emscripten_date_now();
extern double __emscripten_date_start();
clock_t clock()
{
	return (clock_t)((__emscripten_date_now() - __emscripten_date_start()) * ((double)CLOCKS_PER_SEC / 1000.0));
}
#else
int __clock_gettime(clockid_t, struct timespec *);

clock_t clock()
{
	struct timespec ts;

	if (__clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts))
		return -1;

	if (ts.tv_sec > LONG_MAX/1000000
	 || ts.tv_nsec/1000 > LONG_MAX-1000000*ts.tv_sec)
		return -1;

	return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}
#endif
