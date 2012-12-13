/*  =========================================================================
    TestFmqClient.java

    Generated class for FmqClient protocol client
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

//  --------------------------------------------------------------------------
//  Selftest

package org.filemq;

import org.junit.Test;

public class TestFmqClient
{
    @Test
    public void testFmqClient ()
    {
        System.out.printf (" * FmqClient: ");

        FmqClient self;
        //  Run selftest using 'client_test.cfg' configuration
        self = new FmqClient ();
        assert (self != null);
        self.configure ("src/test/resources/client_test.cfg");
        self.connect ("tcp://localhost:6001");             
        try { Thread.sleep (1000); } catch (Exception e) {}
        self.destroy ();

        System.out.printf ("OK\n");
    }
}
