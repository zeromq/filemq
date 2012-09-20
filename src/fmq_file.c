/*  =========================================================================
    fmq_file - work with files

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
#include "../include/fmq_file.h"

//  Structure of our class

struct _fmq_file_t {
    char  *name;            //  File name without path
    time_t time;            //  Modification time
    off_t  size;            //  Size of the file
    mode_t mode;            //  POSIX permission bits
};


//  --------------------------------------------------------------------------
//  Constructor

fmq_file_t *
fmq_file_new (const char *name, time_t time, off_t size, mode_t mode)
{
    fmq_file_t
        *self;

    self = (fmq_file_t *) zmalloc (sizeof (fmq_file_t));
    self->name = strdup (name);
    self->time = time;
    self->size = size;
    self->mode = mode;
    return self;
}


//  Destroy a fileectory item
void
fmq_file_destroy (fmq_file_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_file_t *self = *self_p;
        free (self->name);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Return file name
char *
fmq_file_name (fmq_file_t *self)
{
    assert (self);
    return self->name;
}


//  --------------------------------------------------------------------------
//  Return file time
time_t
fmq_file_time (fmq_file_t *self)
{
    assert (self);
    return self->time;
}


//  --------------------------------------------------------------------------
//  Return file size
off_t
fmq_file_size (fmq_file_t *self)
{
    assert (self);
    return self->size;
}


//  --------------------------------------------------------------------------
//  Return file mode
mode_t
fmq_file_mode (fmq_file_t *self)
{
    assert (self);
    return self->mode;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_file_test (bool verbose)
{
    printf (" * fmq_file: ");

    fmq_file_t *file = fmq_file_new ("bilbo", 123456, 100, 0);
    assert (streq (fmq_file_name (file), "bilbo"));
    assert (fmq_file_time (file) == 123456);
    assert (fmq_file_size (file) == 100);
    assert (fmq_file_mode (file) == 0);
    fmq_file_destroy (&file);

    printf ("OK\n");
    return 0;
}
