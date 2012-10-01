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
#include "../include/fmq.h"

//  Structure of our class

struct _fmq_dir_t {
    char *path;             //  Directory name + separator
    zlist_t *files;         //  List of files in directory
    zlist_t *subdirs;       //  List of subdirectories
    time_t time;            //  Most recent file including subdirs
    off_t  size;            //  Total file size including subdirs
    size_t count;           //  Total file count including subdirs
};

#if (defined (WIN32))
static void
s_win32_populate_entry (fmq_dir_t *self, WIN32_FIND_DATA *entry)
{
    if (entry->cFileName [0] == '.')
        ; //  Skip hidden files
    else
    //  If we have a subdirectory, go load that
    if (entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        fmq_dir_t *subdir = fmq_dir_new (entry->cFileName, self->path);
        zlist_append (self->subdirs, subdir);
    }
    else {
        //  Add file entry to directory list
        fmq_file_t *file = fmq_file_new (self->path, entry->cFileName);
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
    if (stat (fullpath, &stat_buf))
        return;
    
    if (entry->d_name [0] == '.')
        ; //  Skip hidden files
    else
    //  If we have a subdirectory, go load that
    if (stat_buf.st_mode & S_IFDIR) {
        fmq_dir_t *subdir = fmq_dir_new (entry->d_name, self->path);
        zlist_append (self->subdirs, subdir);
    }
    else {
        //  Add file entry to directory list
        fmq_file_t *file = fmq_file_new (self->path, entry->d_name);
        zlist_append (self->files, file);
    }
}
#endif


//  --------------------------------------------------------------------------
//  Constructor
//  Loads full directory tree

fmq_dir_t *
fmq_dir_new (const char *path, const char *parent)
{
    fmq_dir_t *self = (fmq_dir_t *) zmalloc (sizeof (fmq_dir_t));
    if (parent) {
        self->path = malloc (strlen (path) + strlen (parent) + 2);
        sprintf (self->path, "%s/%s", parent, path);
    }
    else
        self->path = strdup (path);
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
        fmq_dir_destroy (&self);
        return NULL;
    }
    //  Update directory signatures
    fmq_dir_t *subdir = (fmq_dir_t *) zlist_first (self->subdirs);
    while (subdir) {
        if (self->time < subdir->time)
            self->time = subdir->time;
        self->size += subdir->size;
        self->count += subdir->count;
        subdir = (fmq_dir_t *) zlist_next (self->subdirs);
    }
    fmq_file_t *file = (fmq_file_t *) zlist_first (self->files);
    while (file) {
        if (self->time < fmq_file_time (file))
            self->time = fmq_file_time (file);
        self->size += fmq_file_size (file);
        self->count += 1;
        file = (fmq_file_t *) zlist_next (self->files);
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
        while (zlist_size (self->subdirs)) {
            fmq_dir_t *subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
            fmq_dir_destroy (&subdir);
        }
        while (zlist_size (self->files)) {
            fmq_file_t *file = (fmq_file_t *) zlist_pop (self->files);
            fmq_file_destroy (&file);
        }
        zlist_destroy (&self->subdirs);
        zlist_destroy (&self->files);
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Return directory path

char *
fmq_dir_path (fmq_dir_t *self)
{
    return self->path;
}


//  --------------------------------------------------------------------------
//  Return directory time

time_t
fmq_dir_time (fmq_dir_t *self)
{
    assert (self);
    return self->time;
}


//  --------------------------------------------------------------------------
//  Return directory size

off_t
fmq_dir_size (fmq_dir_t *self)
{
    assert (self);
    return self->size;
}


//  --------------------------------------------------------------------------
//  Return directory count

size_t
fmq_dir_count (fmq_dir_t *self)
{
    assert (self);
    return self->count;
}


//  --------------------------------------------------------------------------
//  Calculate differences between two versions of a directory tree
//  Returns a list of fmq_patch_t patches. Either older or newer may
//  be null, indicating the directory is empty/absent.

zlist_t *
fmq_dir_diff (fmq_dir_t *older, fmq_dir_t *newer)
{
    zlist_t *patches = zlist_new ();
    fmq_file_t **old_files = fmq_dir_flatten (older);
    fmq_file_t **new_files = fmq_dir_flatten (newer);

    int old_index = 0;
    int new_index = 0;

    //  Note that both lists are sorted, so detecting differences
    //  is rather trivial
    while (old_files [old_index] || new_files [new_index]) {
        fmq_file_t *old = old_files [old_index];
        fmq_file_t *new = new_files [new_index];

        int cmp;
        if (!old)
            cmp = 1;        //  Old file was deleted at end of list
        else
        if (!new)
            cmp = -1;       //  New file was added at end of list
        else
            cmp = strcmp (fmq_file_name (old), fmq_file_name (new));

        if (cmp > 0) {
            //  New file was created
            if (fmq_file_stable (new))
                zlist_append (patches, fmq_patch_new (new, patch_create, 0));
            old_index--;
        }
        else
        if (cmp < 0) {
            //  Old file was deleted
            if (fmq_file_stable (old))
                zlist_append (patches, fmq_patch_new (old, patch_delete, 0));
            new_index--;
        }
        else
        if (cmp == 0 && fmq_file_stable (new)) {
            if (fmq_file_stable (old)) {
                //  Old file was modified or replaced
                //  Since we don't check file contents, treat as created
                if (fmq_file_time (new) != fmq_file_time (old)
                ||  fmq_file_size (new) != fmq_file_size (old))
                    zlist_append (patches, fmq_patch_new (new, patch_create, 0));
            }
            else
                //  File was created over some period of time
                zlist_append (patches, fmq_patch_new (new, patch_create, 0));
        }
        old_index++;
        new_index++;
    }
    free (old_files);
    free (new_files);
    
    return patches;
}


//  --------------------------------------------------------------------------
//  Return sorted array of file references

//  Compare two subdirs, true if they need swapping
static bool
s_dir_compare (void *item1, void *item2)
{
    if (strcmp (fmq_dir_path ((fmq_dir_t *) item1),
                fmq_dir_path ((fmq_dir_t *) item2)) > 0)
        return true;
    else
        return false;
}

//  Compare two files, true if they need swapping
static bool
s_file_compare (void *item1, void *item2)
{
    if (strcmp (fmq_file_name ((fmq_file_t *) item1),
                fmq_file_name ((fmq_file_t *) item2)) > 0)
        return true;
    else
        return false;
}

static int
s_dir_flatten (fmq_dir_t *self, fmq_file_t **files, int index)
{
    //  First flatten the normal files
    zlist_sort (self->files, s_file_compare);
    fmq_file_t *file = (fmq_file_t *) zlist_first (self->files);
    while (file) {
        files [index++] = file;
        file = (fmq_file_t *) zlist_next (self->files);
    }
    //  Now flatten subdirectories, recursively
    zlist_sort (self->subdirs, s_dir_compare);
    fmq_dir_t *subdir = (fmq_dir_t *) zlist_first (self->subdirs);
    while (subdir) {
        index = s_dir_flatten (subdir, files, index);
        subdir = (fmq_dir_t *) zlist_next (self->subdirs);
    }
    return index;
}

fmq_file_t **
fmq_dir_flatten (fmq_dir_t *self)
{
    int flat_size;
    if (self)
        flat_size = self->count + 1;
    else
        flat_size = 1;      //  Just null terminator

    fmq_file_t **files = (fmq_file_t **) zmalloc (sizeof (fmq_file_t *) * flat_size);
    uint index = 0;
    if (self)
        index = s_dir_flatten (self, files, index);
    return files;
}


//  --------------------------------------------------------------------------
//  Remove directory, optionally including all files

void
fmq_dir_remove (fmq_dir_t *self, bool force)
{
    //  If forced, remove all subdirectories and files
    if (force) {
        fmq_file_t *file = (fmq_file_t *) zlist_pop (self->files);
        while (file) {
            fmq_file_remove (file);
            fmq_file_destroy (&file);
            file = (fmq_file_t *) zlist_pop (self->files);
        }
        fmq_dir_t *subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
        while (subdir) {
            fmq_dir_remove (subdir, force);
            fmq_dir_destroy (&subdir);
            subdir = (fmq_dir_t *) zlist_pop (self->subdirs);
        }
        self->size = 0;
        self->count = 0;
    }
    //  Remove if empty
    if (zlist_size (self->files) == 0
    &&  zlist_size (self->subdirs) == 0)
#if (defined (WIN32))
        RemoveDirectory (self->path);
#else
        rmdir (self->path);
#endif
}


//  --------------------------------------------------------------------------
//  Print contents of directory

void
fmq_dir_dump (fmq_dir_t *self, int indent)
{
    assert (self);
    
    fmq_file_t **files = fmq_dir_flatten (self);
    uint index;
    for (index = 0;; index++) {
        fmq_file_t *file = files [index];
        if (!file)
            break;
        puts (fmq_file_name (file));
    }
    free (files);
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_dir_test (bool verbose)
{
    printf (" * fmq_dir: ");

    fmq_dir_t *older = fmq_dir_new (".", NULL);
    assert (older);
    if (verbose) {
        printf ("\n");
        fmq_dir_dump (older, 0);
    }
    fmq_dir_t *newer = fmq_dir_new ("..", NULL);
    zlist_t *patches = fmq_dir_diff (older, newer);
    assert (patches);
    while (zlist_size (patches)) {
        fmq_patch_t *patch = (fmq_patch_t *) zlist_pop (patches);
        fmq_patch_destroy (&patch);
    }
    zlist_destroy (&patches);
    fmq_dir_destroy (&older);
    fmq_dir_destroy (&newer);

    fmq_dir_t *nosuch = fmq_dir_new ("does-not-exist", NULL);
    assert (nosuch == NULL);

    printf ("OK\n");
    return 0;
}
