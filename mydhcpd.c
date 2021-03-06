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

#define dprintf(...) do{}while(0)

#pragma pack(1)

typedef struct eth {
	uint8_t dst[6];
	uint8_t src[6];
	uint16_t type;
} eth_t;

typedef struct arp {
	uint16_t htype;
	uint16_t ptype;
	uint8_t hsize;
	uint8_t psize;
	uint16_t opcode;
	uint8_t sender_mac[6];
	uint8_t sender_ip[4];
	uint8_t target_mac[6];
	uint8_t target_ip[4];
} arp_t;

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
	uint16_t checksum;
} udp_t;

typedef struct bootp {
	uint8_t mtype;
	uint8_t htype;
	uint8_t halen;
	uint8_t hops;
	uint32_t tid;
	uint8_t pad0[2];
	uint16_t flags;
	uint8_t pad1[4];
	uint8_t cli_ip[4];
	uint8_t srv_ip[4];
	uint8_t pad2[4];
	uint8_t cli_mac[6];
	uint8_t mac_pad2[10];
	uint8_t host[64];
	uint8_t boot[128];
	uint32_t magic;
	uint8_t options[60];
} bootp_t;

typedef struct tftp {
	uint16_t opcode;
	uint8_t data[];
} tftp_t;

typedef struct tftp_data {
	struct {
	uint16_t opcode;
	uint16_t blockn;
	} hdr;
	uint8_t data[512];
} tftp_data_t;

#pragma pack()

#define MAX_ETH			1502

#define ETH_TYPE_IP		0x0008
#define ETH_TYPE_ARP	0x0608

#define IP_PROTO_UDP	0x11

#define UDP_PORT_BOOTP 67
#define UDP_PORT_TFTP 69
#define UDP_PORT_TFTP_READ 49008

int send_vnet( int fd, char *buf, int size)
{
	int ret = 0;
	uint32_t nsize;
	int n;
	dprintf( "%s: size=%d\n", __func__, size);
	if (size > MAX_ETH)
	{
		printf( "frame too big for ethernet ? (must send %d, max %d)\n", size, MAX_ETH);
		return 1;
	}
	nsize = htonl( size);
	n = write( fd, &nsize, sizeof( nsize));
	n = write( fd, buf, size);
	if (n == -1)
	{
		perror( "write");
		ret = 2;
	}
	
	return ret;
}

int the_fd = -1;
uint8_t the_mac[6] = { 0x08, 0x00, 0x27, 0xcb, 0x80, 0x2a };
uint8_t the_ip[4] = { 192, 168, 0, 1 };

uint8_t cli_mac[6] = { 0, 0, 0, 0, 0, 0 };
uint8_t cli_ip[4] = { 192, 168, 0, 10 };

#ifdef WIN32
#define PRIzd "d"
#else
#define PRIzd "zd"
#endif

int send_eth( int fd, struct eth *hdr, void *buf, int bufsize)
{
	int ret;
	char *ptr = malloc( sizeof( *hdr) + bufsize);
	uint8_t null_mac[6] = { 0, 0, 0, 0, 0, 0 };
	int i;

	if (!memcmp( hdr->src, null_mac, sizeof( hdr->src)))
	{
		for (i = 0; i < sizeof( hdr->src); i++)
		{
			hdr->src[i] = the_mac[i];
		}
	}
	if (!memcmp( hdr->dst, null_mac, sizeof( hdr->dst)))
	{
		for (i = 0; i < sizeof( hdr->dst); i++)
		{
			hdr->dst[i] = cli_mac[i];
		}
	}
	memcpy( ptr, hdr, sizeof( *hdr));
	memcpy( ptr + sizeof( *hdr), buf, bufsize);
	ret = send_vnet( fd, ptr, sizeof( *hdr) + bufsize);
	free( ptr);
	return ret;
}

