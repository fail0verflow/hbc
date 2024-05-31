CFLAGS = -Wall -W -Os -g -flto
TARGET = wiipax
OBJS = LzFind.o LzmaEnc.o LzmaDec.o lzma.o main.o
OBJS += stub_mini.o stub_mini_debug.o
OBJS += stub_dkp.o stub_dkp_debug.o
OBJS += stub_dkpc.o stub_dkpc_debug.o

NOMAPFILE = 1

include ../../common.mk

install: all
	install -m 755 $(TARGET) $(WIIDEV)/bin
