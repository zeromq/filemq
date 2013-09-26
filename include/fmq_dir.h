/*  =========================================================================
    fmq_dir - work with file-system directories

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

#ifndef __FMQ_DIR_H_INCLUDED__
#define __FMQ_DIR_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_dir_t fmq_dir_t;

//  @interface
//  Create new directory item
fmq_dir_t *
    fmq_dir_new (const char *path, const char *parent);

//  Destroy a directory item
void
    fmq_dir_destroy (fmq_dir_t **self_p);

//  Return directory path
char *
    fmq_dir_path (fmq_dir_t *self);

//  Return last modified time
time_t
    fmq_dir_time (fmq_dir_t *self);

//  Return total hierarchy size
off_t
    fmq_dir_size (fmq_dir_t *self);

//  Calculate differences between two versions of a directory tree
zlist_t *
    fmq_dir_diff (fmq_dir_t *older, fmq_dir_t *newer, char *alias);

//  Return full contents of directory as a patch list.
zlist_t *
    fmq_dir_resync (fmq_dir_t *self, char *alias);

//  Return total hierarchy count
size_t
    fmq_dir_count (fmq_dir_t *self);

//  Return sorted array of file references
zfile_t **
    fmq_dir_flatten (fmq_dir_t *self);

//  Remove directory, optionally including all files
void
    fmq_dir_remove (fmq_dir_t *self, bool force);

//  Print contents of directory
void
    fmq_dir_dump (fmq_dir_t *self, int indent);
    
//  Load directory cache; returns a hash table containing the SHA-1 digests
//  of every file in the tree. The cache is saved between runs in .cache.
//  The caller must destroy the hash table when done with it.
zhash_t *
    fmq_dir_cache (fmq_dir_t *self);

//  Self test of this class
int
    fmq_dir_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif

