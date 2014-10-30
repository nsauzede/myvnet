#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "myvutils.h"

#pragma pack(1)

typedef struct eth {
	uint8_t dst[6];
	uint8_t src[6];
	uint16_t type;
} eth_t;

typedef struct ip {
	uint8_t version;
	uint8_t option;
	uint16_t len;
	uint8_t pad0[4];
	uint8_t ttl;
	uint8_t proto;
	uint16_t checksum;
	uint8_t src[4];
	uint8_t dst[4];
} ip_t;

typedef struct udp {
	uint16_t sport;
	uint16_t dport;
	uint16_t len;
	uint8_t pad1[2];
} udp_t;

typedef struct bootp {
	uint8_t mtype;
	uint8_t htype;
	uint8_t halen;
	uint8_t hops;
	uint32_t tid;
	uint8_t pad0[8];
	uint8_t cli_ip[4];
	uint8_t srv_ip[4];
	uint8_t pad1[4];
	uint8_t cli_mac[6];
	uint8_t pad2[10+64+128];
	uint32_t magic;
	uint8_t options[300-66+32-4-10-64-128];
} bootp_t;

#pragma pack()

#define MAX_ETH			1500
#define ETH_TYPE_IP		0x0008
#define IP_PROTO_UDP	0x11

int send_vnet( int fd, char *buf, int size)
{
	uint32_t nsize;
	int n;
	printf( "%s: size=%d\n", __func__, size);
	if (size > MAX_ETH)
	{
		printf( "frame too big for ethernet ? (must send %d, max %d)\n", size, MAX_ETH);
		return 1;
	}
	nsize = htonl( size);
	n = write( fd, &nsize, sizeof( nsize));
	n = write( fd, buf, size);
	
	return 0;
}

int the_fd = -1;
uint8_t the_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
uint8_t the_ip[4] = { 192, 168, 0, 1 };

uint8_t cli_ip[4] = { 192, 168, 0, 11 };

int manage_bootps( char *buf, int size)
{
	struct bootp *hdr = (void *)buf;
	printf( "%s: size=%d hdr=%zd\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not bootps ?\n");
		return 1;
	}
	
	uint8_t cli[6];
	cli[0] = hdr->cli_mac[0];
	cli[1] = hdr->cli_mac[1];
	cli[2] = hdr->cli_mac[2];
	cli[3] = hdr->cli_mac[3];
	cli[4] = hdr->cli_mac[4];
	cli[5] = hdr->cli_mac[5];
	printf( "%s: client MAC is ", __func__);
	int i;
	for (i = 0; i < 6; i++)
	{
		printf( ":%02" PRIx8, cli[i]);
	}
	printf( "\n");
	printf( "magic=%08" PRIx32 "\n", ntohl( hdr->magic));
	
	int disc = 1;
	int pos = 0;
	while (1)	// parse bootp options
	{
		int type = hdr->options[pos++];
		if (type == 0xff)	// end option
		{
			printf( "option end\n");
			break;
		}
		int len = hdr->options[pos++];
		if (type == 53)	// DHCP message type
		{
			printf( "option DHCP message type\n");
			if (hdr->options[pos] == 1)
				disc = 1;
			else if (hdr->options[pos] == 3)
				disc = 0;
			break;
		}
		pos += len;
	}
	printf( "DHCP %s\n", disc ? "Discovery" : "Request");
	
	char _eth[MAX_ETH];
	int _size = 0;

	struct bootp bootp;
	int bootpcs = sizeof( bootp);
	struct udp udp;
	int udps = sizeof( udp);
	struct ip ip;
	int ips = sizeof( ip);
	struct eth eth;
	int eths = sizeof( eth);
	
	memset( &bootp, 0, bootpcs);
	bootp.mtype = 2;
	bootp.htype = 1;
	bootp.halen = 6;
	bootp.tid = hdr->tid;
	for (i = 0; i < 4; i++)
	{
		bootp.cli_ip[i] = cli_ip[i];
	}
	for (i = 0; i < 6; i++)
	{
		bootp.cli_mac[i] = cli[i];
	}
	bootp.magic = hdr->magic;
	
	pos = 0;
	bootp.options[pos++] = 53;
	bootp.options[pos++] = 1;
	if (disc)
	{
		// offer
		bootp.options[pos++] = 2;
	}
	else	// request
	{
		// acq
		bootp.options[pos++] = 5;
	}
	// end
	bootp.options[pos++] = 0xff;
	
	memset( &udp, 0, udps);
	udp.sport = htons( 67);
	udp.dport = htons( 68);
	udp.len = htons( udps + bootpcs);

	memset( &ip, 0, ips);
	ip.version = 0x45;	// size ?
	ip.len = htons( ips + udps + bootpcs);
	ip.ttl = 128;
	ip.proto = IP_PROTO_UDP;
	ip.checksum = htons( 0xb848);
	for (i = 0; i < 4; i++)
	{
		ip.src[i] = the_ip[i];
		ip.dst[i] = cli_ip[i];
	}
	_size += ips;

	memset( &eth, 0, eths);
	for (i = 0; i < 6; i++)
	{
		eth.dst[i] = cli[i];
		eth.src[i] = the_mac[i];
	}
	eth.type = 0x0008;
	_size += eths;
	
	_size += udps;
	_size += bootpcs;
	if (_size > sizeof( _eth))
	{
		printf( "%s: bootpc frame too big (_size=%d, max=%zd)\n", __func__, _size, sizeof( eth));
		return 1;
	}
	pos = 0;
	memcpy( _eth + pos, &eth, eths);
	pos += eths;
	memcpy( _eth + pos, &ip, ips);
	pos += ips;
	memcpy( _eth + pos, &udp, udps);
	pos += udps;
	memcpy( _eth + pos, &bootp, bootpcs);
	pos += bootpcs;
	send_vnet( the_fd, _eth, _size);
	
	return 0;
}