int send_ip( int fd, struct ip *hdr, void *buf, int bufsize)
{
	int ret;
	struct eth eth;
	char *ptr = malloc( sizeof( *hdr) + bufsize);
	uint8_t null_ip[4] = { 0, 0, 0, 0 };
	int i;

	if (!memcmp( hdr->src, null_ip, sizeof( hdr->src)))
	{
		for (i = 0; i < sizeof( hdr->src); i++)
		{
			hdr->src[i] = the_ip[i];
		}
	}
	if (!memcmp( hdr->dst, null_ip, sizeof( hdr->dst)))
	{
		for (i = 0; i < sizeof( hdr->dst); i++)
		{
			hdr->dst[i] = cli_ip[i];
		}
	}
	if (!hdr->ttl)
	{
		hdr->ttl = 128;
	}
	if (!hdr->version)
	{
		hdr->version = 0x45;	// size ?
	}
	if (!hdr->len)
	{
		hdr->len = htons( sizeof( *hdr) + bufsize);
	}
	if (!hdr->checksum)
	{
		int i;
		uint32_t cks = 0;
		for (i = 0; i < sizeof( *hdr) / 2; i++)
		{
			uint16_t d = ((uint16_t *)hdr)[i];
			cks += d;
		}
		hdr->checksum = ~((cks & 0xffff) + (cks >> 16));
	}

	memset( &eth, 0, sizeof( eth));
	eth.type = ETH_TYPE_IP;
	memcpy( ptr, hdr, sizeof( *hdr));
	memcpy( ptr + sizeof( *hdr), buf, bufsize);
	ret = send_eth( fd, &eth, ptr, sizeof( *hdr) + bufsize);
	free( ptr);
	return ret;
}

int send_udp( int fd, struct udp *hdr, void *buf, int bufsize)
{
	int ret;
	struct ip ip;
	char *ptr = malloc( sizeof( *hdr) + bufsize);

	memset( &ip, 0, sizeof( ip));
	ip.proto = IP_PROTO_UDP;

	if (!hdr->len)
	{
		hdr->len = htons( sizeof( *hdr) + bufsize);
		dprintf( "%s: auto len, bufsize=%d, total=%" PRIu16 "\n", __func__, bufsize, ntohs( hdr->len));
	}
	dprintf( "%s: len=0x%04" PRIx16 "\n", __func__, hdr->len);
	// XXX UDP checksum ? http://stackoverflow.com/questions/1480580/udp-checksum-calculation, http://www.frameip.com/entete-udp/#3.4_-_Checksum, http://www.faqs.org/rfcs/rfc768.html

	memcpy( ptr, hdr, sizeof( *hdr));
	memcpy( ptr + sizeof( *hdr), buf, bufsize);
	ret = send_ip( fd, &ip, ptr, sizeof( *hdr) + bufsize);
	free( ptr);
	return ret;
}

int manage_tftp( int sport, int dport, char *buf, int size)
{
	struct tftp *hdr = (void *)buf;
	dprintf( "%s: size=%d hdr=%" PRIzd "\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not tftp ?\n");
		return 1;
	}
	
	dprintf( "%s: opcode=%" PRIx16 "\n", __func__, hdr->opcode);
	char *file = (char *)hdr->data;
