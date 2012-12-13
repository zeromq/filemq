/*  =========================================================================
    filemq - FileMQ command-line service
    
    This version works just like the track command. I'll improve filemq to
    be a more general service over time.

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation <www.imatix.com>
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
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

package org.filemq;

public class Filemq
{
    private static final String PRODUCT  =       "FileMQ service/1.0a1";
    private static final String COPYRIGHT =      "Copyright (c) 2012 iMatix Corporation";
    private static final String NOWARRANTY =
    "This is free software; see the source for copying conditions.  There is NO\n"
    + "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n" ;
    
    public static void main (String[] args) throws Exception
    {
        System.out.println (PRODUCT);
        System.out.println (COPYRIGHT);
        System.out.println (NOWARRANTY);

        if (args.length < 2) {
            System.out.println ("usage: filemq publish-from subscribe-into");
            return;
        }
        FmqServer server = new FmqServer ();
        server.configure ("src/test/resources/anonymous.cfg");
        server.publish (args [0], "/");
        server.setAnonymous (1);
        server.bind ("tcp://*:5670");

        FmqClient client = new FmqClient ();
        client.connect ("tcp://localhost:5670");
        client.setInbox (args [1]);
        client.subscribe ("/");

        while (!Thread.currentThread ().isInterrupted ())
            Thread.sleep (1000);
        System.out.println ("interrupted");

        server.destroy ();
        client.destroy ();
    }

}
