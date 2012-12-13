package org.filemq;

import static org.junit.Assert.*;
import org.junit.Test;

public class TestFmqChunk
{
    @Test
    public void testFmqChunk ()
    {
        System.out.printf (" * fmq_chunk: ");

        FmqChunk chunk = new FmqChunk ("1234567890".getBytes (), 10);
        assertEquals (chunk.size (), 10);
        assertEquals (new String (chunk.data ()), "1234567890");
        chunk.destroy ();

        System.out.printf ("OK\n");
    }
}
