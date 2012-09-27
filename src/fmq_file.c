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
#include "../include/fmq.h"

//  Structure of our class

struct _fmq_file_t {
    char  *name;            //  File name with path
    time_t time;            //  Modification time
    off_t  size;            //  Size of the file
    mode_t mode;            //  POSIX permission bits
    FILE *handle;           //  Read/write handle
};


//  --------------------------------------------------------------------------
//  Constructor

fmq_file_t *
fmq_file_new (const char *path, const char *name, time_t time, off_t size, mode_t mode)
{
    fmq_file_t *self = (fmq_file_t *) zmalloc (sizeof (fmq_file_t));
    self->name = malloc (strlen (path) + strlen (name) + 2);
    sprintf (self->name, "%s/%s", path, name);
    self->time = time;
    self->size = size;
    self->mode = mode;
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy a file item

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
//  Duplicate a file item

fmq_file_t *
fmq_file_dup (fmq_file_t *self)
{
    fmq_file_t
        *copy;

    copy = (fmq_file_t *) zmalloc (sizeof (fmq_file_t));
    copy->name = strdup (self->name);
    copy->time = self->time;
    copy->size = self->size;
    copy->mode = self->mode;
    return copy;
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


//  Return POSIX file mode or -1 if file doesn't exist

static mode_t
s_file_mode (const char *filename)
{
#if (defined (WIN32))
    DWORD dwfa = GetFileAttributes (filename);
    if (dwfa == 0xffffffff)
        return -1;

    dbyte mode = 0;
    if (dwfa & FILE_ATTRIBUTE_DIRECTORY)
        mode |= S_IFDIR;
    else
        mode |= S_IFREG;
    if (!(dwfa & FILE_ATTRIBUTE_HIDDEN))
        mode |= S_IREAD;
    if (!(dwfa & FILE_ATTRIBUTE_READONLY))
        mode |= S_IWRITE;

    return mode;
#else
    struct stat stat_buf;
    if (stat ((char *) filename, &stat_buf) == 0)
        return stat_buf.st_mode;
    else
        return -1;
#endif
}


//  --------------------------------------------------------------------------
//  Remove the file

void
fmq_file_remove (fmq_file_t *self)
{
    assert (self);
#if (defined (WIN32))
    DeleteFile (self->name);
#else
    unlink (self->name);
#endif
}


static void
s_assert_path (fmq_file_t *self)
{
    //  Create parent directory levels if needed
    char *path = strdup (self->name);
    char *slash = strchr (path + 1, '/');
    do {
        if (slash)
            *slash = 0;         //  Cut at slash
        mode_t mode = s_file_mode (path);
        if (mode == -1) {
            //  Does not exist, try to create it
#if (defined (WIN32))
            if (CreateDirectory (path, NULL))
#else
            if (mkdir (path, 0775))
#endif
                return;         //  Failed
        }
        else
        if ((mode & S_IFDIR) == 0) {
            //  Not a directory, abort
        }
        if (!slash)             //  End if last segment
            break;
       *slash = '/';
        slash = strchr (slash + 1, '/');
    } while (slash);

    free (path);
}


//  --------------------------------------------------------------------------
//  Open file for reading
//  Returns 0 if OK, -1 if not found or not accessible

int
fmq_file_input (fmq_file_t *self)
{
    assert (self);
    if (self->handle)
        fmq_file_close (self);
    
    self->handle = fopen (self->name, "rb");
    if (self->handle) {
        struct stat stat_buf;
        if (stat (self->name, &stat_buf) == 0)
            self->size = stat_buf.st_size;
        else
            self->size = 0;
    }
    return self->handle? 0: -1;
}


//  --------------------------------------------------------------------------
//  Open file for writing, creating directory if needed
//  File is created if necessary; chunks can be written to file at any
//  location. Returns 0 if OK, -1 if error.

int
fmq_file_output (fmq_file_t *self)
{
    assert (self);
    s_assert_path (self);
    if (self->handle)
        fmq_file_close (self);

    //  Create file if it doesn't exist, and always 
    self->handle = fopen (self->name, "r+b");
    if (!self->handle )
        self->handle = fopen (self->name, "w+b");
    return self->handle? 0: -1;
}


//  --------------------------------------------------------------------------
//  Read chunk from file at specified position
//  Zero-sized chunk means we're at the end of the file
//  Null chunk means there was another error

fmq_chunk_t *
fmq_file_read (fmq_file_t *self, size_t bytes, off_t offset)
{
    assert (self);
    assert (self->handle);
    //  Calculate real number of bytes to read
    if (offset > self->size)
        bytes = 0;
    else
    if (bytes > self->size - offset)
        bytes = self->size - offset;

    int rc = fseek (self->handle, (long) offset, SEEK_SET);
    if (rc == -1)
        return NULL;
    
    return fmq_chunk_read (self->handle, bytes);
}


//  --------------------------------------------------------------------------
//  Write chunk to file at specified position
//  Return 0 if OK, else -1

int
fmq_file_write (fmq_file_t *self, fmq_chunk_t *chunk, off_t offset)
{
    assert (self);
    assert (self->handle);
    int rc = fseek (self->handle, (long) offset, SEEK_SET);
    if (rc >= 0)
        rc = fmq_chunk_write (chunk, self->handle);
    return rc;
}


//  --------------------------------------------------------------------------
//  Close file, if open

void
fmq_file_close (fmq_file_t *self)
{
    assert (self);
    if (self->handle)
        fclose (self->handle);
    self->handle = 0;
}


//  --------------------------------------------------------------------------
//  Self test of this class

int
fmq_file_test (bool verbose)
{
    printf (" * fmq_file: ");

    fmq_file_t *file = fmq_file_new (".", "bilbo", 123456, 100, 0);
    assert (streq (fmq_file_name (file), "./bilbo"));
    assert (fmq_file_time (file) == 123456);
    assert (fmq_file_size (file) == 100);
    assert (fmq_file_mode (file) == 0);
    fmq_file_destroy (&file);

    //  Create a test file in some random subdirectory
    file = fmq_file_new ("./this/is/a/test", "bilbo", 0, 0, 0);
    int rc = fmq_file_output (file);
    assert (rc == 0);
    fmq_chunk_t *chunk = fmq_chunk_new (NULL, 100);
    fmq_chunk_fill (chunk, 0, 100);
    //  Write 100 bytes at position 1,000,000 in the file
    rc = fmq_file_write (file, chunk, 1000000);
    assert (rc == 0);
    fmq_file_close (file);
    fmq_chunk_destroy (&chunk);

    //  Check we can read from file
    rc = fmq_file_input (file);
    assert (rc == 0);
    chunk = fmq_file_read (file, 1000100, 0);
    assert (chunk);
    assert (fmq_chunk_cur_size (chunk) == 1000100);
    fmq_chunk_destroy (&chunk);
    
    //  Remove file and directory
    fmq_dir_t *dir = fmq_dir_new ("./this", NULL);
    assert (fmq_dir_size (dir) == 1000100);
    fmq_dir_remove (dir, true);
    assert (fmq_dir_size (dir) == 0);
    fmq_dir_destroy (&dir);

    //  Check we can no longer read from file
    rc = fmq_file_input (file);
    assert (rc == -1);
    fmq_file_destroy (&file);
        
    printf ("OK\n");
    return 0;
}
