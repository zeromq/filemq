/*  =========================================================================
    fmq_hash - provides hashing functions (SHA-1 at present)

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

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class FmqHash
{
    private MessageDigest digest;
    
    //  --------------------------------------------------------------------------
    //  Constructor
    //  Create new SHA object
    public FmqHash ()
    {
        try {
            digest = MessageDigest.getInstance ("SHA-1");
        }
        catch(NoSuchAlgorithmException e) {
            digest = null;
        } 
    }
    
    //  --------------------------------------------------------------------------
    //  Destroy a SHA object
    public void destroy ()
    {
    }
    
    //  --------------------------------------------------------------------------
    //  Add buffer into SHA calculation
    public void update (byte [] data, int size)
    {
        digest.update (data, 0, size);
    }

    //  --------------------------------------------------------------------------
    //  Return final SHA hash data
    public byte [] data ()
    {
        return digest.digest ();
    }

    //  --------------------------------------------------------------------------
    //  Return final SHA hash size
    public int size ()
    {
        return digest.getDigestLength ();
    }


}
