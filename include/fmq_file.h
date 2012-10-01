/*  =========================================================================
    fmq_file - work with files

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

#ifndef __FMQ_FILE_H_INCLUDED__
#define __FMQ_FILE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_file_t fmq_file_t;

//  Create new file item
fmq_file_t *
    fmq_file_new (const char *path, const char *name);

//  Destroy a file item
void
    fmq_file_destroy (fmq_file_t **self_p);

//  Refreshes file properties from file system
void
    fmq_file_restat (fmq_file_t *self);

//  Duplicate a file item
fmq_file_t *
    fmq_file_dup (fmq_file_t *self);

//  Return file name
char *
    fmq_file_name (fmq_file_t *self);

//  Return file time
time_t
    fmq_file_time (fmq_file_t *self);

//  Return file size
off_t
    fmq_file_size (fmq_file_t *self);
    
//  Return file mode
mode_t
    fmq_file_mode (fmq_file_t *self);
    
//  Check if file exists/ed; does not restat file
bool
    fmq_file_exists (fmq_file_t *self);

//  Check if file is/was stable; does not restat file
bool
    fmq_file_stable (fmq_file_t *self);

//  Remove the file
void
    fmq_file_remove (fmq_file_t *self);

//  Open file for reading
int
    fmq_file_input (fmq_file_t *self);

//  Open file for writing, creating directory if needed
int
    fmq_file_output (fmq_file_t *self);
    
//  Read chunk from file at specified position
fmq_chunk_t *
    fmq_file_read (fmq_file_t *self, size_t bytes, off_t offset);

//  Write chunk to file at specified position
int
    fmq_file_write (fmq_file_t *self, fmq_chunk_t *chunk, off_t offset);

//  Close file, if open
void
    fmq_file_close (fmq_file_t *self);
    
//  Self test of this class
int
    fmq_file_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
