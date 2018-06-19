#pngcrush -rem iTXt -rem tEXt -d out *.png

PREFIX = $(DEVKITPPC)/bin/powerpc-eabi-

CC = $(PREFIX)gcc
AS = $(PREFIX)as
OBJCOPY = $(PREFIX)objcopy
ELF2DOL =$(DEVKITPRO)/tools/bin/elf2dol
GDB = $(PREFIX)gdb
BIN2S = $(DEVKITPRO)/tools/bin/bin2s
TARGET_STUB = stub
TARGET_APP = channelapp
TARGET_CHAN = channelapp-channel

WIIPAX = ../../wiipax/client/wiipax

BASE_ADDR = 0x81330000

DIR_STUB = stub
DIR_SRC = source
DIR_DATA = data
DIR_DATA_CRYPT = $(DIR_DATA)/crypt
DIR_I18N = i18n
DIR_BUILD = build
DIR_BUILD_CHAN = $(DIR_BUILD)/channel

DIR_INCLUDES = $(DIR_BUILD) \
		$(DEVKITPRO)/libogc/include \
		$(DEVKITPRO)/portlibs/ppc/include \
		$(DEVKITPRO)/portlibs/ppc/include/freetype2

DIR_LIBS = \
		$(DEVKITPRO)/libogc/lib/wii \
		$(DEVKITPRO)/portlibs/ppc/lib

LIBS = bte mxml freetype png bz2 z db fat wiiuse ogc m

MACHDEP = -g -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float
CFLAGS = $(MACHDEP) -Os -Wall -DBASE_ADDR=$(BASE_ADDR) $(DIR_INCLUDES:%=-I%)
# using -Os for the stub makes it dependent on libgcc
CFLAGS_STUB = $(MACHDEP) -O2 -Wall -DBASE_ADDR=$(BASE_ADDR) $(DIR_INCLUDES:%=-I%)
ASFLAGS = -D_LANGUAGE_ASSEMBLY -DHW_RVL
LDFLAGS_STUB = $(MACHDEP) -Wl,--section-start,.init=0x80001800 -nostartfiles -nodefaultlibs
LDFLAGS_APP = $(MACHDEP) -specs=newrvl.spec -Wl,--section-start,.init=$(BASE_ADDR) \
	      $(DIR_LIBS:%=-L%) $(LIBS:%=-l%) -Wl,-Map,channelapp.map
LDFLAGS_CHAN = $(MACHDEP) -specs=newrvl.spec -Wl,--section-start,.init=$(BASE_ADDR) -Wl,-e,_stub_start $(DIR_LIBS:%=-L%) $(LIBS:%=-l%) -Wl,-Map,channelapp_chan.map

