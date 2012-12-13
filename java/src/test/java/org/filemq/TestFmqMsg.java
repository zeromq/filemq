//  --------------------------------------------------------------------------
//  Selftest

package org.filemq;

import static org.junit.Assert.*;
import org.junit.Test;
import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;
import org.zeromq.ZFrame;
import org.zeromq.ZContext;

public class TestFmqMsg
{
    @Test
    public void testFmqMsg ()
    {
        System.out.printf (" * fmq_msg: ");

        //  Simple create/destroy test
        FmqMsg self = new FmqMsg (0);
        assert (self != null);
        self.destroy ();

        //  Create pair of sockets we can send through
        ZContext ctx = new ZContext ();
        assert (ctx != null);

        Socket output = ctx.createSocket (ZMQ.DEALER);
        assert (output != null);
        output.bind ("inproc://selftest");
        Socket input = ctx.createSocket (ZMQ.ROUTER);
        assert (input != null);
        input.connect ("inproc://selftest");
        
        //  Encode/send/decode and verify each message type

        self = new FmqMsg (FmqMsg.OHAI);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new FmqMsg (FmqMsg.ORLY);
        self.appendMechanisms ("Name: %s", "Brutus");
        self.appendMechanisms ("Age: %d", 43);
        self.setChallenge (new ZFrame ("Captcha Diem"));
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.mechanisms ().size (), 2);
        assertEquals (self.mechanisms ().get (0), "Name: Brutus");
        assertEquals (self.mechanisms ().get (1), "Age: 43");
        assertTrue (self.challenge ().streq ("Captcha Diem"));
        self.destroy ();

        self = new FmqMsg (FmqMsg.YARLY);
        self.setMechanism ("Life is short but Now lasts for ever");
        self.setResponse (new ZFrame ("Captcha Diem"));
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.mechanism (), "Life is short but Now lasts for ever");
        assertTrue (self.response ().streq ("Captcha Diem"));
        self.destroy ();

        self = new FmqMsg (FmqMsg.OHAI_OK);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new FmqMsg (FmqMsg.ICANHAZ);
        self.setPath ("Life is short but Now lasts for ever");
        self.insertOptions ("Name", "Brutus");
        self.insertOptions ("Age", "%d", 43);
        self.insertCache ("Name", "Brutus");
        self.insertCache ("Age", "%d", 43);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.path (), "Life is short but Now lasts for ever");
        assertEquals (self.options ().size (), 2);
        assertEquals (self.optionsString ("Name", "?"), "Brutus");
        assertEquals (self.optionsNumber ("Age", 0), 43);
        assertEquals (self.cache ().size (), 2);
        assertEquals (self.cacheString ("Name", "?"), "Brutus");
        assertEquals (self.cacheNumber ("Age", 0), 43);
        self.destroy ();

        self = new FmqMsg (FmqMsg.ICANHAZ_OK);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new FmqMsg (FmqMsg.NOM);
        self.setCredit ((byte) 123);
        self.setSequence ((byte) 123);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.credit (), 123);
        assertEquals (self.sequence (), 123);
        self.destroy ();

        self = new FmqMsg (FmqMsg.CHEEZBURGER);
        self.setSequence ((byte) 123);
        self.setOperation ((byte) 123);
        self.setFilename ("Life is short but Now lasts for ever");
        self.setOffset ((byte) 123);
        self.setEof ((byte) 123);
        self.insertHeaders ("Name", "Brutus");
        self.insertHeaders ("Age", "%d", 43);
        self.setChunk (new ZFrame ("Captcha Diem"));
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.sequence (), 123);
        assertEquals (self.operation (), 123);
        assertEquals (self.filename (), "Life is short but Now lasts for ever");
        assertEquals (self.offset (), 123);
        assertEquals (self.eof (), 123);
        assertEquals (self.headers ().size (), 2);
        assertEquals (self.headersString ("Name", "?"), "Brutus");
        assertEquals (self.headersNumber ("Age", 0), 43);
        assertTrue (self.chunk ().streq ("Captcha Diem"));
        self.destroy ();

        self = new FmqMsg (FmqMsg.HUGZ);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new FmqMsg (FmqMsg.HUGZ_OK);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new FmqMsg (FmqMsg.KTHXBAI);
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        self.destroy ();

        self = new FmqMsg (FmqMsg.SRSLY);
        self.setReason ("Life is short but Now lasts for ever");
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.reason (), "Life is short but Now lasts for ever");
        self.destroy ();

        self = new FmqMsg (FmqMsg.RTFM);
        self.setReason ("Life is short but Now lasts for ever");
        self.send (output);
    
        self = FmqMsg.recv (input);
        assert (self != null);
        assertEquals (self.reason (), "Life is short but Now lasts for ever");
        self.destroy ();

        ctx.destroy ();
        System.out.printf ("OK\n");
    }
}
