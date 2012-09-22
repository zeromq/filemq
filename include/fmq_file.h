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
    fmq_file_new (const char *path, const char *name, time_t time, off_t size, mode_t mode);

//  Destroy a file item
void
    fmq_file_destroy (fmq_file_t **self_p);

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
    
//  Self test of this class
int
    fmq_file_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif

