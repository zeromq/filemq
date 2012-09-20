/*  =========================================================================
    fmq_dir - work with file-system directories

    Has some untested and probably incomplete support for Win32.
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
    =========================================================================*/


#include <czmq.h>
#include "../include/fmq_file.h"
#include "../include/fmq_dir.h"

//  Structure of our class

struct _fmq_dir_t {
    char *path;                 //  Directory name + separator
    zlist_t *files;             //  List of files in directory
    zlist_t *subdirs;           //  List of subdirectories
};

#if (defined (WIN32))
static void
s_win32_populate_entry (fmq_dir_t *self, WIN32_FIND_DATA *entry)
{
    //  Calculate file time (I suspect this is over-complex)
    unsigned long thi, tlo;
    double dthi, dtlo;
    double secs_since_1601;
    double delta = 11644473600.;
    double two_to_32 = 4294967296.;
    thi = entry->ftLastWriteTime.dwHighDateTime;
    tlo = entry->ftLastWriteTime.dwLowDateTime;
    dthi = (double) thi;
    dtlo = (double) tlo;
    secs_since_1601 = (dthi * two_to_32 + dtlo) / 1.0e7;
    time_t file_time = (unsigned long) (secs_since_1601 - delta);
    
    //  Calculate file mode
    mode_t file_mode = 0;
    if (entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        file_mode |= S_IFDIR;
    else
        file_mode |= S_IFREG;
    if (!(entry->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
        file_mode |= S_IREAD;
    if (!(entry->dwFileAttributes & FILE_ATTRIBUTE_READONLY))
        file_mode |= S_IWRITE;

    //  If we have a subdirectory, go load that
    if (file_mode & S_IFDIR) {
        fmq_dir_t *subdir = fmq_dir_new (entry->cFileName, self->path);
        zlist_append (self->subdirs, subdir);
    }
    else {
        //  Add file entry to directory list
        fmq_file_t *file = fmq_file_new (
            entry->cFileName, file_time, entry->nFileSizeLow, file_mode);
        zlist_append (self->files, file);
    }
}

#else
static void
s_posix_populate_entry (fmq_dir_t *self, struct dirent *entry)
{
    //  Skip . and ..
    if (streq (entry->d_name, ".")
    ||  streq (entry->d_name, ".."))
        return;
        
    char fullpath [1024 + 1];
    snprintf (fullpath, 1024, "%s/%s", self->path, entry->d_name);
    struct stat stat_buf;
    if (stat (fullpath, &stat_buf) == 0) {
        //  If we have a subdirectory, go load that
        if (stat_buf.st_mode & S_IFDIR) {
            fmq_dir_t *subdir = fmq_dir_new (entry->d_name, self->path);
            zlist_append (self->subdirs, subdir);
        }
        else {
            //  Add file entry to directory list
            fmq_file_t *file = fmq_file_new (
                entry->d_name, stat_buf.st_mtime,
                stat_buf.st_size, stat_buf.st_mode);
            zlist_append (self->files, file);
        }
    }
}
#endif


//  --------------------------------------------------------------------------
//  Constructor
//  Loads full directory tree

fmq_dir_t *
fmq_dir_new (const char *name, const char *parent)
{
    fmq_dir_t
        *self;

    self = (fmq_dir_t *) zmalloc (sizeof (fmq_dir_t));
    if (parent) {
        self->path = malloc (strlen (name) + strlen (parent) + 2);
        sprintf (self->path, "%s/%s", parent, name);
    }
    else
        self->path = strdup (name);
    self->files = zlist_new ();
    self->subdirs = zlist_new ();
    
#if (defined (WIN32))
    //  On Windows, replace backslashes by normal slashes
    char *path_clean_ptr = self->path;
    while (*path_clean_ptr) {
        if (*path_clean_ptr == '\\')
            *path_clean_ptr = '/';
        path_clean_ptr++;
    }
    //  Remove any trailing slash
    if (self->path [strlen (self->path) - 1] == '/')
        self->path [strlen (self->path) - 1] = 0;
    
    //  Win32 wants a wildcard at the end of the path
    char wildcard = malloc (strlen (self->path + 2));
    sprintf (wildcard, "%s//", self->path);
    WIN32_FIND_DATA entry;
    HANDLE handle = FindFirstFile (wildcard, &entry);
    free (wildcard);
    
    if (handle != INVALID_HANDLE_VALUE) {
        //  We have read an entry, so return those values
        win_populate_entry (self, entry);
        while (FindNextFile (handle, &entry))
            s_win32_populate_entry (self, &entry);
    }
#else
    //  Remove any trailing slash
    if (self->path [strlen (self->path) - 1] == '/')
        self->path [strlen (self->path) - 1] = 0;
    
    DIR *handle = opendir (self->path);
    if (handle) {
        //  Calculate system-specific size of dirent block
        int dirent_size = offsetof (struct dirent, d_name)
                        + pathconf (self->path, _PC_NAME_MAX) + 1;
        struct dirent *entry = (struct dirent *) malloc (dirent_size);
        struct dirent *result;

        int rc = readdir_r (handle, entry, &result);
        while (rc == 0 && result != NULL) {
            s_posix_populate_entry (self, entry);
            rc = readdir_r (handle, entry, &result);
        }
        free (entry);
        closedir (handle);
    }
#endif
    else {
        free (self->path);
        free (self);
        self = NULL;
    }
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy a directory item

void
fmq_dir_destroy (fmq_dir_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_dir_t *self = *self_p;
        while (zlist_size (self->files)) {
            fmq_file_t *file = (fmq_file_t *) zlist_pop (self->files);
            fmq_file_destroy (&file);
        }
        zlist_destroy (&self->files);
        while (zlist_size (self->subdirs)) {
            fmq_dir_t *subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
            fmq_dir_destroy (&subdir);
        }
        zlist_destroy (&self->subdirs);
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Print contents of directory

void
fmq_dir_dump (fmq_dir_t *self, int indent)
{
    assert (self);
    
    printf ("%*s%s: %td files\n", indent, "", self->path, zlist_size (self->files));
    fmq_dir_t *subdir = (fmq_dir_t *) zlist_first (self->subdirs);
    while (subdir) {
        fmq_dir_dump (subdir, indent + 4);
        subdir = (fmq_dir_t *) zlist_next (self->subdirs);
    }
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_dir_test (bool verbose)
{
    printf (" * fmq_dir: ");

    fmq_dir_t *dir = fmq_dir_new (".", NULL);
    if (verbose)
        fmq_dir_dump (dir, 0);
    fmq_dir_destroy (&dir);

    printf ("OK\n");
    return 0;
}
