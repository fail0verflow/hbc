include ../../broadway.mk

CFLAGS += -mno-eabi -mno-sdata -O2 -ffreestanding
CFLAGS += -Wall -Wextra -flto
DEFINES =
LDFLAGS += -nostartfiles -nodefaultlibs

OBJS_COMMON = crt0.o main.o string.o sync.o elf.o time.o LzmaDec.o
TARGET_ID =

ifeq ($(DKPPC),1)
DEFINES += -DDKPPC
OBJS = $(OBJS_COMMON)
LDSCRIPT = devkitppc.ld
TARGET_ID = _dkp
else
ifeq ($(DKPPCCHANNEL),1)
DEFINES += -DDKPPC
OBJS = realmode.o $(OBJS_COMMON)
LDSCRIPT = channel.ld
TARGET_ID = _dkpc
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
	$(Q)$(MAKE) DKPPC=1
	$(Q)$(MAKE) DKPPC=1 clean
	$(Q)$(MAKE) DKPPC=1 NDEBUG=1
	$(Q)$(MAKE) DKPPC=1 NDEBUG=1 clean
	$(Q)$(MAKE) DKPPCCHANNEL=1
	$(Q)$(MAKE) DKPPCCHANNEL=1 clean
	$(Q)$(MAKE) DKPPCCHANNEL=1 NDEBUG=1
	$(Q)$(MAKE) DKPPCCHANNEL=1 NDEBUG=1 clean

.PHONY: release
