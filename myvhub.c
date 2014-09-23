#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX 10
int s[MAX];

#if 1
#define dprintf(l,...) printf(__VA_ARGS__)
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
	while (1)
	{
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
				uint32_t nsize = 0;
				n = read_full( s[i], &nsize, sizeof( nsize));
				if (n == -1)
				{
					perror( "read size");
					exit( 3);
				}
				else if (n == 0)
				{
					printf( "client %d disconnected size\n", i);
					close( s[i]);
					s[i] = -1;
				}
				else
				{
					int skip = 0;
					uint32_t size = ntohl( nsize);
					if (size & 0x80000000)
					{
						skip = 1;
						size &= 0x80000000 - 1;
					}
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
					}
					dprintf( 5, "read client %d fd %d size %d\n", i, s[i], size);
					if (!skip)
					{
						int j;
						for (j = 0; j < MAX; j++)
						{
							if ((s[j] != -1) && (j != i))
							{
								dprintf( 5, "writing client %d fd %d size %d\n", j, s[j], size);
								n = write( s[j], &nsize, sizeof( nsize));
								n = write( s[j], buf, size);
							}
						}
					}
					free( buf);
				}
			}
		}
		if ((ss != -1) && FD_ISSET( ss, &rfds))
		{
			for (i = 0; i < MAX; i++)
			{
				if (s[i] == -1)
					break;
			}
			if (i >= MAX)
			{
				printf( "too much connections (%d)\n", i);
				exit( 2);
			}
			dprintf( 5, "accepting client %d\n", i);
			s[i] = accept( ss, 0, 0);
		}
	}
	return 0;
}
