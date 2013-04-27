/*  =========================================================================
    fmq_patch - work with directory patches

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

#ifndef __FMQ_PATCH_H_INCLUDED__
#define __FMQ_PATCH_H_INCLUDED__

typedef enum {
    patch_create = 1,
    patch_delete = 2
} fmq_patch_op_t;

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_patch_t fmq_patch_t;

//  @interface
//  Create new patch
fmq_patch_t *
    fmq_patch_new (char *path, fmq_file_t *file, fmq_patch_op_t op, char *alias);

//  Destroy a patch
void
    fmq_patch_destroy (fmq_patch_t **self_p);

//  Create copy of a patch
fmq_patch_t *
    fmq_patch_dup (fmq_patch_t *self);

//  Return patch file directory path
char *
    fmq_patch_path (fmq_patch_t *self);

//  Return patch file item
fmq_file_t *
    fmq_patch_file (fmq_patch_t *self);

//  Return operation
fmq_patch_op_t
    fmq_patch_op (fmq_patch_t *self);
    
//  Return patch virtual file name
char *
    fmq_patch_virtual (fmq_patch_t *self);

//  Set patch virtual file name
void
    fmq_patch_virtual_set (fmq_patch_t *self, char *virtual);

//  Calculate hash digest for file (create only)
void
    fmq_patch_digest_set (fmq_patch_t *self);

//  Return hash digest for patch file 
char *
    fmq_patch_digest (fmq_patch_t *self);
    
//  Self test of this class
int
    fmq_patch_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
