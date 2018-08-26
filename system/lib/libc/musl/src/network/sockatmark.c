#include <sys/socket.h>
#include <sys/ioctl.h>

#ifndef __EMSCRIPTEN__
int sockatmark(int s)
{
	int ret;
	if (ioctl(s, SIOCATMARK, &ret) < 0)
		return -1;
	return ret;
}
#endif
