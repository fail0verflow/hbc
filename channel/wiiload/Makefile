CFLAGS += -Wall -Wextra -Os -g -pipe -flto

ifeq ($(WIN32), 1)
PREFIX ?= i586-mingw32msvc-
BIN_EXT = .exe
CFLAGS += -Iwin32/include
LDFLAGS += -Lwin32/lib -lws2_32 -lz
else
PREFIX ?=
BIN_EXT =
LDFLAGS += -lz
endif

ifeq ($(FTDI), 1)
CFLAGS += -DUSE_LIBFTDI
LDFLAGS += -lftdi -lusb
endif

CC = $(PREFIX)gcc
STRIP = $(PREFIX)strip

BIN = wiiload$(BIN_EXT)

OBJS = gecko.o main.o

all: $(BIN)

clean:
	@rm -f *.o core core.* $(BIN)

strip: all
	$(STRIP) $(BIN)

install: strip
	install -m 755 $(BIN) $(DEVKITPPC)/bin

$(BIN): $(OBJS)
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

%o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

osx:
	$(MAKE) clean
	PREFIX=/opt/toolchains/powerpc-apple-darwin8-10.2/bin/ppc-apple-darwin8- CFLAGS=-fabi-version=1 $(MAKE) strip
	cp $(BIN) $(BIN)_ppc
	$(MAKE) clean
	PREFIX=/opt/toolchains/i686-apple-darwin9-10.4/bin/i686-apple-darwin9- $(MAKE) strip
	cp $(BIN) $(BIN)_intel
	$(MAKE) clean
	/opt/toolchains/i686-apple-darwin9-10.4/bin/i686-apple-darwin9-lipo -create $(BIN)_ppc $(BIN)_intel -output $(BIN)
	rm $(BIN)_ppc $(BIN)_intel
