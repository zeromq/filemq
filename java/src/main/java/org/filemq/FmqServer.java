
/*  =========================================================================
    fmq_server.c

    Generated class for fmq_server protocol server
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
import java.util.HashMap;

import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;
import org.zeromq.ZMQ.Poller;
import org.zeromq.ZThread;
import org.zeromq.ZMsg;
import org.zeromq.ZContext;
import org.zeromq.ZFrame;



//  ---------------------------------------------------------------------
//  Structure of our front-end API class

public class FmqServer {

    private ZContext ctx;        //  CZMQ context
    private Socket pipe;         //  Pipe through to server


    //  --------------------------------------------------------------------------
    //  Create a new fmq_server and a new server instance

    public FmqServer ()
    {
        ctx = new ZContext ();
        pipe = ZThread.fork (ctx, new ServerThread ());
    }


    //  --------------------------------------------------------------------------
    //  Destroy the fmq_server and stop the server

    public void destroy ()
    {
        pipe.send ("STOP");
        pipe.recvStr ();
        ctx.destroy ();
    }


    //  --------------------------------------------------------------------------
    //  Load server configuration data
    public void configure (final String config_file)
    {
        pipe.sendMore ("CONFIG");
        pipe.send (config_file);
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

    public int bind (final String endpoint)
    {
        assert (endpoint != null);
        pipe.sendMore ("BIND");
        pipe.send (String.format("%s", endpoint));
        String reply = pipe.recvStr ();
        int rc = Integer.parseInt (reply);
        return rc;
    }

    //  --------------------------------------------------------------------------

    public void publish (final String location,final String alias)
    {
        assert (location != null);
        assert (alias != null);
        pipe.sendMore ("PUBLISH");
        pipe.sendMore (String.format("%s", location));
        pipe.send (String.format("%s", alias));
    }

    //  --------------------------------------------------------------------------

    public void setAnonymous (long enabled)
    {
        pipe.sendMore ("SET ANONYMOUS");
        pipe.send (String.format("%d", enabled));
    }



    //  ---------------------------------------------------------------------
    //  State machine constants

    private enum State {
        start_state (1),
        checking_client_state (2),
        challenging_client_state (3),
        ready_state (4),
        dispatching_state (5);

        @SuppressWarnings ("unused")
        private final int state;
        State (int state) 
        {
            this.state = state;
        }
    };

    private enum Event {
        terminate_event (-1),
        ohai_event (1),
        heartbeat_event (2),
        expired_event (3),
        _other_event (4),
        friend_event (5),
        foe_event (6),
        maybe_event (7),
        yarly_event (8),
        icanhaz_event (9),
        nom_event (10),
        hugz_event (11),
        kthxbai_event (12),
        dispatch_event (13),
        send_chunk_event (14),
        send_delete_event (15),
        next_patch_event (16),
        no_credit_event (17),
        finished_event (18);

        @SuppressWarnings ("unused")
        private final int event;
        Event (int event) 
        {
            this.event = event;
        }
    };


    //  There's no point making these configurable
    private static final int CHUNK_SIZE = 1000000;

    //  --------------------------------------------------------------------------      
    //  Subscription object                                                             
                                                                                        
    private static class Sub {                                                          
        private Client client;                //  Always refers to live client          
        private String path;                  //  Path client is subscribed to          
        private Map <String, String> cache;   //  Client's cache list                   
                                                                                        
        //  --------------------------------------------------------------------------  
        //  Constructor                                                                 
                                                                                        
        private Sub (Client client, String path, Map <String, String> cache)            
        {                                                                               
            this.client = client;                                                       
            this.path = path;                                                           
            this.cache = new HashMap <String, String> (cache);                          
            resolveCachePath (this.cache, this);                                        
        }                                                                               
                                                                                        
        //  Cached filenames may be local, in which case prefix them with               
        //  the subscription path so we can do a consistent match.                      
                                                                                        
        private static void resolveCachePath (Map <String, String> cache, Sub self)     
        {                                                                               
            for (String key : cache.keySet ()) {                                        
              if (!key.startsWith ("/")) {                                              
                  String new_key = String.format ("%s/%s", self.path, key);             
                  cache.put (new_key, cache.remove (key));                              
              }                                                                         
            }                                                                           
        }                                                                               
                                                                                        
                                                                                        
        //  --------------------------------------------------------------------------  
        //  Destructor                                                                  
                                                                                        
        private void destroy ()                                                         
        {                                                                               
        }                                                                               
                                                                                        
                                                                                        
        //  --------------------------------------------------------------------------  
        //  Add patch to sub client patches list                                        
                                                                                        
        private void addPatch (FmqPatch patch)                                          
        {                                                                               
            //  Skip file creation if client already has identical file                 
            patch.setDigest ();                                                         
            if (patch.op () == FmqPatch.OP.patch_create) {                              
                String digest = cache.get (patch.virtual ());                           
                if (digest != null && digest.compareToIgnoreCase (patch.digest ()) == 0)
                    return;             //  Just skip patch for this client             
            }                                                                           
            //  Remove any previous patches for the same file                           
            Iterator <FmqPatch> it = client.patches.iterator ();                        
            while (it.hasNext ()) {                                                     
                FmqPatch existing = it.next ();                                         
                if (patch.virtual ().equals (existing.virtual ())) {                    
                    it.remove ();                                                       
                    existing.destroy ();                                                
                    break;                                                              
                }                                                                       
            }                                                                           
            //  Track that we've queued patch for client, so we don't do it twice       
            cache.put (patch.digest (), patch.virtual ());                              
            client.patches.add (patch.dup ());                                          
        }                                                                               
    }                                                                                   

    //  --------------------------------------------------------------------------    
    //  Mount point in memory                                                         
                                                                                      
    private static class Mount {                                                      
        private String location;        //  Physical location                         
        private String alias;           //  Alias into our tree                       
        private FmqDir dir;             //  Directory snapshot                        
        private List <Sub> subs;  //  Client subscriptions                            
                                                                                      
                                                                                      
        //  --------------------------------------------------------------------------
        //  Constructor                                                               
        //  Loads directory tree if possible                                          
                                                                                      
        private Mount (String location, String alias)                                 
        {                                                                             
            //  Mount path must start with '/'                                        
            //  We'll do better error handling later                                  
            assert (alias.startsWith ("/"));                                          
                                                                                      
            this.location = location;                                                 
            this.alias = alias;                                                       
            dir = FmqDir.newFmqDir (location, null);                                  
            subs = new ArrayList <Sub> ();                                            
        }                                                                             
                                                                                      
                                                                                      
        //  --------------------------------------------------------------------------
        //  Destructor                                                                
                                                                                      
        private void destroy ()                                                       
        {                                                                             
            //  Destroy subscriptions                                                 
            for (Sub sub : subs) {                                                    
                sub.destroy ();                                                       
            }                                                                         
            if (dir != null)                                                          
                dir.destroy ();                                                       
        }                                                                             
                                                                                      
                                                                                      
        //  --------------------------------------------------------------------------
        //  Reloads directory tree and returns true if activity, false if the same    
                                                                                      
        private boolean refresh (Server server)                                       
        {                                                                             
            boolean activity = false;                                                 
                                                                                      
            //  Get latest snapshot and build a patches list for any changes          
            FmqDir latest = FmqDir.newFmqDir (location, null);                        
            List <FmqPatch> patches = FmqDir.diff (dir, latest, alias);               
                                                                                      
            //  Drop old directory and replace with latest version                    
            if (dir != null)                                                          
                dir.destroy ();                                                       
            dir = latest;                                                             
                                                                                      
            //  Copy new patches to clients' patches list                             
            for (Sub sub : subs) {                                                    
                for (FmqPatch patch : patches) {                                      
                    sub.addPatch (patch);                                             
                    activity = true;                                                  
                }                                                                     
            }                                                                         
                                                                                      
            //  Destroy patches, they've all been copied                              
            for (FmqPatch patch : patches) {                                          
                patch.destroy ();                                                     
            }                                                                         
            return activity;                                                          
        }                                                                             
                                                                                      
                                                                                      
        //  --------------------------------------------------------------------------
        //  Store subscription for mount point                                        
                                                                                      
        private void storeSub (Client client, FmqMsg request)                         
        {                                                                             
            //  Store subscription along with any previous ones                       
            //  Coalesce subscriptions that are on same path                          
            String path = request.path ();                                            
            Iterator <Sub> it = subs.iterator ();                                     
            while (it.hasNext ()) {                                                   
                Sub sub = it.next ();                                                 
                if (client == sub.client) {                                           
                    //  If old subscription is superset/same as new, ignore new       
                    if (path.startsWith (sub.path))                                   
                        return;                                                       
                    else                                                              
                    //  If new subscription is superset of old one, remove old        
                    if (sub.path.startsWith (path)) {                                 
                        it.remove ();                                                 
                        sub.destroy ();                                               
                    }                                                                 
                }                                                                     
            }                                                                         
            //  New subscription for this client, append to our list                  
            Sub sub = new Sub (client, path, request.cache ());                       
            subs.add (sub);                                                           
                                                                                      
            //  If client requested resync, send full mount contents now              
            if (request.optionsNumber ("RESYNC", 0) == 1) {                           
                List <FmqPatch> patches = FmqDir.resync (dir, alias);                 
                for (FmqPatch patch : patches) {                                      
                    sub.addPatch (patch);                                             
                    patch.destroy ();                                                 
                }                                                                     
            }                                                                         
        }                                                                             
                                                                                      
                                                                                      
        //  --------------------------------------------------------------------------
        //  Purge subscriptions for a specified client                                
                                                                                      
        private void purgeSub (Client client)                                         
        {                                                                             
            Iterator <Sub> it = subs.iterator ();                                     
            while (it.hasNext ()) {                                                   
                Sub sub = it.next ();                                                 
                if (sub.client == client) {                                           
                    it.remove ();                                                     
                    sub.destroy ();                                                   
                }                                                                     
            }                                                                         
        }                                                                             
    }                                                                                 

    //  Client hash function that checks if client is alive                         
    private static void clientDispatch (Map <String, Client> clients, Server server)
    {                                                                               
        for (Client client : clients.values ())                                     
            server.clientExecute (client, Event.dispatch_event);                    
    }                                                                               


    //  ---------------------------------------------------------------------
    //  Context for each client connection

    private static class Client {
        //  Properties accessible to client actions
        private long heartbeat;          //  Heartbeat interval
        private Event next_event;         //  Next event
        private int credit;                     //  Credit remaining           
        private List <FmqPatch> patches;  //  Patches to send                  
        private FmqPatch patch;                 //  Current patch              
        private FmqFile file;                   //  Current file we're sending 
        private long offset;                    //  Offset of next read in file
        private long sequence;                  //  Sequence number for chunk  

        //  Properties you should NOT touch
        private Socket router;                  //  Socket to client
        private long heartbeat_at;              //  Next heartbeat at this time
        private long expires_at;                //  Expires at this time
        private State state;                    //  Current state
        private Event event;                    //  Current event
        private String hashkey;                 //  Key into clients hash
        private ZFrame address;                 //  Client address identity
        private FmqMsg request;                 //  Last received request
        private FmqMsg reply;                   //  Reply to send out, if any


        //  Client methods

        private Client (String hashkey, ZFrame address)
        {
            this.hashkey = hashkey;
            this.state = State.start_state;
            this.address = address.duplicate ();
            reply = new FmqMsg (0);
            reply.setAddress (this.address);
            patches = new ArrayList <FmqPatch> ();
        }

        private void destroy ()
        {
            if (address != null)
                address.destroy ();
            if (request != null)
                request.destroy ();
            if (reply != null)
                reply.destroy ();
            for (FmqPatch patch : patches)
                patch.destroy ();         
        }

        private void setRequest (FmqMsg request)
        {
            if (this.request != null)
                this.request.destroy ();
            this.request = request;

            //  Any input from client counts as heartbeat
            heartbeat_at = System.currentTimeMillis () + heartbeat;
            //  Any input from client counts as activity
            expires_at = System.currentTimeMillis () + heartbeat * 3;
        }
    }

    //  Client hash function that calculates tickless timer
    private static long clientTickless (Map <String, Client> clients , long tickless)
    {
        for (Client self: clients.values ()) {
            if (tickless > self.heartbeat_at)
                tickless = self.heartbeat_at;
        }
        return tickless;
    }

    //  Client hash function that checks if client is alive
    private static void clientPing (Map <String, Client> clients , Server server)
    {
        Iterator <Map.Entry <String, Client>> it = clients.entrySet ().iterator ();
        while (it.hasNext ()) {
            Client self = it.next ().getValue ();
            //  Expire client if it's not answered us in a while
            if (System.currentTimeMillis () >= self.expires_at && self.expires_at > 0) {
                //  In case dialog doesn't handle expired_event by destroying
                //  client, set expires_at to zero to prevent busy looping
                self.expires_at = 0;
                if (server.clientExecute (self, Event.expired_event, true))
                    it.remove ();
            }
            else
            //  Check whether to send heartbeat to client
            if (System.currentTimeMillis () >= self.heartbeat_at) {
                server.clientExecute (self, Event.heartbeat_event);
                self.heartbeat_at = System.currentTimeMillis () + self.heartbeat;
            }
        }
    }

    //  ---------------------------------------------------------------------
    //  Context for the server thread

    private static class Server {
        //  Properties accessible to client actions

        //  Properties you should NOT touch
        private ZContext ctx;                   //  Own CZMQ context
        private Socket pipe;                    //  Socket to back to caller
        private Socket router;                  //  Socket to talk to clients
        private Map <String, Client> clients;   //  Clients we've connected to
        private boolean stopped;                //  Has server stopped?
        private FmqConfig config;               //  Configuration tree
        private int monitor;                    //  Monitor interval
        private long monitor_at;                //  Next monitor at this time
        private int heartbeat;                  //  Heartbeat for clients

        //  Server methods
        private void config ()
        {
            //  Get standard server configuration
            monitor = Integer.parseInt (
                config.resolve ("server/monitor", "1")) * 1000;
            heartbeat = Integer.parseInt (
                config.resolve ("server/heartbeat", "1")) * 1000;
            monitor_at = System.currentTimeMillis () + monitor;
        }

        private Server (ZContext ctx, Socket pipe)
        {
            this.ctx = ctx;
            this.pipe = pipe;
            router = ctx.createSocket (ZMQ.ROUTER);
            clients = new HashMap <String, Client> ();
            config = new FmqConfig ("root", null);
            config ();
        }

        private void destroy ()
        {
            ctx.destroySocket (router);
            config.destroy ();
            for (Client c: clients.values ())
                c.destroy ();
        }

        //  Apply configuration tree:
        //   * apply server configuration
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
                if (section.name ().equals ("bind")) {
                    String endpoint = section.resolve ("endpoint", "?");
                    port = bind (router, endpoint);
                }
                else
                if (section.name ().equals ("publish")) {
                    String location = section.resolve ("location", "?");
                    String alias = section.resolve ("alias", "?");
                    Mount mount = new Mount (location, alias);
                    mounts.add (mount);                       
                }
                else
                if (section.name ().equals ("set_anonymous")) {
                    long enabled = Long.parseLong (section.resolve ("enabled", ""));
                    //  Enable anonymous access without a config file             
                    config.setPath ("security/anonymous", enabled > 0 ? "1" :"0");
                }
                section = section.next ();
            }
            config ();
        }

        private void controlMessage ()
        {
            ZMsg msg = ZMsg.recvMsg (pipe);
            String method = msg.popString ();
            if (method.equals ("BIND")) {
                String endpoint = msg.popString ();
                port = bind (router, endpoint);
                pipe.send (String.format ("%d", port));
            }
            else
            if (method.equals ("PUBLISH")) {
                String location = msg.popString ();
                String alias = msg.popString ();
                Mount mount = new Mount (location, alias);
                mounts.add (mount);                       
            }
            else
            if (method.equals ("SET ANONYMOUS")) {
                long enabled = Long.parseLong (msg.popString ());
                //  Enable anonymous access without a config file             
                config.setPath ("security/anonymous", enabled > 0 ? "1" :"0");
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
            msg = null;
        }

        //  Custom actions for state machine

        private void terminateTheClient (Client client)
        {
            for (Mount mount : mounts) {              
                mount.purgeSub (client);              
            }                                         
            client.next_event = Event.terminate_event;
        }

        private void tryAnonymousAccess (Client client)
        {
            if (Integer.parseInt (config.resolve ("security/anonymous", "0")) == 1)
                client.next_event = Event.friend_event;                            
            else                                                                   
            if (Integer.parseInt (config.resolve ("security/plain", "0")) == 1)    
                client.next_event = Event.maybe_event;                             
            else                                                                   
                client.next_event = Event.foe_event;                               
        }

        private void listSecurityMechanisms (Client client)
        {
            if (Integer.parseInt (config.resolve ("security/anonymous", "0")) == 1)
                client.reply.appendMechanisms ("ANONYMOUS");                       
            if (Integer.parseInt (config.resolve ("security/plain", "0")) == 1)    
                client.reply.appendMechanisms ("PLAIN");                           
        }

        private void trySecurityMechanism (Client client)
        {
            client.next_event = Event.foe_event;                             
            String [] result = new String [2];                               
            String login, password;                                          
            if (client.request.mechanism ().equals ("PLAIN")                 
            && FmqSasl.plainDecode (client.request.response (), result)) {   
                login = result [0];                                          
                password = result [1];                                       
                FmqConfig account = config.locate ("security/plain/account");
                while (account != null) {                                    
                    if (account.resolve ("login", "").equals (login)         
                    && account.resolve ("password", "").equals (password)) { 
                        client.next_event = Event.friend_event;              
                        break;                                               
                    }                                                        
                    account = account.next ();                               
                }                                                            
            }                                                                
        }

        private void storeClientSubscription (Client client)
        {
            //  Find mount point with longest match to subscription   
            String path = client.request.path ();                     
                                                                      
            Mount mount = mounts.isEmpty () ? null : mounts.get (0);  
            for (Mount check : mounts) {                              
                //  If check->alias is prefix of path and alias is    
                //  longer than current mount then we have a new mount
                if (path.startsWith (check.alias)                     
                &&  check.alias.length () > mount.alias.length ())    
                    mount = check;                                    
            }                                                         
            mount.storeSub (client, client.request);                  
        }

        private void storeClientCredit (Client client)
        {
            client.credit += client.request.credit ();
        }

        private void monitorTheServer (Client client)
        {
            boolean activity = false;          
            for (Mount mount : mounts) {       
                if (mount.refresh (this))      
                    activity = true;           
            }                                  
            if (activity)                      
                clientDispatch (clients, this);
        }

        private void getNextPatchForClient (Client client)
        {
            //  Get next patch for client if we're not doing one already                    
            if (client.patch == null)                                                       
                client.patch = client.patches.isEmpty () ? null : client.patches.remove (0);
            if (client.patch == null) {                                                     
                client.next_event = Event.finished_event;                                   
                return;                                                                     
            }                                                                               
            //  Get virtual filename from patch                                             
            client.reply.setFilename (client.patch.virtual ());                             
                                                                                            
            //  We can process a delete patch right away                                    
            if (client.patch.op () == FmqPatch.OP.patch_delete) {                           
                client.reply.setSequence (client.sequence++);                               
                client.reply.setOperation (FmqMsg.FMQ_MSG_FILE_DELETE);                     
                client.next_event = Event.send_delete_event;                                
                                                                                            
                //  No reliability in this version, assume patch delivered safely           
                client.patch.destroy ();                                                    
                client.patch = null;                                                        
            }                                                                               
            else                                                                            
            if (client.patch.op () == FmqPatch.OP.patch_create) {                           
                //  Create patch refers to file, open that for input if needed              
                if (client.file == null) {                                                  
                    client.file = client.patch.file ().dup ();                              
                    if (client.file.input () == false) {                                    
                        //  File no longer available, skip it                               
                        client.patch.destroy ();                                            
                        client.file.destroy ();                                             
                        client.patch = null;                                                
                        client.file = null;                                                 
                        client.next_event = Event.next_patch_event;                         
                        return;                                                             
                    }                                                                       
                    client.offset = 0;                                                      
                }                                                                           
                //  Get next chunk for file                                                 
                FmqChunk chunk = client.file.read (CHUNK_SIZE, client.offset);              
                assert (chunk != null);                                                     
                                                                                            
                //  Check if we have the credit to send chunk                               
                if (chunk.size () <= client.credit) {                                       
                    client.reply.setSequence (client.sequence++);                           
                    client.reply.setOperation (FmqMsg.FMQ_MSG_FILE_CREATE);                 
                    client.reply.setOffset (client.offset);                                 
                    client.reply.setChunk (new ZFrame (chunk.data ()));                     
                                                                                            
                    client.offset += chunk.size ();                                         
                    client.credit -= chunk.size ();                                         
                    client.next_event = Event.send_chunk_event;                             
                                                                                            
                    //  Zero-sized chunk means end of file                                  
                    if (chunk.size () == 0) {                                               
                        client.file.destroy ();                                             
                        client.patch.destroy ();                                            
                        client.patch = null;                                                
                        client.file = null;                                                 
                    }                                                                       
                }                                                                           
                else                                                                        
                    client.next_event = Event.no_credit_event;                              
                                                                                            
                chunk.destroy ();                                                           
            }                                                                               
        }

        //  Execute state machine as long as we have events
        //  Returns true, if it requires remove on iteration

        private boolean clientExecute (Client client, Event event)
        {
            return clientExecute (client, event, false);
        }

        private boolean clientExecute (Client client, Event event, boolean oniter)
        {
            client.next_event = event;
            while (client.next_event != null) {
                client.event = client.next_event;
                client.next_event = null;
                switch (client.state) {
                case start_state:
                    if (client.event == Event.ohai_event) {
                        tryAnonymousAccess (client);
                        client.state = State.checking_client_state;
                    }
                    else
                    if (client.event == Event.heartbeat_event) {
                    }
                    else
                    if (client.event == Event.expired_event) {
                        terminateTheClient (client);
                    }
                    else {
                            //  Process all other events
                        client.reply.setId (FmqMsg.RTFM);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        terminateTheClient (client);
                    }
                    break;

                case checking_client_state:
                    if (client.event == Event.friend_event) {
                        client.reply.setId (FmqMsg.OHAI_OK);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        client.state = State.ready_state;
                    }
                    else
                    if (client.event == Event.foe_event) {
                        client.reply.setId (FmqMsg.SRSLY);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        terminateTheClient (client);
                    }
                    else
                    if (client.event == Event.maybe_event) {
                        listSecurityMechanisms (client);
                        client.reply.setId (FmqMsg.ORLY);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        client.state = State.challenging_client_state;
                    }
                    else
                    if (client.event == Event.heartbeat_event) {
                    }
                    else
                    if (client.event == Event.expired_event) {
                        terminateTheClient (client);
                    }
                    else
                    if (client.event == Event.ohai_event) {
                        tryAnonymousAccess (client);
                        client.state = State.checking_client_state;
                    }
                    else {
                            //  Process all other events
                        client.reply.setId (FmqMsg.RTFM);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        terminateTheClient (client);
                    }
                    break;

                case challenging_client_state:
                    if (client.event == Event.yarly_event) {
                        trySecurityMechanism (client);
                        client.state = State.checking_client_state;
                    }
                    else
                    if (client.event == Event.heartbeat_event) {
                    }
                    else
                    if (client.event == Event.expired_event) {
                        terminateTheClient (client);
                    }
                    else
                    if (client.event == Event.ohai_event) {
                        tryAnonymousAccess (client);
                        client.state = State.checking_client_state;
                    }
                    else {
                            //  Process all other events
                        client.reply.setId (FmqMsg.RTFM);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        terminateTheClient (client);
                    }
                    break;

                case ready_state:
                    if (client.event == Event.icanhaz_event) {
                        storeClientSubscription (client);
                        client.reply.setId (FmqMsg.ICANHAZ_OK);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                    }
                    else
                    if (client.event == Event.nom_event) {
                        storeClientCredit (client);
                        getNextPatchForClient (client);
                        client.state = State.dispatching_state;
                    }
                    else
                    if (client.event == Event.hugz_event) {
                        client.reply.setId (FmqMsg.HUGZ_OK);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                    }
                    else
                    if (client.event == Event.kthxbai_event) {
                        terminateTheClient (client);
                    }
                    else
                    if (client.event == Event.dispatch_event) {
                        getNextPatchForClient (client);
                        client.state = State.dispatching_state;
                    }
                    else
                    if (client.event == Event.heartbeat_event) {
                        client.reply.setId (FmqMsg.HUGZ);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                    }
                    else
                    if (client.event == Event.expired_event) {
                        terminateTheClient (client);
                    }
                    else
                    if (client.event == Event.ohai_event) {
                        tryAnonymousAccess (client);
                        client.state = State.checking_client_state;
                    }
                    else {
                            //  Process all other events
                        client.reply.setId (FmqMsg.RTFM);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        terminateTheClient (client);
                    }
                    break;

                case dispatching_state:
                    if (client.event == Event.send_chunk_event) {
                        client.reply.setId (FmqMsg.CHEEZBURGER);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        getNextPatchForClient (client);
                    }
                    else
                    if (client.event == Event.send_delete_event) {
                        client.reply.setId (FmqMsg.CHEEZBURGER);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        getNextPatchForClient (client);
                    }
                    else
                    if (client.event == Event.next_patch_event) {
                        getNextPatchForClient (client);
                    }
                    else
                    if (client.event == Event.no_credit_event) {
                        client.state = State.ready_state;
                    }
                    else
                    if (client.event == Event.finished_event) {
                        client.state = State.ready_state;
                    }
                    else
                    if (client.event == Event.heartbeat_event) {
                    }
                    else
                    if (client.event == Event.expired_event) {
                        terminateTheClient (client);
                    }
                    else
                    if (client.event == Event.ohai_event) {
                        tryAnonymousAccess (client);
                        client.state = State.checking_client_state;
                    }
                    else {
                            //  Process all other events
                        client.reply.setId (FmqMsg.RTFM);
                        client.reply.send (client.router);
                        client.reply = new FmqMsg (0);
                        client.reply.setAddress (client.address);
                        terminateTheClient (client);
                    }
                    break;

                }
                if (client.next_event == Event.terminate_event) {
                    //  Automatically calls client_destroy
                    client.destroy ();
                    if (oniter)
                        return true;
                    clients.remove (client.hashkey);
                    break;
                }
            }
            return false;
        }

        private void clientMessage ()
        {
            FmqMsg request = FmqMsg.recv (router);
            if (request == null)
                return;         //  Interrupted; do nothing

            String hashkey = request.address ().strhex ();
            Client client = clients.get (hashkey);
            if (client == null) {
                client = new Client (hashkey, request.address ());
                client.heartbeat = heartbeat;
                client.router = router;
                clients.put (hashkey, client);
            }

            client.setRequest (request);
            if (request.id () == FmqMsg.OHAI)
                clientExecute (client, Event.ohai_event);
            else
            if (request.id () == FmqMsg.YARLY)
                clientExecute (client, Event.yarly_event);
            else
            if (request.id () == FmqMsg.ICANHAZ)
                clientExecute (client, Event.icanhaz_event);
            else
            if (request.id () == FmqMsg.NOM)
                clientExecute (client, Event.nom_event);
            else
            if (request.id () == FmqMsg.HUGZ)
                clientExecute (client, Event.hugz_event);
            else
            if (request.id () == FmqMsg.KTHXBAI)
                clientExecute (client, Event.kthxbai_event);
        }
    }

    //  The server runs as a background thread so that we can run multiple
    //  engines at once. The API talks to the server thread over an inproc
    //  pipe.

    //  Finally here's the server thread itself, which polls its two
    //  sockets and processes incoming messages

    private static class ServerThread 
                          implements ZThread.IAttachedRunnable {
        @Override
        public void run (Object [] args, ZContext ctx, Socket pipe)
        {
            Server self = new Server (ctx, pipe);

            Poller items = ctx.getContext ().poller ();
            items.register (self.pipe, Poller.POLLIN);
            items.register (self.router, Poller.POLLIN);

            self.monitor_at = System.currentTimeMillis () + self.monitor;
            while (!self.stopped && !Thread.currentThread ().isInterrupted ()) {
                //  Calculate tickless timer, up to interval seconds
                long tickless = System.currentTimeMillis () + self.monitor;
                tickless = clientTickless (self.clients, tickless);

                //  Poll until at most next timer event
                long rc = items.poll (tickless - System.currentTimeMillis ());
                if (rc == -1)
                    break;              //  Context has been shut down

                //  Process incoming message from either socket
                if (items.pollin (0))
                    self.controlMessage ();

                if (items.pollin (1))
                    self.clientMessage ();

                //  Send heartbeats to idle clients as needed
                clientPing (self.clients, self);

                //  If clock went past timeout, then monitor server
                if (System.currentTimeMillis () >= self.monitor_at) {
                    self.monitorTheServer (null);
                    self.monitor_at = System.currentTimeMillis () + self.monitor;
                }
            }
            self.destroy ();
        }
    }

    public static void zclock_log (String fmt, Object ... args) 
    {
        System.out.println (String.format (fmt, args));
    }

    public static int bind (Socket sock, String addr) 
    {
        int pos = addr.lastIndexOf (':');
        String port = addr.substring (pos + 1);
        if (port.equals ("*")) 
            return sock.bindToRandomPort (addr.substring (0, pos));
        else {
            sock.bind (addr);
            return Integer.parseInt (port);
        }
    }
}

