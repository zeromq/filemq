/*  =========================================================================
    fmq_client - a filemq client

    Generated class for fmq_client protocol client
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

/*
@header
  the fmq_client class implements a generic filemq client.
@discuss
@end
*/

#include <czmq.h>
#include "../include/fmq_client.h"
#include "../include/fmq_msg.h"
#include "../include/fmq.h"

//  The client runs as a background thread so that we can run multiple
//  engines at once. The API talks to the client thread over an inproc
//  pipe.

static void
    client_thread (void *args, zctx_t *ctx, void *pipe);

//  ---------------------------------------------------------------------
//  Structure of our front-end API class

struct _fmq_client_t {
    zctx_t *ctx;        //  CZMQ context
    void *pipe;         //  Pipe through to client
};


//  --------------------------------------------------------------------------
//  Create a new fmq_client and a new client instance

fmq_client_t *
fmq_client_new (void)
{
    fmq_client_t *self = (fmq_client_t *) zmalloc (sizeof (fmq_client_t));
    self->ctx = zctx_new ();
    self->pipe = zthread_fork (self->ctx, client_thread, NULL);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fmq_client and stop the client

void
fmq_client_destroy (fmq_client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_client_t *self = *self_p;
        zstr_send (self->pipe, "STOP");
        char *string = zstr_recv (self->pipe);
        free (string);
        zctx_destroy (&self->ctx);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Load client configuration data

void
fmq_client_configure (fmq_client_t *self, const char *config_file)
{
    zstr_sendm (self->pipe, "CONFIG");
    zstr_send  (self->pipe, config_file);
}


//  --------------------------------------------------------------------------
//  Set one configuration key value

void
fmq_client_setoption (fmq_client_t *self, const char *path, const char *value)
{
    zstr_sendm (self->pipe, "SETOPTION");
    zstr_sendm (self->pipe, path);
    zstr_send  (self->pipe, value);
}


//  --------------------------------------------------------------------------
//  Create outgoing connection to server

void
fmq_client_connect (fmq_client_t *self, const char *endpoint)
{
    zstr_sendm (self->pipe, "CONNECT");
    zstr_send  (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------
//  Wait for message from API

zmsg_t *
fmq_client_recv (fmq_client_t *self)
{
    zmsg_t *msg = zmsg_recv (self->pipe);
    return msg;
}


//  --------------------------------------------------------------------------
//  Return API pipe handle for polling

void *
fmq_client_handle (fmq_client_t *self)
{
    return self->pipe;
}


//  --------------------------------------------------------------------------

void
fmq_client_subscribe (fmq_client_t *self, const char *path)
{
    assert (self);
    assert (path);
    zstr_sendm (self->pipe, "SUBSCRIBE");
    zstr_sendf (self->pipe, "%s", path);
}


//  --------------------------------------------------------------------------

void
fmq_client_set_inbox (fmq_client_t *self, const char *path)
{
    assert (self);
    assert (path);
    zstr_sendm (self->pipe, "SET INBOX");
    zstr_sendf (self->pipe, "%s", path);
}


//  --------------------------------------------------------------------------

void
fmq_client_set_resync (fmq_client_t *self, long enabled)
{
    assert (self);
    zstr_sendm (self->pipe, "SET RESYNC");
    zstr_sendf (self->pipe, "%ld", enabled);
}


//  ---------------------------------------------------------------------
//  State machine constants

typedef enum {
    start_state = 1,
    requesting_access_state = 2,
    subscribing_state = 3,
    ready_state = 4,
    terminated_state = 5
} state_t;

typedef enum {
    initialize_event = 1,
    srsly_event = 2,
    rtfm_event = 3,
    _other_event = 4,
    orly_event = 5,
    ohai_ok_event = 6,
    ok_event = 7,
    finished_event = 8,
    cheezburger_event = 9,
    hugz_event = 10,
    send_credit_event = 11,
    icanhaz_ok_event = 12
} event_t;


//  Maximum number of server connections we allow
#define MAX_SERVERS     256

//  Forward declarations
typedef struct _client_t client_t;
typedef struct _server_t server_t;

//  Forward declarations
typedef struct _sub_t sub_t;


//  ---------------------------------------------------------------------
//  Context for the client thread

struct _client_t {
    //  Properties accessible to client actions
    zlist_t *subs;              //  Subscriptions               
    sub_t *sub;                 //  Subscription we want to send
    //  Properties you should NOT touch
    zctx_t *ctx;                //  Own CZMQ context
    void *pipe;                 //  Socket to back to caller
    server_t *servers [MAX_SERVERS];
                                //  Server connections
    uint nbr_servers;           //  How many connections we have
    bool dirty;                 //  If true, rebuild pollset
    bool stopped;               //  Is the client stopped?
    zconfig_t *config;          //  Configuration tree
    int heartbeat;              //  Heartbeat interval
};

//  ---------------------------------------------------------------------
//  Context for each server connection

struct _server_t {
    //  Properties accessible to server actions
    event_t next_event;         //  Next event
    size_t credit;              //  Current credit pending
    fmq_file_t *file;           //  File we're writing to 
    //  Properties you should NOT touch
    zctx_t *ctx;                //  Own CZMQ context
    uint index;                 //  Index into client->server_array
    void *dealer;               //  Socket to talk to server
    int64_t expires_at;         //  Connection expires at
    state_t state;              //  Current state
    event_t event;              //  Current event
    char *endpoint;             //  Server endpoint
    fmq_msg_t *request;         //  Next message to send
    fmq_msg_t *reply;           //  Last received reply
};

static void
client_server_execute (client_t *self, server_t *server, int event);

//  There's no point making these configurable
#define CREDIT_SLICE    1000000
#define CREDIT_MINIMUM  (CREDIT_SLICE * 4) + 1

//  Subscription in memory
struct _sub_t {
    client_t *client;           //  Pointer to parent client
    char *inbox;                //  Inbox location
    char *path;                 //  Path we subscribe to
};

static sub_t *
sub_new (client_t *client, char *inbox, char *path)
{
    sub_t *self = (sub_t *) zmalloc (sizeof (sub_t));
    self->client = client;
    self->inbox = strdup (inbox);
    self->path = strdup (path);
    return self;
}

static void
sub_destroy (sub_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sub_t *self = *self_p;
        free (self->inbox);
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}

//  Return new cache object for subscription path

static zhash_t *
sub_cache (sub_t *self)
{
    //  Get directory cache for this path
    fmq_dir_t *dir = fmq_dir_new (self->path + 1, self->inbox);
    zhash_t *cache = dir? fmq_dir_cache (dir): NULL;
    fmq_dir_destroy (&dir);
    return cache;
}


//  Server methods

static server_t *
server_new (zctx_t *ctx, char *endpoint)
{
    server_t *self = (server_t *) zmalloc (sizeof (server_t));
    self->ctx = ctx;
    self->endpoint = strdup (endpoint);
    self->dealer = zsocket_new (self->ctx, ZMQ_DEALER);
    self->request = fmq_msg_new (0);
    zsocket_connect (self->dealer, endpoint);
    self->state = start_state;
    
    return self;
}

static void
server_destroy (server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        server_t *self = *self_p;
        zsocket_destroy (self->ctx, self->dealer);
        fmq_msg_destroy (&self->request);
        fmq_msg_destroy (&self->reply);
        free (self->endpoint);
        
        free (self);
        *self_p = NULL;
    }
}

//  Client methods

static void
client_config_self (client_t *self)
{
    //  Get standard client configuration
    self->heartbeat = atoi (
        zconfig_resolve (self->config, "client/heartbeat", "1")) * 1000;
}

static client_t *
client_new (zctx_t *ctx, void *pipe)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
    self->ctx = ctx;
    self->pipe = pipe;
    self->config = zconfig_new ("root", NULL);
    client_config_self (self);
    self->subs = zlist_new ();
    return self;
}

static void
client_destroy (client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        client_t *self = *self_p;
        zconfig_destroy (&self->config);
        int server_nbr;
        for (server_nbr = 0; server_nbr < self->nbr_servers; server_nbr++) {
            server_t *server = self->servers [server_nbr];
            server_destroy (&server);
        }
        //  Destroy subscriptions                         
        while (zlist_size (self->subs)) {                 
            sub_t *sub = (sub_t *) zlist_pop (self->subs);
            sub_destroy (&sub);                           
        }                                                 
        zlist_destroy (&self->subs);                      
        free (self);
        *self_p = NULL;
    }
}

//  Apply configuration tree:
//   * apply client configuration
//   * print any echo items in top-level sections
//   * apply sections that match methods
static void
client_apply_config (client_t *self)
{
    //  Apply echo commands and class methods
    zconfig_t *section = zconfig_child (self->config);
    while (section) {
        zconfig_t *entry = zconfig_child (section);
        while (entry) {
            if (streq (zconfig_name (entry), "echo"))
                zclock_log (zconfig_value (entry));
            entry = zconfig_next (entry);
        }
        if (streq (zconfig_name (section), "subscribe")) {
            char *path = zconfig_resolve (section, "path", "?");
            //  Store subscription along with any previous ones                    
            //  Check we don't already have a subscription for this path           
            self->sub = (sub_t *) zlist_first (self->subs);                        
            while (self->sub) {                                                    
                if (streq (path, self->sub->path))                                 
                    return;                                                        
                self->sub = (sub_t *) zlist_next (self->subs);                     
            }                                                                      
            //  Subscription path must start with '/'                              
            //  We'll do better error handling later                               
            assert (*path == '/');                                                 
                                                                                   
            //  New subscription, store it for later replay                        
            char *inbox = zconfig_resolve (self->config, "client/inbox", ".inbox");
            self->sub = sub_new (self, inbox, path);                               
            zlist_append (self->subs, self->sub);                                  
        }
        else
        if (streq (zconfig_name (section), "set_inbox")) {
            char *path = zconfig_resolve (section, "path", "?");
            zconfig_path_set (self->config, "client/inbox", path);
        }
        else
        if (streq (zconfig_name (section), "set_resync")) {
            long enabled = atoi (zconfig_resolve (section, "enabled", ""));
            //  Request resynchronization from server                           
            zconfig_path_set (self->config, "client/resync", enabled? "1" :"0");
        }
        section = zconfig_next (section);
    }
    client_config_self (self);
}

//  Process message from pipe
static void
client_control_message (client_t *self)
{
    zmsg_t *msg = zmsg_recv (self->pipe);
    char *method = zmsg_popstr (msg);
    if (streq (method, "SUBSCRIBE")) {
        char *path = zmsg_popstr (msg);
        //  Store subscription along with any previous ones                    
        //  Check we don't already have a subscription for this path           
        self->sub = (sub_t *) zlist_first (self->subs);                        
        while (self->sub) {                                                    
            if (streq (path, self->sub->path))                                 
                return;                                                        
            self->sub = (sub_t *) zlist_next (self->subs);                     
        }                                                                      
        //  Subscription path must start with '/'                              
        //  We'll do better error handling later                               
        assert (*path == '/');                                                 
                                                                               
        //  New subscription, store it for later replay                        
        char *inbox = zconfig_resolve (self->config, "client/inbox", ".inbox");
        self->sub = sub_new (self, inbox, path);                               
        zlist_append (self->subs, self->sub);                                  
        free (path);
    }
    else
    if (streq (method, "SET INBOX")) {
        char *path = zmsg_popstr (msg);
        zconfig_path_set (self->config, "client/inbox", path);
        free (path);
    }
    else
    if (streq (method, "SET RESYNC")) {
        char *enabled_string = zmsg_popstr (msg);
        long enabled = atoi (enabled_string);
        free (enabled_string);
        //  Request resynchronization from server                           
        zconfig_path_set (self->config, "client/resync", enabled? "1" :"0");
    }
    else
    if (streq (method, "CONFIG")) {
        char *config_file = zmsg_popstr (msg);
        zconfig_destroy (&self->config);
        self->config = zconfig_load (config_file);
        if (self->config)
            client_apply_config (self);
        else {
            printf ("E: cannot load config file '%s'\n", config_file);
            self->config = zconfig_new ("root", NULL);
        }
        free (config_file);
    }
    else
    if (streq (method, "SETOPTION")) {
        char *path = zmsg_popstr (msg);
        char *value = zmsg_popstr (msg);
        zconfig_path_set (self->config, path, value);
        client_config_self (self);
        free (path);
        free (value);
    }
    else
    if (streq (method, "STOP")) {
        zstr_send (self->pipe, "OK");
        self->stopped = true;
    }
    else
    if (streq (method, "CONNECT")) {
        char *endpoint = zmsg_popstr (msg);
        if (self->nbr_servers < MAX_SERVERS) {
            server_t *server = server_new (self->ctx, endpoint);
            self->servers [self->nbr_servers++] = server;
            self->dirty = true;
            client_server_execute (self, server, initialize_event);
        }
        else
            printf ("E: too many server connections (max %d)\n", MAX_SERVERS);
            
        free (endpoint);
    }
    free (method);
    zmsg_destroy (&msg);
}

//  Custom actions for state machine

static void
try_security_mechanism (client_t *self, server_t *server)
{
    char *login = zconfig_resolve (self->config, "security/plain/login", "guest"); 
    char *password = zconfig_resolve (self->config, "security/plain/password", "");
    zframe_t *frame = fmq_sasl_plain_encode (login, password);                     
    fmq_msg_mechanism_set (server->request, "PLAIN");                              
    fmq_msg_response_set  (server->request, frame);                                
}

static void
connected_to_server (client_t *self, server_t *server)
{
    
}

static void
get_first_subscription (client_t *self, server_t *server)
{
    self->sub = (sub_t *) zlist_first (self->subs);
    if (self->sub)                                 
        server->next_event = ok_event;             
    else                                           
        server->next_event = finished_event;       
}

static void
get_next_subscription (client_t *self, server_t *server)
{
    self->sub = (sub_t *) zlist_next (self->subs);
    if (self->sub)                                
        server->next_event = ok_event;            
    else                                          
        server->next_event = finished_event;      
}

static void
format_icanhaz_command (client_t *self, server_t *server)
{
    fmq_msg_path_set (server->request, self->sub->path);                   
    //  If client app wants full resync, send cache to server              
    if (atoi (zconfig_resolve (self->config, "client/resync", "0")) == 1) {
        fmq_msg_options_insert (server->request, "RESYNC", "1");           
        fmq_msg_cache_set (server->request, sub_cache (self->sub));        
    }                                                                      
}

static void
refill_credit_as_needed (client_t *self, server_t *server)
{
    //  If credit has fallen too low, send more credit       
    size_t credit_to_send = 0;                               
    while (server->credit < CREDIT_MINIMUM) {                
        credit_to_send += CREDIT_SLICE;                      
        server->credit += CREDIT_SLICE;                      
    }                                                        
    if (credit_to_send) {                                    
        fmq_msg_credit_set (server->request, credit_to_send);
        server->next_event = send_credit_event;              
    }                                                        
}

static void
process_the_patch (client_t *self, server_t *server)
{
    char *inbox = zconfig_resolve (self->config, "client/inbox", ".inbox");           
    char *filename = fmq_msg_filename (server->reply);                                
                                                                                      
    //  Filenames from server must start with slash, which we skip                    
    assert (*filename == '/');                                                        
    filename++;                                                                       
                                                                                      
    if (fmq_msg_operation (server->reply) == FMQ_MSG_FILE_CREATE) {                   
        if (server->file == NULL) {                                                   
            server->file = fmq_file_new (inbox, filename);                            
            if (fmq_file_output (server->file)) {                                     
                //  File not writeable, skip patch                                    
                fmq_file_destroy (&server->file);                                     
                return;                                                               
            }                                                                         
        }                                                                             
        //  Try to write, ignore errors in this version                               
        zframe_t *frame = fmq_msg_chunk (server->reply);                              
        fmq_chunk_t *chunk = fmq_chunk_new (zframe_data (frame), zframe_size (frame));
        if (fmq_chunk_size (chunk) > 0) {                                             
            fmq_file_write (server->file, chunk, fmq_msg_offset (server->reply));     
            server->credit -= fmq_chunk_size (chunk);                                 
        }                                                                             
        else {                                                                        
            //  Zero-sized chunk means end of file, so report back to caller          
            zstr_sendm (self->pipe, "DELIVER");                                       
            zstr_sendm (self->pipe, filename);                                        
            zstr_sendf (self->pipe, "%s/%s", inbox, filename);                        
            fmq_file_destroy (&server->file);                                         
        }                                                                             
        fmq_chunk_destroy (&chunk);                                                   
    }                                                                                 
    else                                                                              
    if (fmq_msg_operation (server->reply) == FMQ_MSG_FILE_DELETE) {                   
        zclock_log ("I: delete %s/%s", inbox, filename);                              
        fmq_file_t *file = fmq_file_new (inbox, filename);                            
        fmq_file_remove (file);                                                       
        fmq_file_destroy (&file);                                                     
    }                                                                                 
}

static void
log_access_denied (client_t *self, server_t *server)
{
    puts ("W: server denied us access, retrying...");
}

static void
log_invalid_message (client_t *self, server_t *server)
{
    puts ("E: server claims we sent an invalid message");
}

static void
log_protocol_error (client_t *self, server_t *server)
{
    puts ("E: protocol error");
}

//  Execute state machine as long as we have events
static void
client_server_execute (client_t *self, server_t *server, int event)
{
    server->next_event = event;
    while (server->next_event) {
        server->event = server->next_event;
        server->next_event = 0;
        switch (server->state) {
            case start_state:
                if (server->event == initialize_event) {
                    fmq_msg_id_set (server->request, FMQ_MSG_OHAI);
                    fmq_msg_send (&server->request, server->dealer);
                    server->request = fmq_msg_new (0);
                    server->state = requesting_access_state;
                }
                else
                if (server->event == srsly_event) {
                    log_access_denied (self, server);
                    server->state = terminated_state;
                }
                else
                if (server->event == rtfm_event) {
                    log_invalid_message (self, server);
                    server->state = terminated_state;
                }
                else {
                    //  Process all other events
                    log_protocol_error (self, server);
                    server->state = terminated_state;
                }
                break;

            case requesting_access_state:
                if (server->event == orly_event) {
                    try_security_mechanism (self, server);
                    fmq_msg_id_set (server->request, FMQ_MSG_YARLY);
                    fmq_msg_send (&server->request, server->dealer);
                    server->request = fmq_msg_new (0);
                    server->state = requesting_access_state;
                }
                else
                if (server->event == ohai_ok_event) {
                    connected_to_server (self, server);
                    get_first_subscription (self, server);
                    server->state = subscribing_state;
                }
                else
                if (server->event == srsly_event) {
                    log_access_denied (self, server);
                    server->state = terminated_state;
                }
                else
                if (server->event == rtfm_event) {
                    log_invalid_message (self, server);
                    server->state = terminated_state;
                }
                else {
                    //  Process all other events
                }
                break;

            case subscribing_state:
                if (server->event == ok_event) {
                    format_icanhaz_command (self, server);
                    fmq_msg_id_set (server->request, FMQ_MSG_ICANHAZ);
                    fmq_msg_send (&server->request, server->dealer);
                    server->request = fmq_msg_new (0);
                    get_next_subscription (self, server);
                    server->state = subscribing_state;
                }
                else
                if (server->event == finished_event) {
                    refill_credit_as_needed (self, server);
                    server->state = ready_state;
                }
                else
                if (server->event == srsly_event) {
                    log_access_denied (self, server);
                    server->state = terminated_state;
                }
                else
                if (server->event == rtfm_event) {
                    log_invalid_message (self, server);
                    server->state = terminated_state;
                }
                else {
                    //  Process all other events
                    log_protocol_error (self, server);
                    server->state = terminated_state;
                }
                break;

            case ready_state:
                if (server->event == cheezburger_event) {
                    process_the_patch (self, server);
                    refill_credit_as_needed (self, server);
                }
                else
                if (server->event == hugz_event) {
                    fmq_msg_id_set (server->request, FMQ_MSG_HUGZ_OK);
                    fmq_msg_send (&server->request, server->dealer);
                    server->request = fmq_msg_new (0);
                }
                else
                if (server->event == send_credit_event) {
                    fmq_msg_id_set (server->request, FMQ_MSG_NOM);
                    fmq_msg_send (&server->request, server->dealer);
                    server->request = fmq_msg_new (0);
                }
                else
                if (server->event == icanhaz_ok_event) {
                }
                else
                if (server->event == srsly_event) {
                    log_access_denied (self, server);
                    server->state = terminated_state;
                }
                else
                if (server->event == rtfm_event) {
                    log_invalid_message (self, server);
                    server->state = terminated_state;
                }
                else {
                    //  Process all other events
                    log_protocol_error (self, server);
                    server->state = terminated_state;
                }
                break;

            case terminated_state:
                if (server->event == srsly_event) {
                    log_access_denied (self, server);
                    server->state = terminated_state;
                }
                else
                if (server->event == rtfm_event) {
                    log_invalid_message (self, server);
                    server->state = terminated_state;
                }
                else {
                    //  Process all other events
                }
                break;

        }
    }
}

static void
client_server_message (client_t *self, server_t *server)
{
    if (server->reply)
        fmq_msg_destroy (&server->reply);
    server->reply = fmq_msg_recv (server->dealer);
    if (!server->reply)
        return;         //  Interrupted; do nothing

    //  Any input from server counts as activity
    server->expires_at = zclock_time () + self->heartbeat * 3;
    
    if (fmq_msg_id (server->reply) == FMQ_MSG_SRSLY)
        client_server_execute (self, server, srsly_event);
    else
    if (fmq_msg_id (server->reply) == FMQ_MSG_RTFM)
        client_server_execute (self, server, rtfm_event);
    else
    if (fmq_msg_id (server->reply) == FMQ_MSG_ORLY)
        client_server_execute (self, server, orly_event);
    else
    if (fmq_msg_id (server->reply) == FMQ_MSG_OHAI_OK)
        client_server_execute (self, server, ohai_ok_event);
    else
    if (fmq_msg_id (server->reply) == FMQ_MSG_CHEEZBURGER)
        client_server_execute (self, server, cheezburger_event);
    else
    if (fmq_msg_id (server->reply) == FMQ_MSG_HUGZ)
        client_server_execute (self, server, hugz_event);
    else
    if (fmq_msg_id (server->reply) == FMQ_MSG_ICANHAZ_OK)
        client_server_execute (self, server, icanhaz_ok_event);
}

//  Finally here's the client thread itself, which polls its two
//  sockets and processes incoming messages
static void
client_thread (void *args, zctx_t *ctx, void *pipe)
{
    client_t *self = client_new (ctx, pipe);
    int pollset_size = 1;
    zmq_pollitem_t pollset [MAX_SERVERS] = {
        { self->pipe, 0, ZMQ_POLLIN, 0 }
    };
    while (!self->stopped && !zctx_interrupted) {
        //  Rebuild pollset if we need to
        int server_nbr;
        if (self->dirty) {
            for (server_nbr = 0; server_nbr < self->nbr_servers; server_nbr++) {
                pollset [1 + server_nbr].socket = self->servers [server_nbr]->dealer;
                pollset [1 + server_nbr].events = ZMQ_POLLIN;
            }
            pollset_size = 1 + self->nbr_servers;
        }
        if (zmq_poll (pollset, pollset_size, self->heartbeat * ZMQ_POLL_MSEC) == -1)
            break;              //  Context has been shut down

        //  Process incoming messages; either of these can
        //  throw events into the state machine
        if (pollset [0].revents & ZMQ_POLLIN)
            client_control_message (self);

        //  Here, array of sockets to servers
        for (server_nbr = 0; server_nbr < self->nbr_servers; server_nbr++) {
            if (pollset [1 + server_nbr].revents & ZMQ_POLLIN) {
                server_t *server = self->servers [server_nbr];
                client_server_message (self, server);
            }
        }
    }
    client_destroy (&self);
}


//  --------------------------------------------------------------------------
//  Selftest

int
fmq_client_test (bool verbose)
{
    printf (" * fmq_client: ");

    //  @selftest
    fmq_client_t *self;
    //  Run selftest using 'client_test.cfg' configuration
    self = fmq_client_new ();
    assert (self);
    fmq_client_configure (self, "client_test.cfg");
    fmq_client_connect (self, "tcp://localhost:6001");
    zclock_sleep (1000);                              
    fmq_client_destroy (&self);

    //  @end

    printf ("OK\n");
    return 0;
}
