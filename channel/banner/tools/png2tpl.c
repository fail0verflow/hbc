#include <stdio.h>
#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct tpl_hdr {
	uint32_t magic;
	uint32_t ntextures;
	uint32_t hdrsize;
};

struct tpl_tex {
	uint32_t hdroff;
	uint32_t pltoff;
};

struct tpl_texhdr {
	uint16_t height;
	uint16_t width;
	uint32_t format;
	uint32_t offset;
	uint32_t wraps;
	uint32_t wrapt;
	uint32_t minfilter;
	uint32_t maxfilter;
	float lodbias;
	uint8_t edgelod;
	uint8_t minlod;
	uint8_t maxlod;
	uint8_t unpacked;
};

#define SWAB16(x) ((((x)>>8)&0xFF) | (((x)&0xFF)<<8))
#define SWAB32(x) ((SWAB16(((uint32_t)(x))&0xFFFF)<<16)|(SWAB16((((uint32_t)(x))>>16)&0xFFFF)))

#define ISWAB16(x) x=SWAB16(x)
#define ISWAB32(x) x=SWAB32(x)

#define ALIGN(x) (((x)+31)&(~31))

void do_ia8(int width, int height, FILE *fp, uint8_t **row_pointers) {

	uint16_t **in = (uint16_t **)row_pointers;
	int wo, ho, wt, i, j;
	uint16_t *out;
	wo = (width + 3) & ~3;
	ho = (height + 3) & ~3;
	wt = wo/4;
	out = malloc(wo*ho*2);
	memset(out,0,wo*ho*2);
	for(i=0; i<height; i++) {
		for(j=0; j<width; j++) {
			int tx,ty,opos;
			tx = j / 4;
			ty = i / 4;
			opos = (tx*16) + (ty*16*wt) + j%4 + (i%4)*4;
			out[opos] = in[i][j];
		}
	}
	
	fwrite(out,2,wo*ho,fp);
}

void do_rgb565(int width, int height, FILE *fp, uint8_t **row_pointers) {

	uint8_t **in = (uint8_t **)row_pointers;
	int wo, ho, wt, i, j;
	uint16_t *out;
	wo = (width + 3) & ~3;
	ho = (height + 3) & ~3;
	wt = wo/4;
	out = malloc(wo*ho*2);
	memset(out,0,wo*ho*2);
	for(i=0; i<height; i++) {
		for(j=0; j<width; j++) {
			int tx,ty,opos;
			tx = j / 4;
			ty = i / 4;
			opos = (tx*16) + (ty*16*wt) + j%4 + (i%4)*4;
			out[opos] = SWAB16((in[i][j*3]<<8) & 0xF800);
			out[opos] |= SWAB16((in[i][j*3+1]<<3) & 0x07E0);
			out[opos] |= SWAB16((in[i][j*3+2]>>3) & 0x001F);
		}
	}
	
	fwrite(out,2,wo*ho,fp);
}

void do_i8(int width, int height, FILE *fp, uint8_t **in) {

	int wo, ho, wt, i, j;
	uint8_t *out;
	wo = (width + 7) & ~7;
	ho = (height + 3) & ~3;
	wt = wo/8;
	out = malloc(wo*ho);
	memset(out,0,wo*ho);
	for(i=0; i<height; i++) {
		for(j=0; j<width; j++) {
			int tx,ty,opos;
			tx = j / 8;
			ty = i / 4;
			opos = (tx*32) + (ty*32*wt) + j%8 + (i%4)*8;
			out[opos] = in[i][j];
		}
	}
	
	fwrite(out,1,wo*ho,fp);
}


