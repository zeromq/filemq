package org.filemq;

import static org.junit.Assert.*;

import java.nio.ByteBuffer;

import org.junit.Test;

public class TestFmqFile
{
    @Test
    public void testFmqFile () throws Exception
    {
        System.out.printf (" * fmq_file: ");

        FmqFile file = new FmqFile (".", "bilbo");
        assertEquals (file.name ("."), "bilbo");
        assertEquals (file.exists (), false);
        file.destroy ();

        //  Create a test file in some random subdirectory
        file = new FmqFile ("./this/is/a/test", "bilbo");
        boolean rc = file.output ();
        assertEquals (rc, true);
        FmqChunk chunk = new FmqChunk (null, 100);
        chunk.fill ((byte) 0, 100);
        //  Write 100 bytes at position 1,000,000 in the file
        rc = file.write (chunk, 1000000);
        assertEquals (rc, true);
        file.close ();
        assertEquals (file.exists (), true);
        assertEquals (file.size (), 1000100);
        assertEquals (file.stable (), false);
        chunk.destroy ();
        Thread.sleep (1001);
        file.restat ();
        assertEquals (file.stable (), true);

        //  Check we can read from file
        rc = file.input ();
        assertEquals (rc, true);
        chunk = file.read (1000100, 0);
        assertNotNull (chunk);
        
        assertEquals (chunk.size (), 1000100);
        chunk.destroy ();

        //  Try some fun with symbolic links
        FmqFile link = new FmqFile ("./this/is/a/test", "bilbo.ln");
        rc = link.output ();
        assertEquals (rc, true);
        link.handle ().write (ByteBuffer.wrap ("./this/is/a/test/bilbo\n".getBytes ()));
        link.destroy ();

        link = new FmqFile ("./this/is/a/test", "bilbo.ln");
        rc = link.input ();
        assertEquals (rc, true);
        chunk = file.read (1000100, 0);
        assertNotNull (chunk);
        assertEquals (chunk.size (), 1000100);
        chunk.destroy ();
        link.destroy ();

        //  Remove file and directory
        FmqDir dir = FmqDir.newFmqDir ("./this", null);
        assertEquals (dir.size (), 2000200);
        dir.remove (true);
        assertEquals (dir.size (), 0);
        dir.destroy ();

        //  Check we can no longer read from file
        assertEquals (file.exists (), true);
        file.restat ();
        assertEquals (file.exists (), false);
        rc = file.input ();
        assertEquals (rc, false);
        file.destroy ();

        System.out.printf ("OK\n");
    }
}