//	char *mode = 0;
//	int len = strlen( file);
//	if (len)
//		mode = file + len + 1;
	int is_read = 0;
	static int read_sport = 0;
	static int read_dport = 0;
	int last = 0;
	static int blockn = 1;
	switch (hdr->opcode)
	{
#define TFTP_OPCODE_RRQ		0x100
#define TFTP_OPCODE_WRQ		0x200
#define TFTP_OPCODE_DATA	0x300
#define TFTP_OPCODE_ACQ		0x400
#define TFTP_OPCODE_ERROR	0x500
		case TFTP_OPCODE_RRQ:	//RRQ
			dprintf( "RRQ: file=%s mode=%s\n", file, mode);
			if ((dport == UDP_PORT_TFTP) && (!read_sport))
				is_read = 1;
			break;
		case TFTP_OPCODE_ERROR:
		{
//			uint16_t code = *(uint16_t *)(hdr->data + 2);
			dprintf( "ERROR: code=%" PRIx16 "\n", code);
			read_sport = 0;
			break;
		}
		case TFTP_OPCODE_ACQ:	//ACQ
		{
			uint16_t block;
			block = ntohs( *(uint16_t *)hdr->data);
			dprintf( "ACQ: block=%d blockn=%d\n", block, blockn);
			if (last != 1)
			if (block == blockn)
			{
				is_read = 1;
				blockn++;
			}
			break;
		}
		default:
			printf( "%s: unknown opcode %" PRIx16 "\n", __func__, hdr->opcode);
			break;
	}
	
	if (is_read)
	{
		dprintf( "READ: sport=%d dport=%d\n", sport, dport);
		if (read_sport == 0)
		{
			read_sport = sport;
			dport = read_dport = UDP_PORT_TFTP_READ;
		}
		
		if (dport == UDP_PORT_TFTP_READ)
		{
			static int len = 0;
			struct tftp_data tftp_data;
			memset( &tftp_data, 0, sizeof( tftp_data));
			tftp_data.hdr.opcode = TFTP_OPCODE_DATA;
			tftp_data.hdr.blockn = htons( blockn);
	
			static FILE *fd = 0;
			static int fsize = 0;
			static int todo = 0;
			if (blockn == 1)
			{
				last = 0;
				fd = fopen( file, "rb");
				if (!fd)
				{
//					perror( "fopen");
					tftp_data.hdr.opcode = TFTP_OPCODE_ERROR;
					tftp_data.data[len++] = 0;
					tftp_data.data[len++] = 1;
					read_sport = 0;
					blockn = 1;
				}
				else
				{
					fseek( fd, 0, SEEK_END);
					todo = fsize = ftell( fd);
					rewind( fd);
					len = sizeof( tftp_data.data);
				}
			}
			if (fd)
			{
				if (len > todo)
				{
					len = todo;
				}
				fread( tftp_data.data, len, 1, fd);
				if (todo == 0)
				{
					len = 0;
					blockn = 1;
					fclose( fd);
					fd = 0;
					read_sport = 0;
					last = 1;
				}
				else
				{
					todo -= len;
				}
			}
			if (!last)
			{
			dprintf( "about to send %s, len=%d fd=%p, sport=%d dport=%d\n", tftp_data.hdr.opcode == TFTP_OPCODE_DATA ? "DATA" : "ERROR", len, fd, sport, dport);

			struct udp udp;
			memset( &udp, 0, sizeof( udp));
			udp.sport = htons( read_dport);
			if (!read_sport)
				udp.dport = htons( sport);
			else
				udp.dport = htons( read_sport);
			send_udp( the_fd, &udp, &tftp_data, sizeof( tftp_data.hdr) + len);
			}
		}
	}
	
	return 0;
}

int manage_bootps( char *buf, int size)
{
	int ret = -1;
	
	struct bootp *hdr = (void *)buf;
	dprintf( "%s: size=%d hdr=%" PRIzd "\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not bootps ?\n");
		return 1;
	}
	
	uint8_t cli[6];
	dprintf( "%s: client MAC is ", __func__);
	int i;
	for (i = 0; i < 6; i++)
	{
		cli[i] = hdr->cli_mac[i];
		dprintf( ":%02" PRIx8, cli[i]);
	}
	dprintf( "\n");
	dprintf( "magic=%08" PRIx32 "\n", (uint32_t)ntohl( hdr->magic));
	
	int disc = 1;
	int pos = 0;
	while (1)	// parse bootp options
	{
		int type = hdr->options[pos++];
		if (type == 0xff)	// end option
		{
			dprintf( "option end\n");
			break;
		}
		int len = hdr->options[pos++];
		if (type == 53)	// DHCP message type
		{
			dprintf( "option DHCP message type\n");
			if (hdr->options[pos] == 1)
				disc = 1;
			else if (hdr->options[pos] == 3)
				disc = 0;
			break;
		}
		pos += len;
	}
	dprintf( "DHCP %s\n", disc ? "Discovery" : "Request");
	
	struct bootp bootp;
	int bootpcs = sizeof( bootp);

	memset( &bootp, 0, bootpcs);
	bootp.mtype = 2;
	bootp.htype = 1;
	bootp.halen = 6;
	bootp.tid = hdr->tid;
	bootp.flags = 0x80;		// broadcast
	for (i = 0; i < 4; i++)
	{
		bootp.cli_ip[i] = cli_ip[i];
		bootp.srv_ip[i] = the_ip[i];
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
	strcpy( (char *)bootp.boot, "pxelinux.0");
	
	struct udp udp;
	int udps = sizeof( udp);

	memset( &udp, 0, udps);
	udp.sport = htons( 67);
	udp.dport = htons( 68);
	
	ret = send_udp( the_fd, &udp, &bootp, bootpcs);
	if (ret)
	{
		printf( "%s: failed to send udp (%d)\n", __func__, ret);
	}
	
	return ret;
}

