/*
	rpcap.exe : simple remote pcap sniffer
		=> connects to remote vnet server as a client
		=> dumps a pcap file on stdout	

	Usage : ./rpcap.exe [host_ip port]
		default is host_ip=127.0.0.1 port=12346
		
	Can be used to feed wireshark in real time :
		./rpcap.exe | wireshark -k -i -
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// http://www.iana.org/assignments/media-types/application/vnd.tcpdump.pcap
// http://www.tcpdump.org/manpages/pcap-savefile.5.txt
#define PCAP_MAGIC			0xa1b2c3d4
#define PCAP_NSEC_MAGIC		0xa1b23c4d
#define PCAP_MAJOR 2
#define PCAP_MINOR 4

struct pcap_file_header {
	uint32_t magic;
	uint16_t version_major;
	uint16_t version_minor;
	int32_t thiszone;		/* gmt to local correction */
	uint32_t sigfigs;		/* accuracy of timestamps */
	uint32_t snaplen;		/* max length saved portion of each pkt */
	uint32_t linktype;		/* data link type (LINKTYPE_*) */
};
struct pcap_pkthdr {
	uint32_t ts_sec, ts_usec;		/* time stamp */
	uint32_t caplen;		/* length of portion present */
	uint32_t len;			/* length this packet (off wire) */
};

int pcap_write_file_header( FILE *f, int ts_nsec, int snaplen, int linktype)
{
	struct pcap_file_header file_header;

	file_header.magic = ts_nsec ? PCAP_NSEC_MAGIC : PCAP_MAGIC;
	file_header.version_major = PCAP_MAJOR;
	file_header.version_minor = PCAP_MINOR;
	file_header.thiszone = 0;
	file_header.sigfigs = 0;
	file_header.snaplen = snaplen;
	file_header.linktype = linktype;

	return fwrite( &file_header, 1, sizeof( file_header), f);
}

int pcap_write_packet( FILE *f, uint32_t sec, uint32_t usec, uint32_t caplen, uint32_t len, const unsigned char *payload)
{
	int written = 0;
	size_t n;
	struct pcap_pkthdr pkthdr;
	pkthdr.ts_sec = sec;
	pkthdr.ts_usec = usec;
	pkthdr.caplen = caplen;
	pkthdr.len = len;
	n = fwrite( (const uint8_t *) &pkthdr, 1, sizeof (pkthdr), f);
	if (n < (sizeof (pkthdr)))
		return -1;
	written += n;
	n = fwrite( payload, 1, caplen, f);
	if (n < caplen)
		return -2;
	written += n;
	
	return written;
}

size_t read_full( int fd, void *buf, size_t count)
{
	size_t received = 0;
	while (received < count)
	{
		int n = read( fd, (char *)buf + received, count - received);
		if (!n || (n == -1))
		{
			received = n;
			break;
		}
		received += n;
	}
	return received;
}

int main( int argc, char *argv[])
{
	int n;
	FILE *f = stdout;
	int snaplen = 65535;
	int ts_nsec = 0;
	int linktype = 1;
	const char *host = "127.0.0.1";
	int port = 12346;
	struct sockaddr_in sa;
	int s;
	int no_time = 0;
	int arg = 1;
	
	while (arg < argc)
	{
		if (!strcmp( argv[arg], "-n"))
		{
			no_time = 1;
			arg++;
			continue;
		}
		host = argv[arg++];
		if (arg < argc)
		{
			sscanf( argv[arg++], "%d", &port);
		}
	}
	
	fprintf( stderr, "connecting to %s:%d..\n\n", host, port);
	fprintf( stderr, "sizeof(void*)=%zd sizeof(file_header)=%zd sizeof(pkthdr)=%zd\n", sizeof( void *), sizeof( struct pcap_file_header), sizeof( struct pcap_pkthdr));
	s = socket( PF_INET, SOCK_STREAM, 0);
	memset( &sa, 0, sizeof( sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons( port);
	sa.sin_addr.s_addr = inet_addr( host);
	n = connect( s, (struct sockaddr *)&sa, sizeof( sa));
	if (n == -1)
	{
		fprintf( stderr, "couldn't connect to port %d (%s)\n", port, strerror( errno));
		exit( 1);
	}
	pcap_write_file_header( f, ts_nsec, snaplen, linktype);
	fflush( f);
	while (1)
	{
		unsigned char buf[1600];
		int caplen, len;
		uint32_t sec = 0, usec = 0;
		uint32_t nsize, size;
		struct timeval tv;

		n = read_full( s, &nsize, sizeof( nsize));
		if (n == -1)
		{
			fprintf( stderr, "error during read size (%s)\n", strerror( errno));
			exit( 1);
		}
		else if (n == 0)
		{
			fprintf( stderr, "hangup 1\n");
			break;
		}
		size = ntohl( nsize);
		if (size > sizeof( buf))
		{
			fprintf( stderr, "read packet too big (max=%zd read=%d)\n", sizeof( buf), size);
			exit( 1);
		}
		gettimeofday( &tv, 0);
		if (!no_time)
		{
			sec = tv.tv_sec;
			usec = tv.tv_usec;
		}
		fprintf( stderr, "read packet with %d bytes at sec=%d usec=%d\n", size, (int)sec, usec);	
		n = read_full( s, buf, size);
		if (n == -1)
		{
			fprintf( stderr, "error during read payload (%s)\n", strerror( errno));
			exit( 1);
		}
		else if (n == 0)
		{
			fprintf( stderr, "hangup 2\n");
			break;
		}
		len = caplen = size;
		pcap_write_packet( f, sec, usec, caplen, len, buf);
		fflush( f);
	}

	return 0;
}
