/*  =========================================================================
    FmqSasl - work with SASL mechanisms

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

import org.zeromq.ZFrame;

public class FmqSasl
{

    //  --------------------------------------------------------------------------
    //  Encode login and password as PLAIN response
    //                  
    //  Formats a new authentication challenge for a PLAIN login. The protocol
    //  uses a SASL-style binary data block for authentication. This method is
    //  a simple way of turning a name and password into such a block. Note
    //  that plain authentication data is not encrypted and does not provide
    //  any degree of confidentiality. The SASL PLAIN mechanism is defined here:
    //  http://www.ietf.org/internet-drafts/draft-ietf-sasl-plain-08.txt.
    public static ZFrame plainEncode (String login, String password)
    {
        //  PLAIN format is [null]login[null]password
        ZFrame frame = new ZFrame (new byte [login.length () + password.length () + 2]);
        byte [] data = frame.getData ();
        int pos = 0;
        data [pos++] = 0;
        System.arraycopy (login.getBytes (), 0, data, pos, login.length ());
        pos += login.length ();
        data [pos++] = 0;
        System.arraycopy (password.getBytes (), 0, data, pos, password.length ());

        return frame;
    }

    //  --------------------------------------------------------------------------
    //  Decode PLAIN response into new login and password

    public static boolean plainDecode (ZFrame frame, String [] idpw)
    {
        int size = frame.size ();
        byte [] data = frame.getData ();
        if (size < 2 || data [0] != 0 || idpw == null || idpw.length < 2)
            return false;

        int pos = 1;
        for (;pos < size; pos++) 
        {
            if (data [pos] == 0)
                break;
        }
        if (pos < size) {
            byte [] login = new byte [pos - 1];
            byte [] password = new byte [size - pos - 1];
            System.arraycopy (data, 1, login, 0, login.length);
            System.arraycopy (data, pos + 1, password, 0, password.length);

            idpw [0] = new String (login);
            idpw [1] = new String (password);
            
            return true;
        }
        else
            return false;
    }
}
