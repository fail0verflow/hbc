#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#if 0
uint16_t deftbl[16] = {
	2048,	0,
	4096,	-2048,
	0,		0,
	1536,	512,
	1920,	0,
	2176,	0,
	3680,	-1664,
	3136,	-1856
};
#endif

#if 0
int16_t deftbl[16] = {
	2048,	0,
 0,0,
 0,0,
 0,0,
 0,0,
 0,0,
 0,0,
 0,0,
};
#endif

int16_t deftbl[16] = {
674,1040,
3598,-1738,
2270,-583,
3967,-1969,
1516,381,
3453, -1468,
2606, -617,
3795, -1759,
};


typedef struct {

	uint32_t fourcc;
	uint32_t unk;
	uint32_t filesize;
	uint16_t unk1,unk2;
	uint32_t infooff;
	uint32_t infosize;
	uint32_t dataoff;
	uint32_t datasize;
} bnshdr;

typedef struct {
	uint32_t fourcc;
	uint32_t size;
	uint16_t looped;
	uint16_t unk2;
	uint16_t srate;
	uint16_t unk3;
	uint32_t looppoint;
	uint32_t samples;
	uint32_t unknown1[6];
	uint32_t start1;
	uint32_t start2;
	uint32_t unknown2[2];
	int16_t tbl1[16];
	uint16_t unka1[8];
	int16_t tbl2[16];
	uint16_t unka2[8];
} bnsinfo;

float tables[2][16];

typedef struct {
	uint32_t fourcc;
	uint32_t size;
} bnsdatahdr;

#define SWAB16(x) ((((x)>>8)&0xFF) | (((x)&0xFF)<<8))
#define SWAB32(x) ((SWAB16((x)&0xFFFF)<<16)|(SWAB16(((x)>>16)&0xFFFF)))

#define ISWAB16(x) x=SWAB16(x)
#define ISWAB32(x) x=SWAB32(x)

typedef struct {
	int16_t l;
	int16_t r;
} sample;

int16_t lsamps[2][2] = {{0,0},{0,0}};
int16_t rlsamps[2][2] = {{0,0},{0,0}};

#define CLAMP(a,min,max) (((a)>(max))?(max):(((a)<(min))?(min):(a)))

void unpack_adpcm(int idx, int16_t *table, uint8_t *data, int16_t *outbuf) 
{

	int32_t index = (data[0] >> 4) & 0x7; //highest bit of byte is ignored
	uint32_t exponent = 28 - (data[0] & 0xf);
	int32_t factor1 = table[2*index];
	int32_t factor2 = table[2*index + 1];
	int i;
	int32_t sample;
	for(i=0;i<14;i++) {
		sample = data[1+(i/2)];
		if(!(i&1)) {
			sample = (sample&0xf0)<<24;
		} else {
			sample = (sample)<<28;
		}
		sample = ((lsamps[idx][1]*factor1 + lsamps[idx][0]*factor2)>>11) + (sample>>exponent);
		if(sample>32767) sample=32767;
		if(sample<-32768) sample=-32768;
		if(abs(sample)>20000) printf("dammit %d\n",sample);
		outbuf[i] = sample;
		lsamps[idx][0] = lsamps[idx][1];
		lsamps[idx][1] = outbuf[i];
	}
}

uint8_t findexp(float residual, uint8_t *nybble)
{
	uint8_t exp = 0;
	
	while((residual > 7.5f) || (residual < -8.5f)) {
		exp++;
		residual /= 2;
	}
	if(nybble)
		*nybble = CLAMP((int16_t)floor(residual),-8,7);
	return exp;
}

uint8_t determine_std_exponent(int idx, int16_t *table, int index, int16_t *inbuf)
{
	int32_t maxres = 0;
	int32_t factor1 = table[2*index];
	int32_t factor2 = table[2*index + 1];
	int32_t predictor;
	int32_t residual;
	int i;
	int16_t elsamps[2];
	memcpy(elsamps,rlsamps[idx],sizeof(int16_t)*2);
	for(i=0;i<14;i++) {
		predictor = (elsamps[1]*factor1 + elsamps[0]*factor2)/2048;
		residual = inbuf[i] - predictor;
		if(residual > maxres) maxres = residual;
		elsamps[0] = elsamps[1];
		elsamps[1] = inbuf[i];
	}
	return findexp(maxres,NULL);
}

float compress_adpcm(int idx, int16_t *table, uint8_t tblidx, uint8_t *data, int16_t *inbuf, int16_t *lsamps) {
	
	int32_t factor1 = table[2*tblidx];
	int32_t factor2 = table[2*tblidx + 1];
	int32_t predictor;
	int32_t residual;
	uint8_t exp;
	int8_t nybble;
	int i;
	float error = 0;
	
	exp = determine_std_exponent(idx, table, tblidx, inbuf);
	
	while(exp<=15) {
		memcpy(lsamps,rlsamps[idx],sizeof(int16_t)*2);
		data[0] = exp | tblidx<<4;
		error = 0;
		for(i=0;i<14;i++) {
			predictor = (lsamps[1]*factor1 + lsamps[0]*factor2)>>11;
			residual = inbuf[i] - predictor;
			residual = residual>>exp;
			if((residual > 7) || (residual < -8)) {
				exp++;
				break;
			}
			nybble = CLAMP(residual,-8,7);
			if(i&1) {
				data[i/2+1] |= nybble&0xf;
			} else {
				data[i/2+1] = nybble<<4;
			}
			predictor += nybble<<exp;
			lsamps[0] = lsamps[1];
			lsamps[1] = CLAMP(predictor,-32768,32767);
			error += powf(lsamps[1] - inbuf[i],2.0f);
		}
		if(i == 14) break;
	}
	return error;
}


