//  --------------------------------------------------------------------------
//  Selftest

package org.filemq;

import static org.junit.Assert.*;
import org.junit.Test;
import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;
import org.zeromq.ZContext;

public class TestFmqServer
{
    @Test
    public void testFmqServer ()
    {

        System.out.printf (" * fmq_server: ");
        ZContext ctx = new ZContext ();
        
        FmqServer self;
        Socket dealer = ctx.createSocket (ZMQ.DEALER);
        dealer.setReceiveTimeOut (2000);
        dealer.connect ("tcp://localhost:5670");
        
        FmqMsg request, reply;
        
        //  Run selftest using '' configuration
        self = new FmqServer ();
        assert (self != null);
        int port = self.bind ("tcp://*:5670");
        assertEquals (port, 5670);            
        request = new FmqMsg (FmqMsg.OHAI);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.SRSLY);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.ICANHAZ);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.RTFM);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.NOM);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.RTFM);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.HUGZ);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.RTFM);
        reply.destroy ();

        self.destroy ();
        //  Run selftest using 'anonymous.cfg' configuration
        self = new FmqServer ();
        assert (self != null);
        self.configure ("src/test/resources/anonymous.cfg");
        port = self.bind ("tcp://*:5670");
        assertEquals (port, 5670);        
        request = new FmqMsg (FmqMsg.OHAI);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.OHAI_OK);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.NOM);
        request.send (dealer);

        request = new FmqMsg (FmqMsg.HUGZ);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.HUGZ_OK);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.YARLY);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.RTFM);
        reply.destroy ();

        self.destroy ();
        //  Run selftest using 'server_test.cfg' configuration
        self = new FmqServer ();
        assert (self != null);
        self.configure ("src/test/resources/server_test.cfg");
        port = self.bind ("tcp://*:5670");
        assertEquals (port, 5670);        
        request = new FmqMsg (FmqMsg.OHAI);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.ORLY);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.YARLY);
        request.setMechanism ("PLAIN");                              
        request.setResponse (FmqSasl.plainEncode ("guest", "guest"));
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.OHAI_OK);
        reply.destroy ();

        request = new FmqMsg (FmqMsg.NOM);
        request.send (dealer);

        request = new FmqMsg (FmqMsg.HUGZ);
        request.send (dealer);
        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.HUGZ_OK);
        reply.destroy ();

        reply = FmqMsg.recv (dealer);
        assert (reply != null);
        assertEquals (reply.id (), FmqMsg.HUGZ);
        reply.destroy ();

        self.destroy ();

        ctx.destroy ();
        //  No clean way to wait for a background thread to exit
        //  Under valgrind this will randomly show as leakage
        //  Reduce this by giving server thread time to exit
        try {Thread.sleep (200);} catch (Exception e) {};
        System.out.printf ("OK\n");
    }
}
