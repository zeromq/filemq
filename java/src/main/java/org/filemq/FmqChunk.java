/*  =========================================================================
    fmq_chunk - work with memory chunks
    
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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Arrays;

public class FmqChunk
{
    private ByteBuffer data;         //  Data part follows here
    
    public FmqChunk (final byte[] content, int size)
    {
        this.data = ByteBuffer.allocate (size);
        
        if (content != null) {
            data.put (content);
        }
    }

    public void destroy ()
    {
    }

    //  --------------------------------------------------------------------------
    //  Fill chunk data from user-supplied octet
    //  Returns actual size of chunk
    public int fill (byte filler, int size)
    {
        if (size > data.capacity ())
            size = data.capacity ();
        data.clear ();
        Arrays.fill (data.array (), 0, size, filler);
        data.position (size);

        return size;
    }

    //  --------------------------------------------------------------------------
    //  Return chunk cur size
    public int size ()
    {
        return data.position ();
    }

    //  --------------------------------------------------------------------------
    //  Return chunk cur size
    public int maxSize ()
    {
        return data.capacity ();
    }


    //  --------------------------------------------------------------------------
    //  Return chunk data
    public byte [] data ()
    {
        return data.array ();
    }


    //  --------------------------------------------------------------------------
    //  Read chunk from an open file descriptor
    public static FmqChunk read (FileChannel handle, int bytes)
    {
        FmqChunk chunk = new FmqChunk (null, bytes);
        try {
            handle.read (chunk.data);
        } catch (IOException e) {
        }
        
        return chunk;
    }
    //  --------------------------------------------------------------------------
    //  Write chunk to an open file descriptor
    public boolean write (FileChannel handle)
    {
        try {
            data.flip ();
            handle.write (data);
            return !data.hasRemaining ();
        } catch (IOException e) {
            return false;
        }
    }


}
