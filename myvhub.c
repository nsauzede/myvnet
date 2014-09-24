#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define VERBOSE 0

#define MAX 5
int s[MAX];
int e[MAX];		// these guys support extensions (non-data) (when e[i] == 1)

#ifdef VERBOSE
int verbose = VERBOSE;
#define dprintf(l,...) do{if (l <= verbose){printf( "%d/%d: ", l, verbose);printf(__VA_ARGS__);}}while(0)
#else
#define dprintf(l,...) do{}while(0)
#endif

ssize_t read_full( int fd, void *buf, size_t count)
{
	ssize_t ret = -1;
	ssize_t done = 0;
	
	while (done < count)
	{
		ret = read( fd, buf + done, count - done);
		if (ret <= 0)
			break;
		done += ret;
	}
	if (ret > 0)
		ret = done;
		
	return ret;
}

int main( int argc, char *argv[])
{
	int port = 12346;
	int ss;
	struct sockaddr_in sa;
	int n;
	int on;
	int i;
	int arg = 1;
	
	if (arg < argc)
	{
		sscanf( argv[arg++], "%d", &port);
	}
	ss = socket( PF_INET, SOCK_STREAM, 0);
	on = 1;
	setsockopt( ss, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on));
	memset( &sa, 0, sizeof( sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons( port);
	sa.sin_addr.s_addr = INADDR_ANY;
	bind( ss, (struct sockaddr *)&sa, sizeof( sa));
	listen( ss, MAX);
	for (i = 0; i < MAX; i++)
	{
		s[i] = -1;
	}
	memset( e, 0, sizeof( e));
	printf( "listening on port %d..\n", port);
	while (1)
	{
		int j;
		uint32_t nsize;
		int max = -1;
		fd_set rfds;
		FD_ZERO( &rfds);
		if (ss != -1)
		{
			dprintf( 5, "selecting server\n");
			FD_SET( ss, &rfds);
			if (ss > max)
				max = ss;
		}
		for (i = 0; i < MAX; i++)
		{
			if (s[i] != -1)
			{
				dprintf( 5, "selecting client %d\n", i);
				FD_SET( s[i], &rfds);
				if (s[i] > max)
					max = s[i];
			}
		}
		n = select( max + 1, &rfds, 0, 0, 0);
		if (n == -1)
		{
			perror( "select");
			exit( 1);
		}
		if (n == 0)
		{
			usleep( 100);
			continue;
		}
		for (i = 0; i < MAX; i++)
		{
			if ((s[i] != -1) && FD_ISSET( s[i], &rfds))
			{
				dprintf( 5, "reading client %d fd %d\n", i, s[i]);
				nsize = 0;
				n = read_full( s[i], &nsize, sizeof( nsize));
				if (n == -1)
				{
					perror( "read size");
					exit( 3);
				}
				else if (n == 0)
				{
					dprintf( 5, "client %d disconnected size\n", i);
					close( s[i]);
					s[i] = -1;
					e[i] = 0;
				}
				else
				{
					int extension = 0;
					dprintf( 5, "read client %d fd %d nsize %" PRIx32 "\n", i, s[i], nsize);
					uint32_t size = ntohl( nsize);
					if (size & 0x80000000)
					{
						extension = 1;
						size &= 0x80000000 - 1;
						dprintf( 5, "read client %d fd %d extension size %d\n", i, s[i], size);
						e[i] = 1;
					}
					else
						dprintf( 5, "read client %d fd %d size %d\n", i, s[i], size);
					char *buf = malloc( size);
					n = read_full( s[i], buf, size);
					if (n == -1)
					{
						perror( "read payload");
						exit( 3);
					}
					else if (n == 0)
					{
						printf( "client %d disconnected payload\n", i);
						close( s[i]);
						s[i] = -1;
						e[i] = 0;
					}
					else
					{
						for (j = 0; j < MAX; j++)
						{
							if ((s[j] != -1) && (j != i) && (!extension || e[j]))
							{
								if (extension)
									dprintf( 5, "writing client %d fd %d extension size %d\n", j, s[j], size);
								else
									dprintf( 5, "writing client %d fd %d size %d\n", j, s[j], size);
								n = write( s[j], &nsize, sizeof( nsize));
								n = write( s[j], buf, size);
							}
						}
					}
					free( buf);
				}
				
				if (s[i] == -1)
				{
					// now notify the disconnection to those extension-aware guys
					for (j = 0; j < MAX; j++)
					{
						if ((s[j] != -1) && e[j])
						{
							uint32_t payload = (2 << 24) | i;	// 2<<24 is new connection
							nsize = htonl( sizeof( payload) | 0x80000000);
							printf( "%s: SENDING EXTENSION TO %d !!\n", __func__, j);
							n = write( s[j], &nsize, sizeof( nsize));
							n = write( s[j], &payload, sizeof( payload));
						}
					}
				}
			}
		}
		if ((ss != -1) && FD_ISSET( ss, &rfds))
		{
			dprintf( 5, "accepting client..\n");
			n = accept( ss, 0, 0);
			if (n == -1)
			{
				perror( "accept");
				exit( 2);
			}
			for (i = 0; i < MAX; i++)
			{
				if (s[i] == -1)
					break;
			}
			if (i >= MAX)
			{
				printf( "too much connections (%d)\n", i);
				close( n);
			}
			else
			{
				dprintf( 5, "new client %d\n", i);
				s[i] = n;
				e[i] = 0;
				
				// now notify the new connection to those extension-aware guys
				for (j = 0; j < MAX; j++)
				{
					if ((s[j] != -1) && e[j])
					{
						uint32_t payload = (1 << 24) | i;	// 1<<24 is new connection
						nsize = htonl( sizeof( payload) | 0x80000000);
						printf( "%s: SENDING EXTENSION TO %d !!\n", __func__, j);
						n = write( s[j], &nsize, sizeof( nsize));
						n = write( s[j], &payload, sizeof( payload));
					}
				}
			}
		}
	}
	return 0;
}
