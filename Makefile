CC=gcc
LD=gcc
TARGET=pastebin

CFLAGS=-std=c11 -Wall -Werror -Os -Wno-unused-result
LDFLAGS=-lpthread -luuid

.PHONY: clean

ifdef SOCKET_PATH
CFLAGS := $(CFLAGS) -DSOCKET_PATH=\""$(SOCKET_PATH)\""
endif

ifdef STORE_PATH
CFLAGS := $(CFLAGS) -DSTORE_PATH=\""$(STORE_PATH)\""
endif

ifdef BASE_URL
CFLAGS := $(CFLAGS) -DBASE_URL=\""$(BASE_URL)\""
endif

ALL: $(TARGET)

$(TARGET): main.o
	$(LD) -o $(TARGET) main.o $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(TARGET)

