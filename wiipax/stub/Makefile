include ../../broadway.mk

CFLAGS += -mno-eabi -mno-sdata -O2 -ffreestanding
CFLAGS += -Wall -Wextra
DEFINES =
LDFLAGS += -nostartfiles -nodefaultlibs

OBJS_COMMON = crt0.o main.o string.o sync.o elf.o time.o LzmaDec.o
TARGET_ID =

ifeq ($(DEVKITFAIL),1)
DEFINES += -DDEVKITFAIL
OBJS = $(OBJS_COMMON)
LDSCRIPT = devkitfail.ld
TARGET_ID = _dkf
else
ifeq ($(DKFAILCHANNEL),1)
DEFINES += -DDEVKITFAIL
OBJS = realmode.o $(OBJS_COMMON)
LDSCRIPT = channel.ld
TARGET_ID = _dkfc
else
OBJS = realmode.o plunge.o $(OBJS_COMMON)
LDSCRIPT = realmode.ld
TARGET_ID = _mini
endif
endif

ifeq ($(NDEBUG),1)
DEFINES += -DNDEBUG
TARGET_DEBUG =
else
OBJS += exception.o exception_asm.o vsprintf.o gecko.o
TARGET_DEBUG = _debug
endif

TARGET = stub$(TARGET_ID)$(TARGET_DEBUG).elf

include ../../common.mk

all: xxd

xxd: $(TARGET)
	@echo "  XXD       $^"
	@xxd -i $(TARGET) > $(subst .elf,.c,$(TARGET))

distclean: clean
	rm -f stub_*.elf stub_*.c

release:
	$(Q)$(MAKE) clean
	$(Q)$(MAKE)
	$(Q)$(MAKE) clean
	$(Q)$(MAKE) NDEBUG=1
	$(Q)$(MAKE) NDEBUG=1 clean
	$(Q)$(MAKE) DEVKITFAIL=1
	$(Q)$(MAKE) DEVKITFAIL=1 clean
	$(Q)$(MAKE) DEVKITFAIL=1 NDEBUG=1
	$(Q)$(MAKE) DEVKITFAIL=1 NDEBUG=1 clean
	$(Q)$(MAKE) DKFAILCHANNEL=1
	$(Q)$(MAKE) DKFAILCHANNEL=1 clean
	$(Q)$(MAKE) DKFAILCHANNEL=1 NDEBUG=1
	$(Q)$(MAKE) DKFAILCHANNEL=1 NDEBUG=1 clean

.PHONY: release

