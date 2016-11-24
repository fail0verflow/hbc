INSTALLER_VER_MAJOR = 1
INSTALLER_VER_MINOR = 2

TOOLS = $(CURDIR)/../../pywii/pywii-tools
CERTS = $(CURDIR)/dpki/certs
DPKI_ISSUER_TIK = Root-CA00000002-XS00000006
DPKI_ISSUER_TMD = Root-CA00000002-CP00000007

INSTALLERFILES = certs.dpki certs.retail cetk.dpki cetk.retail tmd.dpki tmd.retail 00000000 00000001

all: channel_retail.wad

dpki: channel_dpki.wad

channel_retail.wad: retail/cetk retail/tmd 00000000 00000001
	@$(TOOLS)/wadpack.py $@ retail/

channel_dpki.wad: dpki/cetk dpki/tmd 00000000 00000001
	@$(TOOLS)/wadpack.py -dpki $@ dpki/

00000000: ../banner/channel.imet
	@cat footer $< > $@

00000001: ../channelapp/channelapp-channel.dol
	@cp $< $@

retail/cetk: cetk.template
	@cp $< $@
	@$(TOOLS)/tikfix.py $@

dpki/cetk: retail/cetk
	@$(TOOLS)/dpkisign.py -cetk $< $@ $(CERTS) $(DPKI_ISSUER_TIK)

retail/tmd: tmd.template 00000000 00000001
	@cp $< $@
	@$(TOOLS)/tmdupdatecr.py $@ $(CURDIR)
	@$(TOOLS)/tmdvers.py $@ $(INSTALLER_VER_MAJOR) $(INSTALLER_VER_MINOR)

dpki/tmd: retail/tmd
	@$(TOOLS)/dpkisign.py -tmd $< $@ $(CERTS) $(DPKI_ISSUER_TMD)

check: all
	@echo ===== RETAIL =====
	@$(TOOLS)/tikinfo.py cetk.retail certs.retail
	@$(TOOLS)/tmdinfo.py tmd.retail certs.retail
	@echo ===== DPKI =====
	@$(TOOLS)/tikinfo.py -dpki cetk.dpki certs.dpki
	@$(TOOLS)/tmdinfo.py -dpki tmd.dpki certs.dpki

clean:
	rm -f retail/cetk dpki/cetk retail/tmd retail/tmd 00000000 00000001 channel_retail.wad channel_dpki.wad

