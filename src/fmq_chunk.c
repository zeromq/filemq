/*  =========================================================================
    fmq_chunk - work with memory chunks

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation -- http://www.imatix.com
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of FILEMQ, see http://filemq.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the
    Free Software Foundation; either version 3 of the License, or (at your
    option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTA-
    BILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
    Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see http://www.gnu.org/licenses/.
    =========================================================================*/


#include <czmq.h>
#include "../include/fmq.h"

//  Structure of our class

struct _fmq_chunk_t {
    size_t cur_size;            //  Current size of data part
    size_t max_size;            //  Maximum allocated size
    byte  *data;                //  Data part follows here
};


//  --------------------------------------------------------------------------
//  Constructor

fmq_chunk_t *
fmq_chunk_new (const void *data, size_t size)
{
    fmq_chunk_t *self = (fmq_chunk_t *) zmalloc (sizeof (fmq_chunk_t) + size);
    if (self) {
        self->max_size = size;
        self->data = (byte *) self + sizeof (fmq_chunk_t);
        if (data) {
            self->cur_size = size;
            memcpy (self->data, data, size);
        }
        else
            self->cur_size = 0;
    }
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy a chunk

void
fmq_chunk_destroy (fmq_chunk_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_chunk_t *self = *self_p;
        //  If data was reallocated independently, free it independently
        if (self->data != (byte *) self + sizeof (fmq_chunk_t))
            free (self->data);

        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Resizes chunk max_size as requested; chunk_cur size is set to zero

void
fmq_chunk_resize (fmq_chunk_t *self, size_t size)
{
    //  If data was reallocated independently, free it independently
    if (self->data != (byte *) self + sizeof (fmq_chunk_t))
        free (self->data);

    self->data = zmalloc (size);
    self->max_size = size;
    self->cur_size = 0;
}


//  --------------------------------------------------------------------------
//  Set chunk data from user-supplied data; truncate if too large
//  Returns actual size of chunk

size_t
fmq_chunk_set (fmq_chunk_t *self, const void *data, size_t size)
{
    assert (self);
    if (size > self->max_size)
        size = self->max_size;
    memcpy (self->data, data, size);
    self->cur_size = size;
    return size;
}


//  --------------------------------------------------------------------------
//  Fill chunk data from user-supplied octet
//  Returns actual size of chunk

size_t
fmq_chunk_fill (fmq_chunk_t *self, byte filler, size_t size)
{
    assert (self);
    if (size > self->max_size)
        size = self->max_size;
    memset (self->data, filler, size);
    self->cur_size = size;
    return size;
}


//  --------------------------------------------------------------------------
//  Return chunk cur size

size_t
fmq_chunk_cur_size (fmq_chunk_t *self)
{
    assert (self);
    return self->cur_size;
}


//  --------------------------------------------------------------------------
//  Return chunk max size

size_t
fmq_chunk_max_size (fmq_chunk_t *self)
{
    assert (self);
    return self->max_size;
}


//  --------------------------------------------------------------------------
//  Return chunk data

byte *
fmq_chunk_data (fmq_chunk_t *self)
{
    assert (self);
    return self->data;
}


//  --------------------------------------------------------------------------
//  Write chunk to an open file descriptor

int
fmq_chunk_write (fmq_chunk_t *self, FILE *handle)
{
    size_t items = fwrite (self->data, 1, self->cur_size, handle);
    int rc = (items < self->cur_size)? -1: 0;
    return rc;
}


//  --------------------------------------------------------------------------
//  Read chunk to an open file descriptor

fmq_chunk_t *
fmq_chunk_read (FILE *handle, size_t bytes)
{
    fmq_chunk_t *self = fmq_chunk_new (NULL, bytes);
    self->cur_size = fread (self->data, 1, bytes, handle);
    return self;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_chunk_test (bool verbose)
{
    printf (" * fmq_chunk: ");

    fmq_chunk_t *chunk = fmq_chunk_new ("1234567890", 10);
    assert (chunk);
    assert (fmq_chunk_cur_size (chunk) == 10);
    assert (memcmp (fmq_chunk_data (chunk), "1234567890", 10) == 0);
    fmq_chunk_destroy (&chunk);

    printf ("OK\n");
    return 0;
}