FILES_STUB = $(wildcard $(DIR_STUB)/*.S) $(wildcard $(DIR_STUB)/*.c)
FILES_STUB_OBJx = $(FILES_STUB:$(DIR_STUB)/%.S=$(DIR_BUILD)/%.o)
FILES_STUB_OBJ = $(FILES_STUB_OBJx:$(DIR_STUB)/%.c=$(DIR_BUILD)/%.o)

BANNER_BIN = banner/banner.bin
BANNER_OBJ = $(DIR_BUILD)/banner_bin.o

FILES_TTF = $(wildcard $(DIR_DATA)/*.ttf)
FILES_TTF_OBJ = $(FILES_TTF:$(DIR_DATA)/%.ttf=$(DIR_BUILD)/%_ttf.o)

FILES_PNG = $(wildcard $(DIR_DATA)/*.png)
FILES_PNG_OBJ = $(FILES_PNG:$(DIR_DATA)/%.png=$(DIR_BUILD)/%_png.o)

POTPL = $(DIR_I18N)/template.pot
POTPL_MRG = $(DIR_I18N)/template.merge.pot
FILES_PO = $(wildcard $(DIR_I18N)/*.po)
FILES_PO_ENC = $(FILES_PO:$(DIR_I18N)/%.po=$(DIR_BUILD)/%.enc.po)
FILES_MO = $(FILES_PO_ENC:$(DIR_BUILD)/%.enc.po=$(DIR_BUILD)/%.mo)
FILES_MO_OBJ = $(FILES_MO:$(DIR_BUILD)/%.mo=$(DIR_BUILD)/%_mo.o)
ENCODING = utf-8
FILES_PO_MRG = $(FILES_PO:$(DIR_I18N)/%.po=$(DIR_I18N)/merge/%.po)

FILES_SRC = $(wildcard $(DIR_SRC)/*.c)
FILES_SRC_OBJ = $(FILES_SRC:$(DIR_SRC)/%.c=$(DIR_BUILD)/%.o) \
		$(DIR_BUILD)/$(TARGET_STUB)_bin.o $(BANNER_OBJ) \
		$(FILES_PNG_OBJ) $(FILES_MO_OBJ) $(FILES_TTF_OBJ)

FILES_CHAN = $(DIR_CHAN)/nandloader.S
FILES_CHAN_OBJ = $(DIR_BUILD)/nandloader.o $(FILES_SRC_OBJ)

.PHONY: all channel world clean upload gdb debug

all:
	@[ -d $(DIR_BUILD) ] || mkdir $(DIR_BUILD)
	@$(MAKE) --no-print-directory data
	@$(MAKE) --no-print-directory $(TARGET_APP).elf

channel:
	@$(MAKE) --no-print-directory all
	@$(MAKE) --no-print-directory $(TARGET_CHAN).dol

world:
	@$(MAKE) --no-print-directory all
	@$(MAKE) --no-print-directory $(TARGET_CHAN).dol

data: $(BANNER_OBJ) $(FILES_PNG_OBJ) $(FILES_MO_OBJ) \
		$(FILES_PNG_INT_OBJ) $(FILES_INT_OBJ) \
		$(FILES_TTF_OBJ)

%.bin: %.elf
	@echo $(@F)
	@$(OBJCOPY) -O binary $< $@

%.dol: %.elf $(ELF2DOL)
	@echo $(@F)
	@$(ELF2DOL) $< $@

$(TARGET_STUB).elf: $(FILES_STUB_OBJ)
	@echo $(@F)
	@$(CC) $(FILES_STUB_OBJ) $(LDFLAGS_STUB) -o $@

$(TARGET_APP)_nopax.elf: $(FILES_SRC_OBJ) newrvl.ld
	@echo $(@F)
	@$(CC) $(FILES_SRC_OBJ) $(LDFLAGS_APP) -o $@

$(TARGET_APP).elf: $(TARGET_APP)_nopax.elf
	@$(WIIPAX) -s devkitfail $< $@

$(TARGET_CHAN).elf: $(TARGET_APP)_nopax.elf
	@echo $(@F)
	@$(WIIPAX) -s dkfailchannel $< $@

$(DIR_BUILD)/$(TARGET_STUB)_bin.o: $(TARGET_STUB).bin
	@echo $(@F)
	@$(BIN2S) -a 32 $< | $(AS) -o $@

$(BANNER_BIN):
	@$(MAKE) -C banner

$(BANNER_OBJ): $(BANNER_BIN)
	@echo $(@F)
	@$(BIN2S) -a 32 $< | $(AS) -o $@
	@echo "extern const u8 $(<F:%.bin=%_bin)[];" > $(@:%.o=%.h)
	@echo "extern const u8 $(<F:%.bin=%_bin)_end;" >> $(@:%.o=%.h)
	@echo "extern const u32 $(<F:%.bin=%_bin)_size;" >> $(@:%.o=%.h)

$(DIR_BUILD)/%_png.o: $(DIR_DATA)/%.png
	@echo $(@F)
	@$(BIN2S) -a 32 $< | $(AS) -o $@
	@echo "extern const u8 $(<F:%.png=%_png)[];" > $(@:%.o=%.h)
	@echo "extern const u8 $(<F:%.png=%_png)_end;" >> $(@:%.o=%.h)
	@echo "extern const u32 $(<F:%.png=%_png)_size;" >> $(@:%.o=%.h)

$(DIR_BUILD)/%_ttf.o: $(DIR_DATA)/%.ttf
	@echo $(@F)
	@$(BIN2S) -a 32 $< | $(AS) -o $@
	@echo "extern const u8 $(<F:%.ttf=%_ttf)[];" > $(@:%.o=%.h)
	@echo "extern const u8 $(<F:%.ttf=%_ttf)_end;" >> $(@:%.o=%.h)
	@echo "extern const u32 $(<F:%.ttf=%_ttf)_size;" >> $(@:%.o=%.h)

$(DIR_BUILD)/%_mo.o: $(DIR_BUILD)/%.mo
	@echo $(@F)
	@$(BIN2S) -a 32 $< | $(AS) -o $@
	@echo "extern const u8 $(<F:%.mo=%_mo)[];" > $(@:%.o=%.h)
	@echo "extern const u8 $(<F:%.mo=%_mo)_end;" >> $(@:%.o=%.h)
	@echo "extern const u32 $(<F:%.mo=%_mo)_size;" >> $(@:%.o=%.h)

$(DIR_BUILD)/%.o: $(DIR_INT)/%
	@echo $(@F)
	@$(BIN2S) -a 32 $< | $(AS) -o $@
	@echo "extern const u8 $(subst .,_,$(<F))[];" > $(@:%.o=%.h)
	@echo "extern const u8 $(subst .,_,$(<F))_end;" >> $(@:%.o=%.h)
	@echo "extern const u32 $(subst .,_,$(<F))_size;" >> $(@:%.o=%.h)

$(DIR_BUILD)/%.mo: $(DIR_BUILD)/%.enc.po
	@echo $(@F)
	@msgfmt --no-hash -o $@ $<

$(DIR_BUILD)/%.enc.po: $(DIR_I18N)/%.po
	@echo $(@F)
	@msgconv -t $(ENCODING) -o $@ $<

$(DIR_BUILD)/%.o: $(DIR_STUB)/%.c
	@echo $(@F)
	@$(CC) $(CFLAGS_STUB) -MMD -MP -MF $(@:%.o=%.d) -c $< -o $@

$(DIR_BUILD)/%.o: $(DIR_STUB)/%.S
	@echo $(@F)
	@$(CC) $(ASFLAGS) -c $< -o $@

$(DIR_BUILD)/%.o: $(DIR_SRC)/%.c
	@echo $(@F)
	@$(CC) $(CFLAGS) -MMD -MP -MF $(@:%.o=%.d) -c $< -o $@

-include $(FILES_SRC_OBJ:%.o=%.d)

clean:
	rm -rf $(DIR_BUILD) \
		*.elf *.dol *.bin *.map i18n/merge/* $(POTPL).gen
	@$(MAKE) -C banner clean

wiiload:
	@$(MAKE) --no-print-directory all
	@$(DEVKITPPC)/bin/wiiload $(TARGET_APP).elf

upload: wiiload

$(POTPL).gen: source/*.c
	@echo "GETTEXT $(POTPL).gen"
	@xgettext --from-code=UTF-8 -C -o $(POTPL).gen --keyword=_ source/*.c
	@sed -i.bak s/CHARSET/UTF-8/g $(POTPL).gen

$(POTPL): $(POTPL).gen
	@echo "MERGE $(POTPL_MRG)"
	@msgmerge $(POTPL) $(POTPL).gen > $(POTPL_MRG)
	@mv -v $(POTPL_MRG) $(POTPL)
	@rm $(POTPL).gen.bak

genmsgs: $(POTPL)
msgmerge: $(POTPL) $(FILES_PO_MRG)
movemerge:
	@mv -v i18n/merge/* i18n

msgupdate: msgmerge movemerge msgstats

%.postats: %.po
	@echo -n "$<: "
	@msgfmt --statistics -o /dev/null $<

msgstats: $(FILES_PO:%.po=%.postats)
msgmergestats: msgmerge $(FILES_PO_MRG:%.po=%.postats)

$(DIR_I18N)/merge/%.po : $(DIR_I18N)/%.po $(POTPL)
	@[ ! -d $(DIR_I18N)/merge ] && mkdir -p $(DIR_I18N)/merge || true
	@echo "MERGE $(@F)"
	@msgmerge -o $@ $(@:$(DIR_I18N)/merge/%.po=$(DIR_I18N)/%.po) $(POTPL)

gdb:
	$(GDB) -n $(TARGET_APP)_nopax.elf

debug:
	$(GDB) -n $(TARGET_APP)_nopax.elf -x gdb.txt

