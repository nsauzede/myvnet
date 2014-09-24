TARGET=myvhub
TARGET+=myvmon

CFLAGS=-Wall -Werror

CFLAGS+=-g
CFLAGS+=-O0

all:$(TARGET)

clean:
	$(RM) $(TARGET) *.o
