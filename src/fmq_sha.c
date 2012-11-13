/*  =========================================================================
    fmq_sha - wraps the sha-1.1 library

    sha-1.1 is copyright (c) 2001-2003 Allan Saddi <allan@saddi.com>
    http://www.saddi.com/software/sha/. Note sha-1.1 is slightly shaed to
    build properly with FileMQ.

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
    =========================================================================
*/

#include <czmq.h>
#include "../include/fmq.h"
#include "../sha-1.1/sha1.h"


//  Structure of our class

struct _fmq_sha_t {
    SHA1_Context context;
    byte hash [SHA1_HASH_SIZE];
    bool final;
};


//  --------------------------------------------------------------------------
//  Constructor
//  Create new SHA object

fmq_sha_t *
fmq_sha_new (void)
{
    fmq_sha_t *self = (fmq_sha_t *) zmalloc (sizeof (fmq_sha_t));
    SHA1_Init (&self->context);
    return self;
}

//  --------------------------------------------------------------------------
//  Destroy a SHA object

void
fmq_sha_destroy (fmq_sha_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_sha_t *self = *self_p;
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Add buffer into SHA calculation

void
fmq_sha_update (fmq_sha_t *self, byte *buffer, size_t length)
{
    SHA1_Update (&self->context, buffer, length);
}


//  --------------------------------------------------------------------------
//  Return final SHA hash data

byte *
fmq_sha_hash_data (fmq_sha_t *self)
{
    if (!self->final) {
        SHA1_Final (&self->context, self->hash);
        self->final = true;
    }
    return self->hash;
}


//  --------------------------------------------------------------------------
//  Return final SHA hash size

size_t
fmq_sha_hash_size (fmq_sha_t *self)
{
    return SHA1_HASH_SIZE;
}


//  --------------------------------------------------------------------------
//  Self test of this class

int
fmq_sha_test (bool verbose)
{
    printf (" * fmq_sha: ");

    byte buffer [1024];
    memset (buffer, 0xAA, 1024);
    
    fmq_sha_t *sha = fmq_sha_new ();
    fmq_sha_update (sha, buffer, 1024);
    byte *hash = fmq_sha_hash_data (sha);
    size_t size = fmq_sha_hash_size (sha);
    assert (hash [0] == 0xDE);
    assert (hash [1] == 0xB2);
    assert (hash [2] == 0x38);
    assert (hash [3] == 0x07);
    fmq_sha_destroy (&sha);

    printf ("OK\n");
    return 0;
}
