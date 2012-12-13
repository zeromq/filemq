package org.filemq;

import static org.junit.Assert.*;
import org.junit.Test;
import org.zeromq.ZFrame;

public class TestFmqSasl
{
    @Test
    public void testFmqSasl ()
    {
        System.out.printf (" * fmq_sasl: ");

        ZFrame frame = FmqSasl.plainEncode ("Happy", "Harry");
        String [] result = new String [2];
        String login, password;
        boolean rc = FmqSasl.plainDecode (frame, result);
        login = result [0];
        password = result [1];
        
        assertTrue (rc);
        assertEquals (login, "Happy");
        assertEquals (password, "Harry");
        frame.destroy ();

        System.out.printf ("OK\n");
    }

}