void repack_adpcm(int idx, int16_t *table, uint8_t *data, int16_t *inbuf)
{
	uint8_t tblidx;
	uint8_t testdata[8];
	int16_t tlsamps[2];
	int16_t blsamps[2];
	float error;
	float besterror = 99999999.0f;
	for(tblidx = 0; tblidx < 8; tblidx++) {
		error = compress_adpcm(idx, table, tblidx, testdata, inbuf, tlsamps);
		if(error < besterror) {
			besterror = error;
			memcpy(data, testdata, 8);
			memcpy(blsamps, tlsamps, sizeof(int16_t)*2);
		}
	}
	memcpy(rlsamps[idx], blsamps, sizeof(int16_t)*2);

}

int main(int argc, char **argv)
{

	FILE *f;
	FILE *f2 = NULL;
	FILE *fo;
	int i,j;
	int16_t sampbuf[14];
	
	bnshdr hdr;
	bnsinfo info;
	bnsdatahdr datahdr;
	uint8_t *databuf;
	uint8_t *data1;
	uint8_t *data2;
	sample *datain;
	
	int samples;
	int loop_pt = 0;
	int blocks;
	int separated_loop = 0;

	if(argc > 4 && (atoi(argv[3]) == 1))
		separated_loop = 1;	
	
	f = fopen(argv[1],"r");
	fo = fopen(argv[2],"w");
	
	fseek(f,0,SEEK_END);
	
	samples = ftell(f)/(sizeof(uint16_t)*2);
	
	if(separated_loop) {
		f2 = fopen(argv[4],"r");
		fseek(f2,0,SEEK_END);
		loop_pt = samples;
		samples += ftell(f2)/(sizeof(uint16_t)*2);
	}
	
	blocks = (samples+13)/14;
	
	memset(&hdr,0,sizeof(hdr));
	memset(&info,0,sizeof(info));
	memset(&datahdr,0,sizeof(datahdr));
	
	hdr.fourcc = 0x20534e42;
	hdr.unk = SWAB32(0xfeff0100);
	hdr.filesize = SWAB32(blocks * 16 + sizeof(hdr) + sizeof(info) + sizeof(datahdr));
	hdr.unk1 = SWAB16(32);
	hdr.unk2 = SWAB16(2);
	hdr.infooff = SWAB32(sizeof(hdr));
	hdr.infosize = SWAB32(sizeof(info));
	hdr.dataoff = SWAB32(sizeof(hdr) + sizeof(info));
	hdr.datasize = SWAB32(sizeof(datahdr) + blocks * 16);
	
	info.fourcc = 0x4f464e49;
	info.size = SWAB32(sizeof(info));
	info.srate = SWAB16(32000);
	if(argc > 3 && (atoi(argv[3]) == 1))
		info.looped = SWAB16(1);
	info.unk2 = SWAB16(0x200);
	info.looppoint = SWAB32(loop_pt);
	info.samples = SWAB32(samples);
	info.unknown1[0] = SWAB32(0x18);
	info.unknown1[1] = SWAB32(0x00);
	info.unknown1[2] = SWAB32(0x20);
	info.unknown1[3] = SWAB32(0x2c);
	info.unknown1[4] = SWAB32(0x00);
	info.unknown1[5] = SWAB32(0x38);
	info.unknown2[0] = SWAB32(0x68);
	info.unknown2[1] = SWAB32(0x00);
	info.start1 = SWAB32(0);
	info.start2 = SWAB32(blocks * 8);
	for(i=0;i<16;i++) {
		info.tbl1[i] = SWAB16((int16_t)(deftbl[i]));
		info.tbl2[i] = SWAB16((int16_t)(deftbl[i]));
	}
	
	datahdr.fourcc = 0x41544144;
	datahdr.size = SWAB32(blocks * 16);
	
	fwrite(&hdr,sizeof(hdr),1,fo);
	fwrite(&info,sizeof(info),1,fo);
	fwrite(&datahdr,sizeof(datahdr),1,fo);
	
	
	datain = malloc(sizeof(uint16_t)*2*blocks*14);
	memset(datain,0,sizeof(uint16_t)*2*blocks*14);
	
	databuf = malloc(blocks * 16);
	data1 = databuf;
	data2 = databuf + blocks * 8;
	
	if(separated_loop) {
		fseek(f,0,SEEK_SET);
		fread(datain,sizeof(uint16_t)*2,loop_pt,f);
		fseek(f2,0,SEEK_SET);
		fread(&datain[loop_pt],sizeof(uint16_t)*2,samples-loop_pt,f2);
		fclose(f);
		fclose(f2);
	} else {
		fseek(f,0,SEEK_SET);
		fread(datain,sizeof(uint16_t)*2,samples,f);
		fclose(f);
	}
	
	printf("Samples: 0x%x\n",samples);
	printf("Blocks: 0x%x Size ADPCM: 0x%x Size PCM: 0x%x\n",blocks,blocks*8,blocks*14);
	if(separated_loop)
		printf("Loop point: 0x%x samples\n",loop_pt);
	
	for(i=0;i<blocks;i++) {
		//printf("Block %d\n",i);
		for(j=0;j<14;j++) {
			sampbuf[j] = datain[i*14+j].l;
		}
		repack_adpcm(0,deftbl,data1,sampbuf);
		//unpack_adpcm(0,deftbl,data1,sampbuf);
		for(j=0;j<14;j++) {
			sampbuf[j] = datain[i*14+j].r;
		}
		repack_adpcm(1,deftbl,data2,sampbuf);
		//unpack_adpcm(1,deftbl,data2,sampbuf);
		data1 += 8;
		data2 += 8;
	}
	
	fwrite(databuf, blocks*16, 1, fo);
	
	fclose(fo);
	
	free(datain); free(databuf);
	return 0;
}
