/*  =========================================================================
    fmq_diff - work with directory diffs

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
#include "../include/fmq_diff.h"

//  Structure of our class

struct _fmq_diff_t {
    fmq_file_t *file;           //  File we refer to
    fmq_diff_op_t op;           //  Operation
};


//  --------------------------------------------------------------------------
//  Constructor

fmq_diff_t *
fmq_diff_new (fmq_file_t *file, fmq_diff_op_t op)
{
    fmq_diff_t
        *self;

    self = (fmq_diff_t *) zmalloc (sizeof (fmq_diff_t));
    self->file = fmq_file_dup (file);
    self->op = op;
    return self;
}

//  --------------------------------------------------------------------------
//  Destroy a diff item

void
fmq_diff_destroy (fmq_diff_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_diff_t *self = *self_p;
        fmq_file_destroy (&self->file);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Return diff file item

fmq_file_t *
fmq_diff_file (fmq_diff_t *self)
{
    assert (self);
    return self->file;
}


//  --------------------------------------------------------------------------
//  Return diff operation

fmq_diff_op_t
fmq_diff_op (fmq_diff_t *self)
{
    assert (self);
    return self->op;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_diff_test (bool verbose)
{
    printf (" * fmq_diff: ");

    fmq_file_t *file = fmq_file_new (".", "bilbo", 123456, 100, 0);
    fmq_diff_t *diff = fmq_diff_new (file, diff_create);
    fmq_file_destroy (&file);
    
    file = fmq_diff_file (diff);
    assert (streq (fmq_file_name (file), "bilbo"));
    assert (fmq_file_time (file) == 123456);
    assert (fmq_file_size (file) == 100);
    assert (fmq_file_mode (file) == 0);
    
    fmq_diff_destroy (&diff);

    printf ("OK\n");
    return 0;
}
