CFLAGS = -Wall -W -Os -g
TARGET = wiipax
OBJS = LzFind.o LzmaEnc.o LzmaDec.o lzma.o main.o
OBJS += stub_mini.o stub_mini_debug.o
OBJS += stub_dkf.o stub_dkf_debug.o
OBJS += stub_dkfc.o stub_dkfc_debug.o

NOMAPFILE = 1

include ../../common.mk

install: all
	install -m 755 $(TARGET) $(WIIDEV)/bin

