/*  =========================================================================
    FmqFile - work with files

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
package org.filemq;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

public class FmqFile
{
    private File path;
    private String name;             //  File name with path
    private String link;             //  Optional linked file

    //  Properties from file system
    private long time;            //  Modification time
    private long size;            //  Size of the file
    //mode_t mode;            //  POSIX permission bits

    //  Other properties
    private boolean exists;            //  true if file exists
    private boolean stable;            //  true if file is stable
    private boolean eof;               //  true if at end of file
    private FileChannel handle;        //  Read/write handle
    
    public FmqFile (final String parent, final String name)
    {
        this (new File (parent, name));
    }

    public FmqFile (final File parent, final String name)
    {
        this (new File (parent, name));
    }
    
    public FmqFile (File path)
    {
        this.path = path;
        name = path.getAbsolutePath ();
        
        if (path.exists () && 
                path.getName ().endsWith (".ln")) {
            BufferedReader in = null;
            try {
                in = new BufferedReader (new FileReader (path));
                String buffer = in.readLine ();
                link = buffer.trim ();
                if (link == null) {
                    //  There could be a race condition or corrupted, try once more
                    in.close ();
                    in = new BufferedReader (new FileReader (path));
                    buffer = in.readLine ();
                    link = buffer.trim ();
                }
                if (link == null) {
                    //  Guess it is corrupted
                    in.close ();
                    in = null;
                    path.delete ();
                } else {
                    this.path = new File (link);
                    name = name.substring (0, name.length () -3);
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
        restat ();
    }

    //  --------------------------------------------------------------------------
    //  Destroy a file item
    public void destroy ()
    {
        if (handle != null)
            try {
                handle.close ();
            } catch (IOException e) {
            }
    }

    //  --------------------------------------------------------------------------
    //  Duplicate a file item
    public FmqFile dup ()
    {
        FmqFile copy = new FmqFile (path);
        return copy;
    }


    //  --------------------------------------------------------------------------
    //  Return file name, remove prefix if provided
    public String name (String prefix)
    {
        if (prefix == null)
            return name;
        
        String parent = new File (prefix).getAbsolutePath ();
        if (name.startsWith (parent)) {
            String result = name.substring (parent.length ());
            if (result.startsWith ("/"))
                result = result.substring (1);
            return result;
        }
        
        return name;
    }

    //  --------------------------------------------------------------------------
    //  Refreshes file properties from file system
    public void restat ()
    {
        if (path.exists ()) {
            //  Not sure if stat mtime is fully portable
            exists = true;
            size = path.length ();
            time = path.lastModified ();
            //  File is 'stable' if more than 1 second old
            long age = System.currentTimeMillis () - time;
            stable = age > 1000;
        }
        else
            exists = false;        
    }
    
    //  --------------------------------------------------------------------------
    //  Check if file exists/ed; does not restat file
    public boolean exists () 
    {
        return exists;
    }
    
    //  --------------------------------------------------------------------------
    //  Check if file is/was stable; does not restat file
    public boolean stable ()
    {
        return stable;
    }
    
    //  --------------------------------------------------------------------------
    //  Remove the file
    public void remove ()
    {
        if (link != null)
            new File (name + ".ln").delete ();
        else
            path.delete ();
    }
    
    //  --------------------------------------------------------------------------
    //  Open file for reading
    //  Returns 0 if OK, -1 if not found or not accessible
    //  If file is symlink, opens real physical file, not link
    public boolean input ()
    {
        if (handle != null)
            close ();

        try {
            handle = new RandomAccessFile (path, "r").getChannel ();
            size = path.length ();
        } catch (FileNotFoundException e) {
            size = 0;
            return false;
        }
        return true;
    }
    
    //  --------------------------------------------------------------------------
    //  Open file for writing, creating directory if needed
    //  File is created if necessary; chunks can be written to file at any
    //  location. Returns 0 if OK, -1 if error.
    //  If file was symbolic link, that's over-written
    public boolean output ()
    {
        //  Wipe symbolic link if that's what the file was
        if (link != null) {
            link = null;
        }
        path.getParentFile ().mkdirs ();
        if (handle != null)
            close ();

        //  Create file if it doesn't exist
        try {
            handle = new RandomAccessFile (path, "rw").getChannel ();
        } catch (FileNotFoundException e) {
            return false;
        }
        return true;
    }
    
    //  --------------------------------------------------------------------------
    //  Read chunk from file at specified position
    //  If this was the last chunk, sets self->eof
    //  Null chunk means there was another error
    public FmqChunk read (int bytes, long offset)
    {
        assert (handle != null);
        //  Calculate real number of bytes to read
        if (offset > size)
            bytes = 0;
        else
        if (bytes > size - offset)
            bytes = (int) (size - offset);

        try {
            handle.position (offset);
        } catch (IOException e) {
            return null;
        }

        eof = false;
        FmqChunk chunk = FmqChunk.read (handle, bytes);
        if (chunk != null)
            eof = chunk.size () < bytes;
        return chunk;
    }

    //  --------------------------------------------------------------------------
    //  Write chunk to file at specified position
    //  Return 0 if OK, else -1
    public boolean write (FmqChunk chunk, long offset)
    {
        assert (handle != null);
        try {
            handle.position (offset);
        } catch (IOException e) {
            return false;
        }
        return chunk.write (handle);
    }
    
    //  --------------------------------------------------------------------------
    //  Write string to file at specified position
    //  Return 0 if OK, else -1
    public boolean write (String data, long offset)
    {
        assert (handle != null);
        try {
            handle.position (offset);
            handle.write (ByteBuffer.wrap (data.getBytes ()));
            return true;
        } catch (IOException e) {
            return false;
        }
    }
    
    //  --------------------------------------------------------------------------
    //  Close file, if open
    public void close ()
    {
        if (handle != null) {
            try {
                handle.close ();
            } catch (IOException e) {
            }
            restat ();
        }
        handle = null;
    }
    
    //  --------------------------------------------------------------------------
    //  Return file handle, if opened
    public FileChannel handle ()
    {
        return handle;
    }
    
    //  --------------------------------------------------------------------------
    //  Return file SHA-1 hash as string;
    public String hash ()
    {
        boolean rc = input ();
        if (!rc)
            return null;            //  Problem reading directory

        //  Now calculate hash for file data, chunk by chunk
        FmqHash hash = new FmqHash ();
        int blocksz = 65535;

        FmqChunk chunk = FmqChunk.read (handle, blocksz);
        while (chunk.size () > 0) {
            hash.update (chunk.data (), chunk.size ());
            chunk.destroy ();
            chunk = FmqChunk.read (handle, blocksz);
        }
        chunk.destroy ();
        close ();

        //  Convert to printable string
        byte [] hex_char = "0123456789ABCDEF".getBytes ();
        byte [] hashstr = new byte [hash.size () * 2 + 1];
        byte [] data = hash.data ();
        int byte_nbr;
        for (byte_nbr = 0; byte_nbr < hash.size (); byte_nbr++) {
            hashstr [byte_nbr * 2 + 0] = hex_char [(0xff & data [byte_nbr]) >> 4];
            hashstr [byte_nbr * 2 + 1] = hex_char [data [byte_nbr] & 15];
        }
        hash.destroy ();
        return new String (hashstr);
    }

    
    public long time ()
    {
        return time;
    }

    public long size ()
    {
        return size;
    }


}
