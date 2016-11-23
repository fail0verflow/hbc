all: banner.bin

banner.bin: twintig title banner.ppm icon.ppm
	@$(CURDIR)/twintig

twintig: twintig.c
	@gcc -g -O2 -Wall $< -o $@

clean:
	rm -f twintig banner.bin

