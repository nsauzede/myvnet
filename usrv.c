#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main( int argc, char *argv[])
{
	int uport = 10001;
	char *srv = 0;
	int port = 12346;
	int n, ss, s;
	struct sockaddr_in sa, usa;
	int arg = 1;
	
	if (arg < argc)
	{
		sscanf( argv[arg++], "%d", &uport);
		if (arg < argc)
		{
			srv = argv[arg++];
			if (arg < argc)
			{
				sscanf( argv[arg++], "%d", &port);
			}
		}
	}
	
	if (!srv)
	{
		printf( "Usage: %s <UDP_port> <TCP_srv> <TCP_port>\n", argv[0]);
		exit( 1);
	}
	
	s = socket( PF_INET, SOCK_STREAM, 0);
	memset( &sa, 0, sizeof( sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons( port);
	sa.sin_addr.s_addr = inet_addr( srv);
	printf( "connecting to TCP %s:%d..\n", srv, port);
	n = connect( s, (struct sockaddr *)&sa, sizeof( sa));
	if (n == -1)
	{
		perror( "connect");
		exit( 1);
	}

	ss = socket( PF_INET, SOCK_DGRAM, 0);
	memset( &usa, 0, sizeof( usa));
	usa.sin_family = AF_INET;
	usa.sin_port = htons( uport);
	usa.sin_addr.s_addr = INADDR_ANY;
	n = bind( ss, (struct sockaddr *)&usa, sizeof( usa));
	if (n == -1)
	{
		perror( "bind");
		exit( 1);
	}
	printf( "bound to listen on UDP port %d..\n", uport);
	while (1)
	{
		socklen_t ssa = sizeof( usa);
		char buf[1600];
		fd_set rfds;
		int max = -1;
		
		FD_ZERO( &rfds);
		if (s != -1)
		{
			FD_SET( s, &rfds);
			if (s > max)
				max = s;
		}
		if (ss != -1)
		{
			FD_SET( ss, &rfds);
			if (ss > max)
				max = ss;
		}
		n = select( max + 1, &rfds, 0, 0, 0);
		if (n == -1)
		{
			perror( "accept");
			exit( 3);
		}
		if ((ss != -1) && FD_ISSET( ss, &rfds))
		{
		memset( buf, 0, sizeof( buf));
		n = recvfrom( ss, buf, sizeof( buf) - 1, 0, (struct sockaddr *)&usa, &ssa);
		if (n == -1)
		{
			perror( "recvfrom");
			exit( 1);
		}
		else if (n == 0)
		{
			printf( "hangup\n");
			exit( 2);
		}
		printf( "recvfrom returned %d from %s\n", n, inet_ntoa( usa.sin_addr));
		uint32_t size = n;
		uint32_t nsize = htonl( size);
		n = write( s, &nsize, sizeof( nsize));
		n = write( s, buf, size);
		}
		
		if ((s != -1) && FD_ISSET( s, &rfds))
		{
			uint32_t nsize = 0, size;
			n = read( s, &nsize, sizeof( nsize));
			size = ntohl( nsize);
			n = read( s, buf, size);
			
			n = sendto( ss, buf, size, 0, (struct sockaddr *)&usa, ssa);
		}
	}

	return 0;
}
