/*  =========================================================================
    fmq_diff - work with directory diffs

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

#ifndef __FMQ_DIFF_H_INCLUDED__
#define __FMQ_DIFF_H_INCLUDED__

typedef enum {
    diff_create = 1,
    diff_delete = 2,
    diff_resize = 3,
    diff_retime = 4
} fmq_diff_op_t;

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_diff_t fmq_diff_t;

//  Create new diff item
fmq_diff_t *
    fmq_diff_new (fmq_file_t *file, fmq_diff_op_t op);

//  Destroy a diff item
void
    fmq_diff_destroy (fmq_diff_t **self_p);

//  Return diff file item
fmq_file_t *
    fmq_diff_file (fmq_diff_t *self);

//  Return diff operation
fmq_diff_op_t
    fmq_diff_op (fmq_diff_t *self);
    
//  Self test of this class
int
    fmq_diff_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
