#include <stdio.h>
#include <errno.h>

int socket(int domain, int type, int protocol)
{
	printf( "%s: domain=%d type=%d protocol=%d\n", __func__, domain, type, protocol);
	getchar();
	errno = EINVAL;
	return -1;
}
