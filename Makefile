TARGET=myvhub
TARGET+=myvmon
TARGET+=usrv
TARGET+=icmp_ping
TARGET+=myping

CFLAGS=-Wall -Werror

CFLAGS+=-g
CFLAGS+=-O0

all:$(TARGET)

clean:
	$(RM) $(TARGET) *.o
