/*  =========================================================================
    fmq_sha - wraps the sha-1.1 library

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

#ifndef __FMQ_SHA_H_INCLUDED__
#define __FMQ_SHA_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_sha_t fmq_sha_t;

//  Create new SHA object
fmq_sha_t *
    fmq_sha_new (void);

//  Destroy a SHA object
void
    fmq_sha_destroy (fmq_sha_t **self_p);

//  Add buffer into SHA calculation
void
    fmq_sha_update (fmq_sha_t *self, byte *buffer, size_t length);
    
//  Return final SHA hash data
byte *
    fmq_sha_hash_data (fmq_sha_t *self);

//  Return final SHA hash size
size_t
    fmq_sha_hash_size (fmq_sha_t *self);

//  Self test of this class
int
    fmq_sha_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
