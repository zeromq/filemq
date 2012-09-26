/*  =========================================================================
    fmq_patch - work with directory patches
    A patch is a change to the directory (create/delete/resize/retime).

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
#include "../include/fmq_patch.h"

//  Structure of our class

struct _fmq_patch_t {
    fmq_file_t *file;           //  File we refer to
    fmq_patch_op_t op;          //  Operation
    int number;                 //  Patch number
};


//  --------------------------------------------------------------------------
//  Constructor
//  Create new patch

fmq_patch_t *
fmq_patch_new (fmq_file_t *file, fmq_patch_op_t op, int number)
{
    fmq_patch_t *self = (fmq_patch_t *) zmalloc (sizeof (fmq_patch_t));
    self->file = fmq_file_dup (file);
    self->op = op;
    self->number = number;
    return self;
}

//  --------------------------------------------------------------------------
//  Destroy a patch

void
fmq_patch_destroy (fmq_patch_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_patch_t *self = *self_p;
        fmq_file_destroy (&self->file);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Create copy of a patch

fmq_patch_t *
fmq_patch_dup (fmq_patch_t *self)
{
    return fmq_patch_new (self->file, self->op, self->number);
}


//  --------------------------------------------------------------------------
//  Return patch file item

fmq_file_t *
fmq_patch_file (fmq_patch_t *self)
{
    assert (self);
    return self->file;
}


//  --------------------------------------------------------------------------
//  Return operation

fmq_patch_op_t
fmq_patch_op (fmq_patch_t *self)
{
    assert (self);
    return self->op;
}


//  --------------------------------------------------------------------------
//  Get patch number

int
fmq_patch_number (fmq_patch_t *self)
{
    assert (self);
    return self->number;
}


//  --------------------------------------------------------------------------
//  Set patch number

void
fmq_patch_set_number (fmq_patch_t *self, int number)
{
    assert (self);
    self->number = number;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_patch_test (bool verbose)
{
    printf (" * fmq_patch: ");

    fmq_file_t *file = fmq_file_new (".", "bilbo", 123456, 100, 0);
    fmq_patch_t *patch = fmq_patch_new (file, patch_create, 123);
    fmq_file_destroy (&file);
    
    file = fmq_patch_file (patch);
    assert (streq (fmq_file_name (file), "./bilbo"));
    assert (fmq_file_time (file) == 123456);
    assert (fmq_file_size (file) == 100);
    assert (fmq_file_mode (file) == 0);
    assert (fmq_patch_number (patch) == 123);
    
    fmq_patch_destroy (&patch);

    printf ("OK\n");
    return 0;
}