void do_rgba8(int width, int height, FILE *fp, uint8_t **row_pointers) {

	uint32_t **in = (uint32_t **)row_pointers;
	int wo, ho, wt, i, j;
	uint16_t *out;
	wo = (width + 3) & ~3;
	ho = (height + 3) & ~3;
	wt = wo/4;
	out = malloc(wo*ho*4);
	memset(out,0,wo*ho*4);
	for(i=0; i<height; i++) {
		for(j=0; j<width; j++) {
			int tx,ty,opos;
			tx = j / 4;
			ty = i / 4;
			opos = (tx*32) + (ty*32*wt) + j%4 + (i%4)*4;
			out[opos]    = (in[i][j]&0xFFFF);
			out[opos+16] = (in[i][j]>>16);
		}
	}
	
	fwrite(out,2,wo*ho*2,fp);
}


int main(int argc, char **argv)
{
	png_structp png_ptr = png_create_read_struct
			(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	
	png_infop info_ptr = png_create_info_struct(png_ptr);
	
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		printf("PNG error\n");
		return 1;
	}
	
	FILE *fp = fopen(argv[1],"rb");
	
	png_init_io(png_ptr, fp);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING|PNG_TRANSFORM_EXPAND|PNG_TRANSFORM_STRIP_16|PNG_TRANSFORM_SWAP_ALPHA, NULL);
	
	uint8_t **row_pointers = png_get_rows(png_ptr, info_ptr);
	
	png_uint_32 width, height;
	int bit_depth, color_type, filter_method, compression_type, interlace_type;
	png_get_IHDR(png_ptr, info_ptr, &width, &height,
				 &bit_depth, &color_type, &interlace_type,
				 &compression_type, &filter_method);

	printf("Texture: %d x %d\n",(int)width,(int)height);
	
	FILE *fo = fopen(argv[2],"wb");
	
	struct tpl_hdr tplhdr;
	tplhdr.magic = SWAB32(0x0020af30);
	tplhdr.ntextures = SWAB32(1);
	tplhdr.hdrsize = SWAB32(sizeof(struct tpl_hdr));
	
	fwrite(&tplhdr,sizeof(struct tpl_hdr),1,fo);
	
	struct tpl_tex tpltex;
	tpltex.hdroff = SWAB32(ftell(fo) + sizeof(struct tpl_tex));
	tpltex.pltoff = SWAB32(0);
	
	fwrite(&tpltex,sizeof(struct tpl_tex),1,fo);
	
	struct tpl_texhdr texhdr;
	memset(&texhdr,0,sizeof(struct tpl_texhdr));
	
	
	
	texhdr.width = SWAB16(width);
	texhdr.height = SWAB16(height);
	texhdr.offset = SWAB32(ALIGN(ftell(fo) + sizeof(struct tpl_texhdr)));
	texhdr.minfilter = SWAB32(1);
	texhdr.maxfilter = SWAB32(1);
	texhdr.wraps = SWAB32(atoi(argv[3]));
	texhdr.wrapt = SWAB32(atoi(argv[4]));
	
	switch(color_type) {
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			texhdr.format = SWAB32(3); //IA8
			fwrite(&texhdr,sizeof(struct tpl_texhdr),1,fo);
			fseek(fo,SWAB32(texhdr.offset),SEEK_SET);
			do_ia8(width, height, fo,row_pointers);
			break;
		case PNG_COLOR_TYPE_GRAY:
			texhdr.format = SWAB32(1); //I8
			fwrite(&texhdr,sizeof(struct tpl_texhdr),1,fo);
			fseek(fo,SWAB32(texhdr.offset),SEEK_SET);
			do_i8(width, height, fo,row_pointers);
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			texhdr.format = SWAB32(6); //ARGB
			fwrite(&texhdr,sizeof(struct tpl_texhdr),1,fo);
			fseek(fo,SWAB32(texhdr.offset),SEEK_SET);
			do_rgba8(width, height, fo,row_pointers);
			break;
		case PNG_COLOR_TYPE_RGB:
			texhdr.format = SWAB32(4); //RGB565
			fwrite(&texhdr,sizeof(struct tpl_texhdr),1,fo);
			fseek(fo,SWAB32(texhdr.offset),SEEK_SET);
			do_rgb565(width, height, fo,row_pointers);
			break;
	}
	return 0;
}
