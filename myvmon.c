#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
	int port = 12346;
	struct sockaddr_in sa;
	int n;
	int s = socket( PF_INET, SOCK_STREAM, 0);
	memset( &sa, 0, sizeof( sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons( port);
	sa.sin_addr.s_addr = inet_addr( "127.0.0.1");
	connect( s, (struct sockaddr *)&sa, sizeof( sa));

	uint32_t payload = 0;
	uint32_t size = sizeof( payload) | 0x80000000;
	uint32_t nsize = htonl( size);
	n = write( s, &nsize, sizeof( nsize));			// size
	n = write( s, &payload, sizeof( payload));		// payload
	
	while (1)
	{
		int ext = 0;
		n = read( s, &nsize, sizeof( nsize));
		if (n == -1)
		{
			perror( "read size");
		}
		if (n == 0)
			break;
		size = ntohl( nsize);
		if (size & 0x80000000)
		{
			size &= 0x7fffffff;
			ext = 1;
		}
		char *buf = malloc( size);
		n = read( s, buf, size);
		if (n == -1)
		{
			perror( "read payload");
		}
		if (n == 0)
			break;
		if (ext)
		{
			memcpy( &payload, buf, sizeof( payload));
			printf( "%s: received extension : size %" PRIu32 " payload %" PRIx32 "\n", __func__, size, payload);
		}
		else
		{
			static int count = 0;
			printf( "%s: skip regular data : size %" PRIu32 ", packet count=%d\n", __func__, size, count);
			count++;
		}
		free( buf);
	}
	
	close( s);
	
	return 0;
}