int manage_udp( char *buf, int size)
{
	struct udp *hdr = (void *)buf;
	dprintf( "%s: size=%d hdr=%" PRIzd "\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not ip ?\n");
		return 1;
	}
	switch (ntohs( hdr->dport))
	{
		case UDP_PORT_BOOTP:
			manage_bootps( buf + sizeof( *hdr), size - sizeof( *hdr));
			break;
		case UDP_PORT_TFTP:
		case UDP_PORT_TFTP_READ:
			manage_tftp( ntohs( hdr->sport), ntohs( hdr->dport), buf + sizeof( *hdr), size - sizeof( *hdr));
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
	dprintf( "%s: size=%d hdr=%" PRIzd "\n", __func__, size, sizeof( *hdr));
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
			printf( "unknown ip protocol 0x%02x\n", hdr->proto);
			break;
	}
	return 0;
}

int manage_arp( char *buf, int size)
{
	struct arp *hdr = (void *)buf;
	dprintf( "%s: size=%d hdr=%" PRIzd "\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not arp ?\n");
		return 1;
	}

	uint8_t cli[6];
	dprintf( "%s: sender MAC is ", __func__);
	int i;
	for (i = 0; i < 6; i++)
	{
		cli[i] = hdr->sender_mac[i];
		dprintf( ":%02" PRIx8, cli[i]);
	}
	dprintf( "\n");

	int req = 0;
	switch (hdr->opcode)
	{
#define ARP_OPCODE_REQUEST 0x0100
		case ARP_OPCODE_REQUEST:
			req = 1;
			break;
		default:
			printf( "unknown arp opcode 0x%04x\n", hdr->opcode);
			return -1;
	}

	dprintf( "ARP REQUEST\n");

	struct arp arp;

	memset( &arp, 0, sizeof( arp));
	arp.htype = 0x0100;
	arp.ptype = 0x0008;
	arp.hsize = 6;
	arp.psize = 4;
	if (req == 1)
		arp.opcode = 0x0200;
	for (i = 0; i < 6; i++)
	{
		arp.sender_mac[i] = the_mac[i];
		arp.target_mac[i] = hdr->sender_mac[i];
	}
	for (i = 0; i < 4; i++)
	{
		arp.sender_ip[i] = the_ip[i];
		arp.target_ip[i] = cli_ip[i];
	}

	struct eth eth;
	int eths = sizeof( eth);
	memset( &eth, 0, eths);
	for (i = 0; i < 6; i++)
	{
		eth.dst[i] = cli[i];
		eth.src[i] = the_mac[i];
	}
	eth.type = ETH_TYPE_ARP;
	send_eth( the_fd, &eth, &arp, sizeof( arp));

	return 0;
}

int manage_eth( char *buf, int size)
{
	struct eth *hdr = (void *)buf;
	int i;
	dprintf( "%s: size=%d hdr=%" PRIzd "\n", __func__, size, sizeof( *hdr));
	if (size < sizeof( *hdr))
	{
		printf( "not ethernet ?\n");
		return 1;
	}
	for (i = 0; i < sizeof( hdr->src); i++)
	{
		cli_mac[i] = hdr->src[i];
	}
	switch (hdr->type)
	{
		case ETH_TYPE_IP:
			manage_ip( buf + sizeof( *hdr), size - sizeof( *hdr));
			break;
		case ETH_TYPE_ARP:
			manage_arp( buf + sizeof( *hdr), size - sizeof( *hdr));
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
				printf( "frame too big ! got %d max %" PRIzd "\n", size, sizeof( buf));
				exit( 3);
			}
			dprintf( "about to read %d bytes\n", size);
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
