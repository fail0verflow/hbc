#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/*Altered by Kuwanger*/
/*************************************************************************
* Name:        lz.c
* Author:      Marcus Geelnard
* Description: LZ77 coder/decoder implementation.
* Reentrant:   Yes
* $Id: lz.c,v 1.4 2004/10/08 19:28:04 marcus256 Exp $
*
* The LZ77 compression scheme is a substitutional compression scheme
* proposed by Abraham Lempel and Jakob Ziv in 1977. It is very simple in
* its design, and uses no fancy bit level compression.
*
* This is my first attempt at an implementation of a LZ77 code/decoder.
*
* The principle of the LZ77 compression algorithm is to store repeated
* occurrences of strings as references to previous occurrences of the same
* string. The point is that the reference consumes less space than the
* string itself, provided that the string is long enough (in this
* implementation, the string has to be at least 4 bytes long, since the
* minimum coded reference is 3 bytes long). Also note that the term
* "string" refers to any kind of byte sequence (it does not have to be
* an ASCII string, for instance).
*
* The coder uses a brute force approach to finding string matches in the
* history buffer (or "sliding window", if you wish), which is very, very
* slow. I recon the complexity is somewhere between O(n^2) and O(n^3),
* depending on the input data.
*
* There is also a faster implementation that uses a large working buffer
* in which a "jump table" is stored, which is used to quickly find
* possible string matches (see the source code for LZ_CompressFast() for
* more information). The faster method is an order of magnitude faster,
* and also does a full string search in the entire input buffer (it does
* not use a sliding window).
*
* The upside is that decompression is very fast, and the compression ratio
* is often very good.
*
* The reference to a string is coded as a (length,offset) pair, where the
* length indicates the length of the string, and the offset gives the
* offset from the current data position. To distinguish between string
* references and literal strings (uncompressed bytes), a string reference
* is preceded by a marker byte, which is chosen as the least common byte
* symbol in the input data stream (this marker byte is stored in the
* output stream as the first byte).
*
* Occurrences of the marker byte in the stream are encoded as the marker
* byte followed by a zero byte, which means that occurrences of the marker
* byte have to be coded with two bytes.
*
* The lengths and offsets are coded in a variable length fashion, allowing
* values of any magnitude (up to 4294967295 in this implementation).
*
* With this compression scheme, the worst case compression result is
* (257/256)*insize + 1.
*
*-------------------------------------------------------------------------
* Copyright (c) 2003-2004 Marcus Geelnard
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would
*    be appreciated but is not required.
*
* 2. Altered source versions must be plainly marked as such, and must not
*    be misrepresented as being the original software.
*
* 3. This notice may not be removed or altered from any source
*    distribution.
*
* Marcus Geelnard
* marcus.geelnard at home.se
*************************************************************************/


/*************************************************************************
* Constants used for LZ77 coding
*************************************************************************/

/* Maximum offset (can be any size < 2^32). Lower values gives faster
   compression, while higher values gives better compression.
   NOTE: LZ_CompressFast does not use this constant. */
#define LZ_MAX_OFFSET 4096



/*************************************************************************
*                           INTERNAL FUNCTIONS                           *
*************************************************************************/


/*************************************************************************
* _LZ_StringCompare() - Return maximum length string match.
*************************************************************************/

static unsigned int _LZ_StringCompare( unsigned char * str1,
  unsigned char * str2, unsigned int minlen, unsigned int maxlen )
{
    unsigned int len;

    for( len = minlen; (len < maxlen) && (str1[len] == str2[len]); ++ len );
//    for( len = minlen; (len < maxlen) && (str2+len+1 < str1-1) && (str1[len] == str2[len]); ++ len );

    return len;
}


/*************************************************************************
*                            PUBLIC FUNCTIONS                            *
*************************************************************************/


/*************************************************************************
* LZ_Compress() - Compress a block of data using an LZ77 coder.
*  in     - Input (uncompressed) buffer.
*  out    - Output (compressed) buffer. This buffer must be 0.4% larger
*           than the input buffer, plus one byte.
*  insize - Number of input bytes.
* The function returns the size of the compressed data.
*************************************************************************/

