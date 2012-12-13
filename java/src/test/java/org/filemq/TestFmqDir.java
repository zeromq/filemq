package org.filemq;

import static org.junit.Assert.*;
import java.util.List;
import java.util.Map;

import org.junit.Test;

public class TestFmqDir
{
    @Test
    public void testFmqDir ()
    {
        boolean verbose = false;
        System.out.printf (" * fmq_dir: ");

        FmqDir older = FmqDir.newFmqDir (".", null);
        assertNotNull (older);
        if (verbose) {
            System.out.printf ("\n");
            older.dump (0);
        }
        FmqDir newer = FmqDir.newFmqDir ("..", null);
        List <FmqPatch> patches = FmqDir.diff (older, newer, "/");
        assertNotNull (patches);
        for (FmqPatch patch : patches) {
            patch.destroy ();
        }
        older.destroy ();
        newer.destroy ();

        //  Test directory cache calculation
        FmqDir here = FmqDir.newFmqDir (".", null);
        Map <String, String> cache = here.cache ();
        assertNotNull (cache);
        here.destroy ();

        FmqDir nosuch = FmqDir.newFmqDir ("does-not-exist", null);
        assertNull (nosuch);

        System.out.printf ("OK\n");
    }
}
