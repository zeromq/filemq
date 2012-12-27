/*  =========================================================================
    FmqClient.java

    Generated header for FmqClient protocol client
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

import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.Map;

import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;
import org.zeromq.ZMQ.Poller;
import org.zeromq.ZContext;
import org.zeromq.ZThread;
import org.zeromq.ZMsg;
import org.zeromq.ZFrame;

//  ---------------------------------------------------------------------
//  Structure of our front-end API class

public class FmqClient {
    ZContext ctx;        //  CZMQ context
    Socket pipe;         //  Pipe through to client

    //  The client runs as a background thread so that we can run multiple
    //  engines at once. The API talks to the client thread over an inproc
    //  pipe.


    //  --------------------------------------------------------------------------
    //  Create a new FmqClient and a new client instance

    public FmqClient ()
    {
        ctx = new ZContext ();
        pipe = ZThread.fork (ctx, new ClientThread ());
    }


    //  --------------------------------------------------------------------------
    //  Destroy the FmqClient and stop the client

    public void destroy ()
    {
        pipe.send ("STOP");
        pipe.recvStr ();
        ctx.destroy ();
    }


    //  --------------------------------------------------------------------------
    //  Load client configuration data

    public void configure (final String configFile)
    {
        pipe.sendMore ("CONFIG");
        pipe.send (configFile);
    }


    //  --------------------------------------------------------------------------
    //  Set one configuration key value

    public void setoption (final String path, final String value)
    {
        pipe.sendMore ("SETOPTION");
        pipe.sendMore (path);
        pipe.send (value);
    }


    //  --------------------------------------------------------------------------
    //  Open connection to server

    public void connect (final String endpoint)
    {
        pipe.sendMore ("CONNECT");
        pipe.send (endpoint);
    }


    //  --------------------------------------------------------------------------
    //  Wait for message from API

    public ZMsg recv ()
    {
        return ZMsg.recvMsg (pipe);
    }


    //  --------------------------------------------------------------------------
    //  Return API pipe handle for polling

    public Socket handle ()
    {
        return pipe;
    }


    //  --------------------------------------------------------------------------

    public void subscribe (final String path)
    {
        assert (path != null);
        pipe.sendMore ("SUBSCRIBE");
        pipe.send (String.format("%s", path));
    }


    //  --------------------------------------------------------------------------

    public void setInbox (final String path)
    {
        assert (path != null);
        pipe.sendMore ("SET INBOX");
        pipe.send (String.format("%s", path));
    }


    //  --------------------------------------------------------------------------

    public void setResync (long enabled)
    {
        pipe.sendMore ("SET RESYNC");
        pipe.send (String.format("%d", enabled));
    }


//  ---------------------------------------------------------------------
//  State machine constants

    private enum State {
        start_state (1),
        requesting_access_state (2),
        subscribing_state (3),
        ready_state (4);

        @SuppressWarnings ("unused")
        private final int state;
        State (int state) 
        {
            this.state = state;
        }
    };

    private enum Event {
        terminate_event (-1),
        ready_event (1),
        srsly_event (2),
        rtfm_event (3),
        _other_event (4),
        orly_event (5),
        ohai_ok_event (6),
        ok_event (7),
        finished_event (8),
        cheezburger_event (9),
        hugz_event (10),
        subscribe_event (11),
        send_credit_event (12),
        icanhaz_ok_event (13);

        @SuppressWarnings ("unused")
        private final int event;
        Event (int event) 
        {
            this.event = event;
        }
    };

  
    //  There's no point making these configurable                   
    private static final int CREDIT_SLICE   = 1000000;               
    private static final int CREDIT_MINIMUM = (CREDIT_SLICE * 4) + 1;

    //  Subscription in memory                                       
    private static class Sub {                                       
        private Client client;           //  Pointer to parent client
        private String inbox;            //  Inbox location          
        private String path;             //  Path we subscribe to    
                                                                     
        private Sub (Client client, String inbox, String path)       
        {                                                            
            this.client = client;                                    
            this.inbox = inbox;                                      
            this.path = path;                                        
        }                                                            
                                                                     
        private void destroy ()                                      
        {                                                            
        }                                                            
                                                                     
        //  Return new cache object for subscription path            
                                                                     
        private Map <String, String> cache ()                        
        {                                                            
            //  Get directory cache for this path                    
            FmqDir dir = FmqDir.newFmqDir (path.substring(1), inbox);
            if (dir != null) {                                       
                Map <String, String> cache = dir.cache ();           
                dir.destroy ();                                      
                return cache;                                        
            }                                                        
            return null;                                             
        }                                                            
    }                                                                


    //  ---------------------------------------------------------------------
    //  Context for the client thread

    private static class Client {
        //  Properties accessible to client actions
        private Event next_event;           //  Next event

        private boolean connected;          //  Are we connected to server? 
        private List <Sub> subs;      //  Subscriptions                     
        private Sub sub;                    //  Subscription we want to send
        private int credit;                 //  Current credit pending      
        private FmqFile file;               //  File we're writing to       
        private Iterator <Sub> subIterator;                                 
        //  Properties you should NOT touch
        private ZContext ctx;               //  Own CZMQ context
        private Socket pipe;                //  Socket to back to caller
        private Socket dealer;              //  Socket to talk to server
        private boolean stopped;            //  Has client stopped?
        private FmqConfig config;           //  Configuration tree
        private State state;                //  Current state
        private Event event;                //  Current event
        private FmqMsg request;             //  Next message to send
        private FmqMsg reply;               //  Last received reply
        private int heartbeat;              //  Heartbeat interval
        private long expires_at;            //  Server expires at

        private void config ()
        {
            //  Get standard client configuration
            heartbeat = Integer.parseInt (
                config.resolve ("client/heartbeat", "1")) * 1000;
        }
        private Client (ZContext ctx, Socket pipe)
        {
            this.ctx = ctx;
            this.pipe = pipe;
            this.config = new FmqConfig ("root", null);
            config ();

            subs = new ArrayList <Sub> ();
            connected = false;            
        }
        private void destroy ()
        {
            if (config != null)
                config.destroy ();
            if (request != null)
                request.destroy ();
            if (reply != null)
                reply.destroy ();
            for (Sub sub: subs)
                sub.destroy ();
        }

        //  Apply configuration tree:
        //   * apply client configuration
        //   * print any echo items in top-level sections
        //   * apply sections that match methods

        private void applyConfig ()
        {
            //  Apply echo commands and class methods
            FmqConfig section = config.child ();
            while (section != null) {
                FmqConfig entry = section.child ();
                while (entry != null) {
                    if (entry.name ().equals ("echo"))
                        zclock_log (entry.value ());
                    entry = entry.next ();
                }
                if (section.name ().equals ("subscribe")) {
                    String path = section.resolve ("path", "?");
                    //  Store subscription along with any previous ones         
                    //  Check we don't already have a subscription for this path
                    for (Sub sub: subs) {                                       
                        if (path.equals (sub.path))                             
                            return;                                             
                    }                                                           
                    //  Subscription path must start with '/'                   
                    //  We'll do better error handling later                    
                    assert (path.startsWith ("/"));                             
                                                                                
                    //  New subscription, store it for later replay             
                    String inbox = config.resolve ("client/inbox", ".inbox");   
                    sub = new Sub (this, inbox, path);                          
                    subs.add (sub);                                             
                                                                                
                    //  If we're connected, then also send to server            
                    if (connected)                                              
                        next_event = Event.subscribe_event;                     
                }
                else
                if (section.name ().equals ("set_inbox")) {
                    String path = section.resolve ("path", "?");
                    config.setPath ("client/inbox", path);
                }
                else
                if (section.name ().equals ("set_resync")) {
                    long enabled = Long.parseLong (section.resolve ("enabled", ""));
                    //  Request resynchronization from server                
                    config.setPath ("client/resync", enabled > 0 ? "1" :"0");
                }
                section = section.next ();
            }
            config ();
        }

        //  Custom actions for state machine

        private void initializeTheClient ()
        {
            next_event = Event.ready_event;
        }

        private void trySecurityMechanism ()
        {
            String login = config.resolve ("security/plain/login", "guest"); 
            String password = config.resolve ("security/plain/password", "");
            ZFrame frame = FmqSasl.plainEncode (login, password);            
            request.setMechanism ("PLAIN");                                  
            request.setResponse (frame);                                     
        }

        private void connectedToServer ()
        {
            connected = true;
        }

        private void getFirstSubscription ()
        {
            subIterator = subs.iterator ();       
            if (subIterator.hasNext ()) {         
                sub = subIterator.next ();        
                next_event = Event.ok_event;      
            } else                                
                next_event = Event.finished_event;
        }

        private void getNextSubscription ()
        {
            if (subIterator.hasNext ()) {         
                sub = subIterator.next ();        
                next_event = Event.ok_event;      
            } else                                
                next_event = Event.finished_event;
        }

        private void formatIcanhazCommand ()
        {
            request.setPath (sub.path);                                         
            //  If client app wants full resync, send cache to server           
            if (Integer.parseInt (config.resolve ("client/resync", "0")) == 1) {
                request.insertOptions ("RESYNC", "1");                          
                request.setCache (sub.cache ());                                
            }                                                                   
        }

        private void refillCreditAsNeeded ()
        {
            //  If credit has fallen too low, send more credit
            int credit_to_send = 0;                           
            while (credit < CREDIT_MINIMUM) {                 
                credit_to_send += CREDIT_SLICE;               
                credit += CREDIT_SLICE;                       
            }                                                 
            if (credit_to_send > 0) {                         
                request.setCredit (credit_to_send);           
                next_event = Event.send_credit_event;         
            }                                                 
        }

        private void processThePatch ()
        {
            String inbox = config.resolve ("client/inbox", ".inbox");               
            String filename = reply.filename ();                                    
                                                                                    
            //  Filenames from server must start with slash, which we skip          
            assert (filename.startsWith ("/"));                                     
            filename = filename.substring (1);                                      
                                                                                    
            if (reply.operation () == FmqMsg.FMQ_MSG_FILE_CREATE) {                 
                if (file == null) {                                                 
                    file = new FmqFile (inbox, filename);                           
                    if (!file.output ()) {                                          
                        //  File not writeable, skip patch                          
                        file.destroy ();                                            
                        file = null;                                                
                        return;                                                     
                    }                                                               
                }                                                                   
                //  Try to write, ignore errors in this version                     
                ZFrame frame = reply.chunk ();                                      
                FmqChunk chunk = new FmqChunk (frame.getData (), frame.size ());    
                if (chunk.size () > 0) {                                            
                    file.write (chunk, reply.offset ());                            
                    credit -= chunk.size ();                                        
                }                                                                   
                else {                                                              
                    //  Zero-sized chunk means end of file, so report back to caller
                    pipe.sendMore ("DELIVER");                                      
                    pipe.sendMore (filename);                                       
                    pipe.send (String.format ("%s/%s", inbox, filename));           
                    file.destroy ();                                                
                    file = null;                                                    
                }                                                                   
                chunk.destroy ();                                                   
            }                                                                       
            else                                                                    
            if (reply.operation () == FmqMsg.FMQ_MSG_FILE_DELETE) {                 
                zclock_log ("I: delete %s/%s", inbox, filename);                    
                FmqFile file = new FmqFile (inbox, filename);                       
                file.remove ();                                                     
                file.destroy ();                                                    
                file = null;                                                        
            }                                                                       
        }

        private void logAccessDenied ()
        {
            System.out.println ("W: server denied us access, retrying...");
        }

        private void logInvalidMessage ()
        {
            System.out.println ("E: server claims we sent an invalid message");
        }

        private void logProtocolError ()
        {
            System.out.println ("E: protocol error");
        }

        private void terminateTheClient ()
        {
            connected = false;                 
            next_event = Event.terminate_event;
        }

        //  Execute state machine as long as we have events

        private void execute (Event event)
        {
            next_event = event;
            while (next_event != null) {
                event = next_event;
                next_event = null;
                switch (state) {
                case start_state:
                    if (event == Event.ready_event) {
                        request.setId (FmqMsg.OHAI);
                        request.send (dealer);
                        request = new FmqMsg (0);
                        state = State.requesting_access_state;
                    }
                    else
                    if (event == Event.srsly_event) {
                        logAccessDenied ();
                        terminateTheClient ();
                        state = State.start_state;
                    }
                    else
                    if (event == Event.rtfm_event) {
                        logInvalidMessage ();
                        terminateTheClient ();
                    }
                    else {
                        //  Process all other events
                        logProtocolError ();
                        terminateTheClient ();
                    }
                    break;

                case requesting_access_state:
                    if (event == Event.orly_event) {
                        trySecurityMechanism ();
                        request.setId (FmqMsg.YARLY);
                        request.send (dealer);
                        request = new FmqMsg (0);
                        state = State.requesting_access_state;
                    }
                    else
                    if (event == Event.ohai_ok_event) {
                        connectedToServer ();
                        getFirstSubscription ();
                        state = State.subscribing_state;
                    }
                    else
                    if (event == Event.srsly_event) {
                        logAccessDenied ();
                        terminateTheClient ();
                        state = State.start_state;
                    }
                    else
                    if (event == Event.rtfm_event) {
                        logInvalidMessage ();
                        terminateTheClient ();
                    }
                    else {
                        //  Process all other events
                    }
                    break;

                case subscribing_state:
                    if (event == Event.ok_event) {
                        formatIcanhazCommand ();
                        request.setId (FmqMsg.ICANHAZ);
                        request.send (dealer);
                        request = new FmqMsg (0);
                        getNextSubscription ();
                        state = State.subscribing_state;
                    }
                    else
                    if (event == Event.finished_event) {
                        refillCreditAsNeeded ();
                        state = State.ready_state;
                    }
                    else
                    if (event == Event.srsly_event) {
                        logAccessDenied ();
                        terminateTheClient ();
                        state = State.start_state;
                    }
                    else
                    if (event == Event.rtfm_event) {
                        logInvalidMessage ();
                        terminateTheClient ();
                    }
                    else {
                        //  Process all other events
                        logProtocolError ();
                        terminateTheClient ();
                    }
                    break;

                case ready_state:
                    if (event == Event.cheezburger_event) {
                        processThePatch ();
                        refillCreditAsNeeded ();
                    }
                    else
                    if (event == Event.hugz_event) {
                        request.setId (FmqMsg.HUGZ_OK);
                        request.send (dealer);
                        request = new FmqMsg (0);
                    }
                    else
                    if (event == Event.subscribe_event) {
                        formatIcanhazCommand ();
                        request.setId (FmqMsg.ICANHAZ);
                        request.send (dealer);
                        request = new FmqMsg (0);
                    }
                    else
                    if (event == Event.send_credit_event) {
                        request.setId (FmqMsg.NOM);
                        request.send (dealer);
                        request = new FmqMsg (0);
                    }
                    else
                    if (event == Event.icanhaz_ok_event) {
                    }
                    else
                    if (event == Event.srsly_event) {
                        logAccessDenied ();
                        terminateTheClient ();
                        state = State.start_state;
                    }
                    else
                    if (event == Event.rtfm_event) {
                        logInvalidMessage ();
                        terminateTheClient ();
                    }
                    else {
                        //  Process all other events
                        logProtocolError ();
                        terminateTheClient ();
                    }
                    break;

                }
                if (next_event == Event.terminate_event) {
                    stopped = true;
                    break;
                }
            }
        }

        //  Restart client dialog from zero

        private void restart (String endpoint)
        {
            //  Reconnect to new endpoint if specified
            if (endpoint != null)  {
                if (dealer != null)
                    ctx.destroySocket (dealer);
                dealer = ctx.createSocket (ZMQ.DEALER);
                dealer.connect (endpoint);
            }
            //  Clear out any previous request data
            if (request != null)
                request.destroy ();
            request = new FmqMsg (0);

            //  Restart dialog state machine from zero
            state = State.start_state;
            expires_at = 0;

            //  Application hook to reinitialize dialog
            //  Provides us with an event to kick things off
            initializeTheClient ();
            execute (next_event);
        }

        private void controlMessage ()
        {
            ZMsg msg = ZMsg.recvMsg (pipe);
            String method = msg.popString ();
            if (method.equals ("SUBSCRIBE")) {
                String path = msg.popString ();
                //  Store subscription along with any previous ones         
                //  Check we don't already have a subscription for this path
                for (Sub sub: subs) {                                       
                    if (path.equals (sub.path))                             
                        return;                                             
                }                                                           
                //  Subscription path must start with '/'                   
                //  We'll do better error handling later                    
                assert (path.startsWith ("/"));                             
                                                                            
                //  New subscription, store it for later replay             
                String inbox = config.resolve ("client/inbox", ".inbox");   
                sub = new Sub (this, inbox, path);                          
                subs.add (sub);                                             
                                                                            
                //  If we're connected, then also send to server            
                if (connected)                                              
                    next_event = Event.subscribe_event;                     
            }
            else
            if (method.equals ("SET INBOX")) {
                String path = msg.popString ();
                config.setPath ("client/inbox", path);
            }
            else
            if (method.equals ("SET RESYNC")) {
                long enabled = Long.parseLong (msg.popString ());
                //  Request resynchronization from server                
                config.setPath ("client/resync", enabled > 0 ? "1" :"0");
            }
            else
            if (method.equals ("CONNECT")) {
                String endpoint = msg.popString ();
                restart (endpoint);
            }
            else
            if (method.equals ("CONFIG")) {
                String config_file = msg.popString ();
                config.destroy ();
                config = FmqConfig.load (config_file);
                if (config != null)
                    applyConfig ();
                else {
                    System.out.printf ("E: cannot load config file '%s'\n", config_file);
                    config = new FmqConfig ("root", null);
                }
            }
            else
            if (method.equals ("SETOPTION")) {
                String path = msg.popString ();
                String value = msg.popString ();
                config.setPath (path, value);
                config ();
            }
            else
            if (method.equals ("STOP")) {
                pipe.send ("OK");
                stopped = true;
            }
            msg.destroy ();

            if (next_event != null)
                execute (next_event);
        }

        private void serverMessage ()
        {
            if (reply != null)
                reply.destroy ();
            reply = FmqMsg.recv (dealer);
            if (reply == null)
                return;         //  Interrupted; do nothing
    
            if (reply.id () == FmqMsg.SRSLY)
                execute (Event.srsly_event);
            else
            if (reply.id () == FmqMsg.RTFM)
                execute (Event.rtfm_event);
            else
            if (reply.id () == FmqMsg.ORLY)
                execute (Event.orly_event);
            else
            if (reply.id () == FmqMsg.OHAI_OK)
                execute (Event.ohai_ok_event);
            else
            if (reply.id () == FmqMsg.CHEEZBURGER)
                execute (Event.cheezburger_event);
            else
            if (reply.id () == FmqMsg.HUGZ)
                execute (Event.hugz_event);
            else
            if (reply.id () == FmqMsg.ICANHAZ_OK)
                execute (Event.icanhaz_ok_event);

            //  Any input from server counts as activity
            expires_at = System.currentTimeMillis () + heartbeat * 2;
        }

    }
    //  Finally here's the client thread itself, which polls its two
    //  sockets and processes incoming messages

    private static class ClientThread 
                          implements ZThread.IAttachedRunnable {
        @Override
        public void run (Object [] args, ZContext ctx, Socket pipe)
        {
            Client self = new Client (ctx, pipe);

            while (!self.stopped && !Thread.currentThread().isInterrupted ()) {

                Poller items = ctx.getContext ().poller ();
                items.register (self.pipe, Poller.POLLIN);

                //  Build structure each time since self->dealer can change
                if (self.dealer != null)
                    items.register (self.dealer, Poller.POLLIN);

                if (items.poll (self.heartbeat) == -1)
                    break;              //  Context has been shut down

                //  Process incoming messages; either of these can
                //  throw events into the state machine
                if (items.pollin (0))
                    self.controlMessage ();

                if (items.pollin (1))
                    self.serverMessage ();

                //  Check whether server seems dead
                if (self.expires_at > 0 && System.currentTimeMillis () >= self.expires_at)
                    self.restart (null);
            }
            self.destroy ();
        }
    }

    public static void zclock_log (String fmt, Object ... args) 
    {
        System.out.println (String.format (fmt, args));
    }

}

