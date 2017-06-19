#include <stdio.h>
#include <emscripten/emscripten.h>

#include <emscripten/pthread_proxy_main.h>
int emscripten_main(int argc, char **argv)
{
	double devicePixelRatio = emscripten_get_device_pixel_ratio();
	printf("window.devicePixelRatio = %f.\n", devicePixelRatio);
	int result = (devicePixelRatio > 0) ? 1 : 0;
	if (result) {
		printf("Test succeeded!\n");
	}
#ifdef REPORT_RESULT
	REPORT_RESULT(result);
#endif
	return 0;
}
