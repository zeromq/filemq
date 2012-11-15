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
#include "../include/fmq.h"

//  Structure of our class
//  If you modify this beware to also change _dup

struct _fmq_patch_t {
    char *path;                 //  Directory path
    char *virtual;              //  Virtual file name
    fmq_file_t *file;           //  File we refer to
    fmq_patch_op_t op;          //  Operation
    char *hashstr;              //  File hash digest
};


//  --------------------------------------------------------------------------
//  Constructor
//  Create new patch

fmq_patch_t *
fmq_patch_new (char *path, fmq_file_t *file, fmq_patch_op_t op)
{
    fmq_patch_t *self = (fmq_patch_t *) zmalloc (sizeof (fmq_patch_t));
    self->path = strdup (path);
    self->file = fmq_file_dup (file);
    self->op = op;
    if (self->op == patch_create)
        self->hashstr = fmq_file_hash (self->file);
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
        free (self->path);
        free (self->virtual);
        free (self->hashstr);
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
    fmq_patch_t *copy = (fmq_patch_t *) zmalloc (sizeof (fmq_patch_t));
    copy->path = strdup (self->path);
    copy->file = fmq_file_dup (self->file);
    copy->op = self->op;
    //  Don't recalculate hash when we duplicate patch
    copy->hashstr = self->hashstr? strdup (self->hashstr): NULL;
    copy->virtual = self->virtual? strdup (self->virtual): NULL;
    return copy;
}


//  --------------------------------------------------------------------------
//  Return patch file directory path

char *
fmq_patch_path (fmq_patch_t *self)
{
    assert (self);
    return self->path;
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
//  Return patch virtual file name

char *
fmq_patch_virtual (fmq_patch_t *self)
{
    assert (self);
    return self->virtual;
}


//  --------------------------------------------------------------------------
//  Set patch virtual file name

void
fmq_patch_virtual_set (fmq_patch_t *self, char *virtual)
{
    assert (self);
    free (self->virtual);
    self->virtual = strdup (virtual);
}


//  --------------------------------------------------------------------------
//  Return hash digest for patch file (create only)

char *
fmq_patch_hashstr (fmq_patch_t *self)
{
    assert (self);
    return self->hashstr;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_patch_test (bool verbose)
{
    printf (" * fmq_patch: ");

    fmq_file_t *file = fmq_file_new (".", "bilbo");
    fmq_patch_t *patch = fmq_patch_new (".", file, patch_create);
    fmq_file_destroy (&file);
    
    file = fmq_patch_file (patch);
    assert (streq (fmq_file_name (file, "."), "bilbo"));
    fmq_patch_virtual_set (patch, "/virtual/path");
    assert (streq (fmq_patch_virtual (patch), "/virtual/path"));
    fmq_patch_destroy (&patch);

    printf ("OK\n");
    return 0;
}
