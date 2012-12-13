/*  =========================================================================
    FmqDir - work with file-system directories

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

package org.filemq;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class FmqDir
{
    private final File path;            //  Directory name + separator
    private final List <FmqFile> files;     //  List of files in directory
    private final List <FmqDir> subdirs;    //  List of subdirectories
    private long time;                  //  Most recent file including subdirs
    private long  size;                 //  Total file size including subdirs
    private int count;                  //  Total file count including subdirs
    
    //  --------------------------------------------------------------------------
    //  Constructor
    //  Loads full directory tree
    
    public FmqDir (File path)
    {
        assert (path != null);
        assert (path.exists ());

        this.path = path;

        files = new ArrayList <FmqFile> ();
        subdirs = new ArrayList <FmqDir> ();
        count = 0;
        size = time = 0L;
        
        for (File f : path.listFiles ()) {
            if (f.getName ().startsWith ("."))
                continue; //  Skip hidden files
            if (f.isDirectory ())
                subdirs.add (new FmqDir (f));
            else
                files.add (new FmqFile (f));
        }
        
        //  Update directory signatures
        for (FmqDir subdir : subdirs) {
            if (time < subdir.time)
                time = subdir.time;
            size += subdir.size;
            count += subdir.count;
        }
        for (FmqFile file : files) {
            if (time < file.time ())
                time = file.time ();
            size += file.size ();
            count ++;
        }
    }

    public static FmqDir newFmqDir (final String path, final String parent)
    {
        File dir;
        if (parent != null)
            dir = new File (parent, path);
        else
            dir = new File (path);

        if (!dir.exists ())
            return null;
        
        return new FmqDir (dir);
    }

    //  --------------------------------------------------------------------------
    //  Destroy a directory item
    public void destroy ()
    {
        for (FmqDir dir : subdirs) {
            dir.destroy ();
        }
        for (FmqFile file : files) {
            file.destroy ();
        }
    }
    
    //  --------------------------------------------------------------------------
    //  Return directory path
    
    public String path ()
    {
        return path.getAbsolutePath ();
    }
    
    //  --------------------------------------------------------------------------
    //  Return directory time
    public long time ()
    {
        return time;
    }
    
    //  --------------------------------------------------------------------------
    //  Return directory size
    public long size ()
    {
        return size;
    }
    
    //  --------------------------------------------------------------------------
    //  Return sorted array of file references
    
    //  Compare two subdirs, true if they need swapping
    private static Comparator <FmqDir> compareDir = new Comparator <FmqDir> () {

        @Override
        public int compare (FmqDir arg0, FmqDir arg1)
        {
            return arg0.path ().compareTo (arg1.path ());
        }
    };
    
    //  Compare two files, true if they need swapping
    //  We sort by ascending name
    private static Comparator <FmqFile> compareFile = new Comparator <FmqFile> () {

        @Override
        public int compare (FmqFile arg0, FmqFile arg1)
        {
            return arg0.name (null).compareTo (arg1.name (null));
        }
    };
    
    private static int flattenDir (FmqDir self, FmqFile [] files, int index)
    {
        //  First flatten the normal files
        Collections.sort (self.files, compareFile);
        for (FmqFile file : self.files)
            files [index++] = file;
        
        //  Now flatten subdirectories, recursively
        Collections.sort (self.subdirs, compareDir);
        for (FmqDir subdir : self.subdirs)
            index = flattenDir (subdir, files, index);
        
        return index;
    }

    //  --------------------------------------------------------------------------
    //  Return sorted array of file references
    
    public static FmqFile [] flatten (FmqDir self)
    {
        FmqFile [] files = new FmqFile [self != null ? self.count + 1 : 1];
        int index = 0;
        if (self != null)
            index = flattenDir (self, files, index);
        return files;
    }
    
    //  --------------------------------------------------------------------------
    //  Remove directory, optionally including all files
    public void remove (boolean force)
    {
    //  If forced, remove all subdirectories and files
        if (force) {
            for (FmqFile file : files) {
                file.remove ();
                file.destroy ();
            }
            for (FmqDir dir : subdirs) {
                dir.remove (force);
                dir.destroy ();
            }
            size = 0;
            count = 0;
        }
        //  Remove if empty
        if (files.size () == 0 && subdirs.size () == 0)
            path.delete ();
    }
    
    //  --------------------------------------------------------------------------
    //  Print contents of directory
    public void dump (int indent)
    {
        FmqFile [] files = flatten (this);
        for (FmqFile file : files) {
            if (file == null)
                break;
            System.out.println (file.name (null));
        }
    }
    
    //  --------------------------------------------------------------------------
    //  Load directory cache; returns a hash table containing the SHA-1 digests
    //  of every file in the tree. The cache is saved between runs in .cache.
    //  The caller must destroy the hash table when done with it.
    public Map <String, String> cache ()
    {
        //  Load any previous cache from disk
        Map <String, String> cache = new HashMap <String, String> ();
        File cache_file = new File (path, ".cache" );
        
        //  Load
        if (cache_file.exists ()) {
            String line;
            BufferedReader in = null;
            
            try {
                in = new BufferedReader (new FileReader (cache_file));
                while ((line = in.readLine ()) != null) {
                    String [] kv = line.trim ().split ("=", 2);
                    cache.put (kv [0], kv [1]);
                }
            } catch (IOException e) {
            } finally {
                if (in != null)
                    try {
                        in.close ();
                    } catch (IOException e) {
                    }
            }
        }

        //  Recalculate digest for any new files
        FmqFile [] files = flatten (this);
        for (FmqFile file : files) {
            if (file == null)
                break;
            String filename = file.name (path ());
            if (!cache.containsKey (filename))
                cache.put (filename, file.hash ());
        }

        //  Save cache to disk for future reference
        BufferedWriter out = null;
        try {
            out = new BufferedWriter (new FileWriter (cache_file));
            for (Map.Entry <String, String> entry : cache.entrySet ()) {
                out.write (String.format ("%s=%s\n", entry.getKey (), entry.getValue ()));
            }
        } catch (IOException e) {
        } finally {
            if (out != null)
                try {
                    out.close ();
                } catch (IOException e) {
                }
        }
        return cache;
    }

    //  --------------------------------------------------------------------------
    //  Calculate differences between two versions of a directory tree
    //  Returns a list of fmq_patch_t patches. Either older or newer may
    //  be null, indicating the directory is empty/absent. If alias is set,
    //  generates virtual filename (minus path, plus alias).
    public static List<FmqPatch> diff (FmqDir older, FmqDir newer, String alias)
    {
        ArrayList <FmqPatch> patches = new ArrayList <FmqPatch> ();
        FmqFile [] old_files = flatten (older);
        FmqFile [] new_files = flatten (newer);

        int old_index = 0;
        int new_index = 0;

        //  Note that both lists are sorted, so detecting differences
        //  is rather trivial
        while (old_files [old_index] != null || new_files [new_index] != null) {
            FmqFile old = old_files [old_index];
            FmqFile new_ = new_files [new_index];

            int cmp;
            
            if (old == null)
                cmp = 1;        //  Old file was deleted at end of list
            else
            if (new_ == null)
                cmp = -1;       //  New file was added at end of list
            else
                cmp = compareFile.compare (old, new_);
            
            if (cmp > 0) {
                //  New file was created
                if (new_.stable ())
                    patches.add (new FmqPatch (newer.path, new_, FmqPatch.OP.patch_create, alias));
                old_index--;
            }
            else
            if (cmp < 0) {
                //  Old file was deleted
                if (old.stable ())
                    patches.add (new FmqPatch (older.path, old, FmqPatch.OP.patch_delete, alias));
                new_index--;
            }
            else
            if (cmp == 0 && new_.stable ()) {
                if (old.stable ()) {
                    //  Old file was modified or replaced
                    //  Since we don't check file contents, treat as created
                    //  Could better do SHA check on file here
                    if (new_.time () != old.time ()
                    ||  new_.size () != old.size ())
                        patches.add (new FmqPatch (newer.path, new_, FmqPatch.OP.patch_create, alias));
                }
                else
                    //  File was created over some period of time
                    patches.add (new FmqPatch (newer.path, new_, FmqPatch.OP.patch_create, alias));
            }
            old_index++;
            new_index++;
        }

        return patches;
    }

    //  --------------------------------------------------------------------------
    //  Return full contents of directory as a patch list. If alias is set,
    //  generates virtual filename (minus path, plus alias).
    public static List<FmqPatch> resync (FmqDir self, String alias)
    {
        List <FmqPatch> patches = new ArrayList <FmqPatch> ();
        FmqFile [] files = flatten (self);
        for (FmqFile file : files) {
            if (file == null)
                break;
            patches.add (new FmqPatch (self.path, file, FmqPatch.OP.patch_create, alias));
        }
        return patches;

    }

}
