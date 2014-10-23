#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main()
{
	int s = socket( PF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if (s == -1)
		perror( "socket");
	return s;
}
