
CFLAGS = -O3 -Wall -I/usr/local/include -L/usr/local/lib

ifeq ($(shell uname), Darwin)
	CFLAGS += -I/opt/homebrew/include -L/opt/homebrew/lib
endif

CC = $(PREFIX)gcc
STRIP = $(PREFIX)strip

all: mkbns$(EXE) png2tpl$(EXE) lz77$(EXE)

mkbns$(EXE): mkbns.c
	$(CC) $(CFLAGS) -o mkbns mkbns.c -lm

png2tpl$(EXE): png2tpl.c
	$(CC) $(CFLAGS) -o png2tpl png2tpl.c -lpng

lz77$(EXE): lz77.c
	$(CC) $(CFLAGS) -o lz77 lz77.c

clean:
	rm -f mkbns$(EXE) png2tpl$(EXE) lz77$(EXE)
