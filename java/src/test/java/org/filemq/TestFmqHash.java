package org.filemq;

import java.util.Arrays;

import org.junit.Test;
import static org.junit.Assert.*;

public class TestFmqHash
{
    @Test
    public void testFmqHash ()
    {
        System.out.printf (" * fmq_hash: ");

        byte [] buffer = new byte [1024];
        Arrays.fill (buffer, (byte) 0xAA);

        FmqHash hash = new FmqHash ();
        hash.update (buffer, 1024);
        byte [] data = hash.data ();
        int size = hash.size ();
        assertEquals (size, 20);
        assertEquals (data [0], (byte) 0xDE);
        assertEquals (data [1], (byte) 0xB2);
        assertEquals (data [2], (byte) 0x38);
        assertEquals (data [3], (byte) 0x07);
        hash.destroy ();

        System.out.printf ("OK\n");
    }
}