int manage_udp( char *buf, int size)
{
	struct udp *hdr = (void *)buf;
	printf( "%s: size=%d hdr=%zd\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not ip ?\n");
		return 1;
	}
	switch (hdr->dport)
	{
		case 0x4300:
			manage_bootps( buf + sizeof( *hdr), size - sizeof( *hdr));
			break;
		default:
			printf( "unknown ip port 0x%01x\n", hdr->dport);
			break;
	}
	return 0;
}

int manage_ip( char *buf, int size)
{
	struct ip *hdr = (void *)buf;
	printf( "%s: size=%d hdr=%zd\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not ip ?\n");
		return 1;
	}
	switch (hdr->proto)
	{
		case IP_PROTO_UDP:
			manage_udp( buf + sizeof( *hdr), size - sizeof( *hdr));
			break;
		default:
			printf( "unknown ip protocol 0x%01x\n", hdr->proto);
			break;
	}
	return 0;
}

int manage_eth( char *buf, int size)
{
	struct eth *hdr = (void *)buf;
	printf( "%s: size=%d hdr=%zd\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not ethernet ?\n");
		return 1;
	}
	switch (hdr->type)
	{
		case ETH_TYPE_IP:
			manage_ip( buf + sizeof( *hdr), size - sizeof( *hdr));
			break;
		default:
			printf( "unknown ethernet type 0x%04x\n", hdr->type);
			break;
	}
	return 0;
}

int main( int argc, char *argv[])
{
	int s = -1;
	int port = VNET_PORT;
	struct sockaddr_in sa;
	int n;
	
	s = socket( PF_INET, SOCK_STREAM, 0);
	memset( &sa, 0, sizeof( sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons( port);
	sa.sin_addr.s_addr = inet_addr( "127.0.0.1");
	n = connect( s, (struct sockaddr *)&sa, sizeof( sa));
	if (n == -1)
	{
		perror( "connect");
		exit( 1);
	}
	printf( "connected to port %d\n", port);
	while (1)
	{
		fd_set rfds;
		int max = -1;
		FD_ZERO( &rfds);
		if (s != -1)
		{
			FD_SET( s, &rfds);
			if (s > max)
				max = s;
		}
		n = select( max + 1, &rfds, 0, 0, 0);
		if (n == -1)
		{
			perror( "select");
			exit( 1);
		}
		else if (n == 0)
		{
			printf( "select returned 0 ?\n");
			exit( 2);
		}
		if (s != -1 && FD_ISSET( s, &rfds))
		{
			uint32_t size, nsize;
			char buf[1600];
			n = read_full( s, &nsize, sizeof( nsize));
			if (n <= 0)
			{
				printf( "read_full size returned %d\n", n);
				exit( 4);
			}
			size = ntohl( nsize);
			if (size > sizeof( buf))
			{
				printf( "frame too big ! got %d max %zd\n", size, sizeof( buf));
				exit( 3);
			}
			printf( "about to read %d bytes\n", size);
			n = read_full( s, buf, size);
			if (n <= 0)
			{
				printf( "read_full payload returned %d\n", n);
				exit( 5);
			}
			the_fd = s;
			manage_eth( buf, size);
		}
	}
	close( s);
	return 0;
}