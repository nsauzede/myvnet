TARGET=myvhub
TARGET+=myvmon
TARGET+=usrv
TARGET+=icmp_ping
TARGET+=myping
TARGET+=mydhcpd

CFLAGS=-Wall -Werror

CFLAGS+=-g
CFLAGS+=-O0

all:$(TARGET)

mydhcpd: mydhcpd.o myvutils.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) *.o
