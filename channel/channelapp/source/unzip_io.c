/* ioapi_mem2.c -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   This version of ioapi is designed to access memory rather than files.
   We do use a region of memory to put data in to and take it out of. We do
   not have auto-extending buffers and do not inform anyone else that the
   data has been written. It is really intended for accessing a zip archive
   embedded in an application such that I can write an installer with no
   external files. Creation of archives has not been attempted, although
   parts of the framework are present.

   Based on Unzip ioapi.c version 0.22, May 19th, 2003

   Copyright (C) 1998-2003 Gilles Vollant
             (C) 2003 Justin Fletcher

   Dynamically allocated memory version. Troels K 2004
      mem_close deletes the data: file is single-session. No filenames.

   This file is under the same license as the Unzip tool it is distributed
   with.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "panic.h"
#include "unzip_io.h"

static voidpf ZCALLBACK mem_open OF((
   voidpf opaque,
   void* buffer,
   size_t buf_len,
   int mode));

static uLong ZCALLBACK mem_read OF((
   voidpf opaque,
   voidpf stream,
   void* buf,
   uLong size));

static long ZCALLBACK mem_tell OF((
   voidpf opaque,
   voidpf stream));

static long ZCALLBACK mem_seek OF((
   voidpf opaque,
   voidpf stream,
   uLong offset,
   int origin));

static int ZCALLBACK mem_close OF((
   voidpf opaque,
   voidpf stream));

static int ZCALLBACK mem_error OF((
   voidpf opaque,
   voidpf stream));

typedef struct _MEMFILE
{
  void* buffer;    /* Base of the region of memory we're using */
  long length;   /* Size of the region of memory we're using */
  long position; /* Current offset in the area */
} MEMFILE;

static voidpf ZCALLBACK mem_open (opaque, buffer, buf_len, mode)
   voidpf opaque;
   void* buffer;
   size_t buf_len;
   int mode;
{
    MEMFILE* handle = pmalloc(sizeof(*handle));

    handle->position = 0;
    handle->buffer   = buffer;
    handle->length   = buf_len;
    return handle;
}

static uLong ZCALLBACK mem_read (opaque, stream, buf, size)
   voidpf opaque;
   voidpf stream;
   void* buf;
   uLong size;
{
   MEMFILE* handle = (MEMFILE*) stream;

   if ( (handle->position + size) > handle->length)
   {
      size = handle->length - handle->position;
   }
   memcpy(buf, ((char*)handle->buffer) + handle->position, size);
   handle->position+=size;
   return size;
}

static long ZCALLBACK mem_tell (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
   MEMFILE *handle = (MEMFILE *)stream;
   return handle->position;
}

static long ZCALLBACK mem_seek (opaque, stream, offset, origin)
   voidpf opaque;
   voidpf stream;
   uLong offset;
   int origin;
{
   MEMFILE* handle = (MEMFILE*)stream;

   int bOK = 1;
   switch (origin)
   {
      case SEEK_SET :
         //bOK = (offset >= 0) && (offset <= size);
         if (bOK) handle->position = offset;
         break;
      case SEEK_CUR :
         bOK = ((offset + handle->position) >= 0) && (((offset + handle->position) <= handle->length));
         if (bOK) handle->position = offset + handle->position;
         break;
      case SEEK_END:
         bOK = ((handle->length - offset) >= 0) && (((handle->length - offset) <= handle->length));
         if (bOK) handle->position = offset + handle->length - 0;
         break;
      default:
         bOK = 0;
         break;
   }
   return bOK ? 0 : -1;
}

int ZCALLBACK mem_close (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    MEMFILE *handle = (MEMFILE *)stream;

    free (handle);
    return 0;
}

int ZCALLBACK mem_error (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    //MEMFILE *handle = (MEMFILE *)stream;
    /* We never return errors */
    return 0;
}

void mem_simple_create_file(zlib_filefunc_def* api)
{
    api->zopen_file  = mem_open;
    api->zread_file  = mem_read;
    api->zwrite_file = NULL;
    api->ztell_file  = mem_tell;
    api->zseek_file  = mem_seek;
    api->zclose_file = mem_close;
    api->zerror_file = mem_error;
    api->opaque      = NULL;
}