int LZ_Compress( unsigned char *in, unsigned char *out,
    unsigned int insize )
{
    unsigned char mask, bundle1, bundle2;
    int  inpos, outpos, bytesleft;
    int  maxoffset, offset, bestoffset;
    int  maxlength, length, bestlength;
    unsigned char *ptr1, *ptr2, *flags;

    /* Do we have anything to compress? */
    if( insize < 1 )
    {
        return 0;
    }

    /* Remember the repetition marker for the decoder */

    out[0] = 0x10;
    out[1] = insize&0xFF;
    out[2] = (insize>>8)&0xFF;
    out[3] = (insize>>16)&0xFF;
    flags = &out[4];
    *flags = 0;
    mask = 128;
    
    /* Start of compression */
    inpos = 0;
    outpos = 5;

    /* Main compression loop */
    bytesleft = insize;
    do
    {
        /* Determine most distant position */
        if( inpos > LZ_MAX_OFFSET ) maxoffset = LZ_MAX_OFFSET;
        else                        maxoffset = inpos;

        /* Get pointer to current position */
        ptr1 = &in[ inpos ];

        /* Search history window for maximum length string match */
        bestlength = 2;
        bestoffset = 0;
        for( offset = 3; offset <= maxoffset; ++ offset )
        {
            /* Get pointer to candidate string */
            ptr2 = &ptr1[ -offset ];

            /* Quickly determine if this is a candidate (for speed) */
            if( (ptr1[ 0 ] == ptr2[ 0 ]) &&
                (ptr1[ bestlength ] == ptr2[ bestlength ]) )
            {
                /* Determine maximum length for this offset */
                maxlength = ((inpos+1) > 18 ? 18 : inpos + 1);

                /* Count maximum length match at this offset */
                length = _LZ_StringCompare( ptr1, ptr2, 0, maxlength );

                /* Better match than any previous match? */
                if( length > bestlength )
                {
                    bestlength = length;
                    bestoffset = offset;
                }
            }
        }

        /* Was there a good enough match? */
        if( bestlength > 2)
        {
            *flags |= mask;
	    mask >>= 1;
	    bundle2 = ((bestlength-3)<<4) | (((bestoffset-1)&0xF00)>>8);
	    bundle1 = (bestoffset-1)&0xFF;
	    out [ outpos++ ] = bundle2;
	    out [ outpos++ ] = bundle1;

            inpos += bestlength;
            bytesleft -= bestlength;
	    if (!mask) {
		    mask = 128;
		    flags = &out [ outpos++ ];
		    *flags = 0;
	    }
        }
        else
        {
	    mask >>= 1;
            out[ outpos ++ ] = in[ inpos++ ];
            -- bytesleft;
	    if (!mask) {
		    mask = 128;
		    flags = &out [ outpos++ ];
		    *flags = 0;
	    }
        }
    }
    while( bytesleft > 3 );

    /* Dump remaining bytes, if any */
    while( inpos < insize )
    {
        out[ outpos ++ ] = in[ inpos++ ];
    }

    while(outpos&3)
	out [ outpos ++] = 0;

    return outpos;
}

int main(int argc, char *argv[])
{
	int in, out, size;
	struct stat buf;
	unsigned char *uncompressed, *compressed;
   
	if (argc == 3) {
		in = open(argv[1], O_RDONLY);
		out = creat(argv[2], 0600);
		fstat(in, &buf);
		uncompressed = malloc(buf.st_size);
		read(in, uncompressed, buf.st_size);
		close(in);
		compressed = malloc(buf.st_size*2);
		size = LZ_Compress(uncompressed, compressed, buf.st_size);
		write(out, "LZ77", 4);
		write(out, compressed, size);
		close(out);
		free(compressed);
		free(uncompressed);
		return 0;
	}
	printf("Usage: %s file.bin file.lz77\n", argv[0]);
	return 1;
}

