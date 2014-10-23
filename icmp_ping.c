#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define mtod(m,t) (t)m->ip
typedef struct ip {
	struct {
	uint32_t s_addr;
	} ip_src;
	struct {
	uint32_t s_addr;
	} ip_dst;
} ip_t;
typedef struct mbuf {
	struct ip *ip;
} mbuf_t;

int
icmp_user_ping(struct mbuf *m)
{
  register struct ip *ip = mtod(m, struct ip *);

  printf( "%s: src=%lx dst=%lx\n", __func__, (long)ip->ip_src.s_addr, (long)ip->ip_dst.s_addr);

  char buf[1024];
  struct in_addr ia;
  memcpy( &ia, &ip->ip_dst.s_addr, sizeof( ia));
  snprintf( buf, sizeof( buf), "ping -w 2 -c 1 %s", inet_ntoa( ia));
  printf( "%s: about to run cmd : '%s'\n", __func__, buf);
  FILE *in = popen( buf, "r");
  int ok = 0;
  if (in)
  {
    while (!feof( in))
    {
      printf( "%s: about to read..\n", __func__);
      if (!fgets( buf, sizeof( buf), in))
        break;
      printf( "%s: read %s\n", __func__, buf);
      if (strstr( buf, " bytes from "))
      {
        ok = 1;
        break;
      }
    }
    pclose( in);
  }

  if (!ok)
  {
    printf( "%s: ping failed.\n", __func__);
    return -1;
  }
  else
    printf( "%s: ping success !\n", __func__);
  return 0;
}

int main( int argc, char *argv[])
{
	char *dst = "127.0.0.1";
	int arg = 1;

	if (arg < argc)
	{
		dst = argv[arg++];
	}
	struct ip ip;
	struct mbuf m;
	m.ip = &ip;
	ip.ip_dst.s_addr = inet_addr( dst);
	printf( "pinging dst %s\n", dst);
	int n = icmp_user_ping( &m);
	printf( "icmp_user_ping returned %d\n", n);
	return 0;
}

