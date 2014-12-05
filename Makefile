ifneq ($(strip $(shell $(CC) -v 2>&1 | grep "mingw")),)
WIN32=1
endif

TARGET=myvhub.exe
TARGET+=myvmon.exe
TARGET+=usrv.exe
TARGET+=icmp_ping.exe
TARGET+=myping.exe
TARGET+=mydhcpd.exe
TARGET+=rpcap.exe
ifndef WIN32
TARGET+=mybridge.exe
TARGET+=mypxesrv.exe
endif

CFLAGS=-Wall -Werror

CFLAGS+=-g
CFLAGS+=-O0

ifdef WIN32
LIBYACAPI=	~/tmp/build-yacapi-compat/the_install
YACAPICONFIG=$(LIBYACAPI)/bin/yacapi-config
#CFLAGS+=	-I$(LIBYACAPI)/include/compat
CFLAGS+=	`$(YACAPICONFIG) --compat --cflags`
#LDFLAGS+=	-L$(LIBYACAPI)/lib -lyacapi -lws2_32
#LDFLAGS+=	$(LIBYACAPI)/lib/libyacapi.a -lws2_32
LDFLAGS+=	`$(YACAPICONFIG) --static-libs`
endif

all:$(TARGET)

#%.exe:	%.c
#	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)
%.exe:	%.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

mydhcpd.exe: mydhcpd.o myvutils.o
ifdef MIPS
mypxesrv.exe:CC=mips-linux-gnu-gcc
mypxesrv.exe:CFLAGS=-msoft-float
mypxesrv.exe:LDFLAGS=-msoft-float
mypxesrv.exe: LDFLAGS+=-static
endif
mypxesrv.exe: mypxesrv.o myvutils.o
	$(CC) -o $@ $^ $(LDFLAGS)
	mips-linux-gnu-strip.exe --strip-all mypxesrv.exe

clean:
	$(RM) $(TARGET) *.o
clobber: clean
	$(RM) *~
