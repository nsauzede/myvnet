#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>

#define dprintf(...) do{}while(0)

int main( int argc, char *argv[])
{
	int tport = 12346;
	char *tsrv = "127.0.0.1";
	char *intf = "eth1";
	int n;
	int arg = 1;
	int st, sr;
	
	if (arg < argc)
	{
		intf = argv[arg++];
	}
	
	st = socket( PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sa;
	memset( &sa, 0, sizeof( sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons( tport);
	sa.sin_addr.s_addr = inet_addr( tsrv);
	n = connect( st, (struct sockaddr *)&sa, sizeof( sa));
	if (n == -1)
	{
		perror( "connect");
		exit( 1);
	}
//	int s = socket( PF_PACKET, SOCK_RAW, IPPROTO_RAW);
	sr = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sr == -1)
	{
		perror( "socket");
		exit( 1);
	}
	struct ifreq ifr;
	memset( &ifr, 0, sizeof( ifr));
	strncpy( ifr.ifr_name, intf, IFNAMSIZ);
	ioctl( sr, SIOCGIFINDEX, &ifr);
	printf( "index for if '%s' is %d\n", intf, ifr.ifr_ifindex);
	struct sockaddr_ll sll;
	memset( &sll, 0, sizeof( sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons( ETH_P_ALL);
	n = bind( sr, (struct sockaddr *)&sll, sizeof( sll));
	if (n == -1)
	{
		perror( "bind");
		exit( 2);
	}
	while (1)
	{
		char buf[1600];
		fd_set rfds;
		int max = -1;
		FD_ZERO( &rfds);
		if (st != -1)
		{
			FD_SET( st, &rfds);
			if (st > max)
				max = st;
		}
		if (sr != -1)
		{
			FD_SET( sr, &rfds);
			if (sr > max)
				max = sr;
		}
		n = select( max + 1, &rfds, 0, 0, 0);
		if (n == -1)
		{
			perror( "select");
			exit( 5);
		}
		if (n == 0)
		{
			printf( "select returned 0 ?\n");
			exit( 5);
		}
		if ((st != -1) && FD_ISSET( st, &rfds))
		{
			uint32_t nsize, size;
			dprintf( "reading size from st..\n");
			n = read( st, &nsize, sizeof( nsize));
			if (n <= 0)
			{
				if (n == 0)
					printf( "st hangup\n");
				else
					perror( "read size st");
				exit( 2);
			}
			size = ntohl( nsize);
			dprintf( "read st size returned %d, size=%" PRIx32 "\n", n, size);
			if (size > sizeof( buf))
			{
				printf( "read size=%" PRIx32 " too large, max=%zd\n", size, sizeof( buf));
				exit( 1);
			}
			dprintf( "reading from st..\n");
			n = read( st, buf, size);
			dprintf( "read st returned %d\n", n);
			n = write( sr, buf, n);
		}
		if ((sr != -1) && FD_ISSET( sr, &rfds))
		{
			uint32_t nsize, size;
			dprintf( "reading from sr..\n");
			n = read( sr, buf, sizeof( buf));
			if (n <= 0)
			{
				if (n == 0)
					printf( "sr hangup\n");
				else
					perror( "read sr");
				exit( 1);
			}
			size = n;
			nsize = htonl( size);
			dprintf( "read sr returned %d\n", n);
			n = write( st, &nsize, sizeof( nsize));
			n = write( st, buf, size);
		}
	}
	return 0;
}
