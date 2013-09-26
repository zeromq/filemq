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

/*
@header
    The fmq_patch class works with one patch, which really just says "create
    this file" or "delete this file" (referring to a zfile item each time).
@discuss
@end
*/

#include <czmq.h>
#include "../include/fmq.h"

//  Structure of our class
//  If you modify this beware to also change _dup

struct _fmq_patch_t {
    char *path;                 //  Directory path
    char *vpath;                //  Virtual file path
    zfile_t *file;              //  File we refer to
    fmq_patch_op_t op;          //  Operation
    char *digest;               //  File SHA-1 digest
};


//  --------------------------------------------------------------------------
//  Constructor
//  Create new patch, create virtual path from alias

fmq_patch_t *
fmq_patch_new (char *path, zfile_t *file, fmq_patch_op_t op, char *alias)
{
    fmq_patch_t *self = (fmq_patch_t *) zmalloc (sizeof (fmq_patch_t));
    self->path = strdup (path);
    self->file = zfile_dup (file);
    self->op = op;
    
    //  Calculate virtual path for patch (remove path, prefix alias)
    char *filename = zfile_filename (file, path);
    assert (*filename != '/');
    self->vpath = (char *) zmalloc (strlen (alias) + strlen (filename) + 2);
    if (alias [strlen (alias) - 1] == '/')
        sprintf (self->vpath, "%s%s", alias, filename);
    else
        sprintf (self->vpath, "%s/%s", alias, filename);
    
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
        free (self->vpath);
        free (self->digest);
        zfile_destroy (&self->file);
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
    copy->file = zfile_dup (self->file);
    copy->op = self->op;
    copy->vpath = strdup (self->vpath);
    //  Don't recalculate hash when we duplicate patch
    copy->digest = self->digest? strdup (self->digest): NULL;
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

zfile_t *
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
//  Return patch virtual file path

char *
fmq_patch_vpath (fmq_patch_t *self)
{
    assert (self);
    return self->vpath;
}


//  Return file SHA-1 hash as string; caller has to free it
//  TODO: this has to be moved into CZMQ along with SHA1 and hash class

static char *
s_file_hash (zfile_t *file)
{
    if (zfile_input (file) == -1)
        return NULL;            //  Problem reading directory

    //  Now calculate hash for file data, chunk by chunk
    size_t blocksz = 65535;
    off_t offset = 0;
    fmq_hash_t *hash = fmq_hash_new ();
    zchunk_t *chunk = zfile_read (file, blocksz, offset);
    while (zchunk_size (chunk)) {
        fmq_hash_update (hash, zchunk_data (chunk), zchunk_size (chunk));
        zchunk_destroy (&chunk);
        offset += blocksz;
        chunk = zfile_read (file, blocksz, offset);
    }
    zchunk_destroy (&chunk);
    zfile_close (file);

    //  Convert to printable string
    char hex_char [] = "0123456789ABCDEF";
    char *hashstr = (char *) zmalloc (fmq_hash_size (hash) * 2 + 1);
    int byte_nbr;
    for (byte_nbr = 0; byte_nbr < fmq_hash_size (hash); byte_nbr++) {
        hashstr [byte_nbr * 2 + 0] = hex_char [fmq_hash_data (hash) [byte_nbr] >> 4];
        hashstr [byte_nbr * 2 + 1] = hex_char [fmq_hash_data (hash) [byte_nbr] & 15];
    }
    fmq_hash_destroy (&hash);
    return hashstr;
}


//  --------------------------------------------------------------------------
//  Calculate hash digest for file (create only)

void
fmq_patch_digest_set (fmq_patch_t *self)
{
    if (self->op == patch_create
    &&  self->digest == NULL)
        self->digest = s_file_hash (self->file);
}


//  --------------------------------------------------------------------------
//  Return hash digest for patch file (create only)

char *
fmq_patch_digest (fmq_patch_t *self)
{
    assert (self);
    return self->digest;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_patch_test (bool verbose)
{
    printf (" * fmq_patch: ");

    //  @selftest
    zfile_t *file = zfile_new (".", "bilbo");
    fmq_patch_t *patch = fmq_patch_new (".", file, patch_create, "/");
    zfile_destroy (&file);
    
    file = fmq_patch_file (patch);
    assert (streq (zfile_filename (file, "."), "bilbo"));
    assert (streq (fmq_patch_vpath (patch), "/bilbo"));
    fmq_patch_destroy (&patch);
    //  @end

    printf ("OK\n");
    return 0;
}
