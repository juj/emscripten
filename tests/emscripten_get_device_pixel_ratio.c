#include <stdio.h>
#include <emscripten/emscripten.h>

int main()
{
	double devicePixelRatio = emscripten_get_device_pixel_ratio();
	printf("window.devicePixelRatio = %f.\n", devicePixelRatio);
#ifdef REPORT_RESULT
	int result = (devicePixelRatio > 0) ? 1 : 0;
	if (result) {
		printf("Test succeeded!\n");
	}
	REPORT_RESULT();
#endif
}
