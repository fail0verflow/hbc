.PHONY: clean installer

all:
	@$(MAKE) -C channelapp channel
	@$(MAKE) -C banner
	@$(MAKE) -C title

clean:
	@$(MAKE) -C channelapp clean
	@$(MAKE) -C banner clean
	@$(MAKE) -C title clean
	@$(MAKE) -C wiiload clean
	@$(MAKE) -C wiiload clean WIN32=1

