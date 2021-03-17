ifeq ($(WIN32), 1)
export PREFIX = i586-mingw32msvc-
export EXE := .exe
export LFLAGS = -lws2_32
else
export PREFIX =
export EXE =
export LFLAGS =
endif

export PYWII := $(CURDIR)/../../pywii/pywii-tools
export ALAMEDA := $(CURDIR)/../../pywii/Alameda

export PNG2TPL := $(CURDIR)/tools/png2tpl$(EXE)
export MKBNS := $(CURDIR)/tools/mkbns$(EXE)
export LZ77 := $(CURDIR)/tools/lz77$(EXE)
export ADDIMD5 := python3 $(CURDIR)/tools/addimd5.py
export ARCPACK := python3 $(PYWII)/arcpack.py
export SOX := sox

all: channel.imet

channel.imet: build/data.arc names.txt tools/join-imet.py
	python3 tools/join-imet.py $@ build/data.arc build/icon.arc build/banner.arc build/sound.bns names.txt

build/data.arc : build/data/meta/icon.bin build/data/meta/banner.bin build/data/meta/sound.bin
	$(ARCPACK) $@ build/data

build/data/meta/icon.bin : icon/* $(PNG2TPL) $(LZ77)
	$(MAKE) -C icon -f ../Makefile.section

build/data/meta/banner.bin : banner/* $(PNG2TPL) $(LZ77)
	$(MAKE) -C banner -f ../Makefile.section

build/data/meta/sound.bin : build/sound.bns
	@[ ! -d build/data/meta ] && mkdir -p build/data/meta || true
	$(ADDIMD5) $< $@

build/sound.bns : build/wiibrew-banner-intro-part.raw build/wiibrew-banner-loop-part.raw $(MKBNS)
	@[ ! -d build ] && mkdir -p build || true
	$(MKBNS) build/wiibrew-banner-intro-part.raw $@ 1 build/wiibrew-banner-loop-part.raw

build/%.raw : sound/%.wav
	@[ ! -d build ] && mkdir -p build || true
	$(SOX) $< -r 32000 -c 2 -e signed-integer -b 16 -t raw $@

testi : channel.imet
	python3 $(ALAMEDA)/Alameda.py channel.imet icon
testb : channel.imet
	python3 $(ALAMEDA)/Alameda.py channel.imet banner

$(PNG2TPL): tools/*.c
	$(MAKE) -C tools png2tpl$(EXE)

$(MKBNS): tools/*.c
	$(MAKE) -C tools mkbns$(EXE)

$(LZ77): tools/*.c
	$(MAKE) -C tools lz77$(EXE)

clean:
	rm -f channel.imet
	rm -rf build
	$(MAKE) -C tools clean
