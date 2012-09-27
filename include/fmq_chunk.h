/*  =========================================================================
    fmq_chunk - work with memory chunks

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation -- http://www.imatix.com
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of FILEMQ, see http://filemq.org.

    This is free software; you can redistribute it and/or modify it under the
    terms of the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your option)
    any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABIL-
    ITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
    Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#ifndef __FMQ_CHUNK_H_INCLUDED__
#define __FMQ_CHUNK_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_chunk_t fmq_chunk_t;

//  Create new chunk
fmq_chunk_t *
    fmq_chunk_new (const void *data, size_t size);

//  Destroy a chunk
void
    fmq_chunk_destroy (fmq_chunk_t **self_p);

//  Resizes chunk max_size as requested; chunk_cur size is set to zero
void
    fmq_chunk_resize (fmq_chunk_t *self, size_t size);

//  Return chunk cur size
size_t
    fmq_chunk_cur_size (fmq_chunk_t *self);

//  Return chunk max size
size_t
    fmq_chunk_max_size (fmq_chunk_t *self);

//  Return chunk data
byte *
    fmq_chunk_data (fmq_chunk_t *self);

//  Set chunk data from user-supplied data; truncate if too large
size_t
    fmq_chunk_set (fmq_chunk_t *self, const void *data, size_t size);

//  Fill chunk data from user-supplied octet
size_t
    fmq_chunk_fill (fmq_chunk_t *self, byte filler, size_t size);

//  Write chunk to an open file descriptor
int
    fmq_chunk_write (fmq_chunk_t *self, FILE *handle);
    
//  Read chunk to an open file descriptor
fmq_chunk_t *
    fmq_chunk_read (FILE *handle, size_t bytes);
    
//  Self test of this class
int
    fmq_chunk_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif

