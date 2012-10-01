/*  =========================================================================
    fmq_client.c

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
//  Open connection to server

void
fmq_client_connect (fmq_client_t *self, const char *endpoint)
{
    zstr_sendm (self->pipe, "CONNECT");
    zstr_send  (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------

void
fmq_client_subscribe (fmq_client_t *self, const char *path)
{
    assert (self);
    assert (path);
    zstr_sendm (self->pipe, "SUBSCRIBE");
    zstr_send (self->pipe, path);
}


//  ---------------------------------------------------------------------
//  State machine constants

typedef enum {
    start_state = 1,
    requesting_access_state = 2,
    subscribing_state = 3,
    ready_state = 4
} state_t;

typedef enum {
    terminate_event = -1,
    ready_event = 1,
    srsly_event = 2,
    rtfm_event = 3,
    _other_event = 4,
    orly_event = 5,
    ohai_ok_event = 6,
    ok_event = 7,
    finished_event = 8,
    cheezburger_event = 9,
    hugz_event = 10,
    subscribe_event = 11,
    send_credit_event = 12,
    icanhaz_ok_event = 13
} event_t;


//  Forward declarations
typedef struct _client_t client_t;

//  There's no point making these configurable
#define CREDIT_SLICE    1000000
#define CREDIT_MINIMUM  (CREDIT_SLICE * 4) + 1

//  Subscription in memory
typedef struct {
    char *path;                 //  Path we subscribe to
} sub_t;

static sub_t *
sub_new (client_t *client, char *path)
{
    sub_t *self = (sub_t *) zmalloc (sizeof (sub_t));
    self->path = strdup (path);
    return self;
}

static void
sub_destroy (sub_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sub_t *self = *self_p;
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}


//  ---------------------------------------------------------------------
//  Context for the client thread

struct _client_t {
    //  Properties accessible to client actions
    event_t next_event;         //  Next event
    bool connected;             //  Are we connected to server?
    zlist_t *subs;              //  Subscriptions              
    size_t credit;              //  Current credit pending     
    fmq_file_t *file;           //  File we're writing to      
    
    //  Properties you should NOT touch
    zctx_t *ctx;                //  Own CZMQ context
    void *pipe;                 //  Socket to back to caller
    void *dealer;               //  Socket to talk to server
    bool stopped;               //  Has client stopped?
    fmq_config_t *config;       //  Configuration tree
    state_t state;              //  Current state
    event_t event;              //  Current event
    fmq_msg_t *request;         //  Next message to send
    fmq_msg_t *reply;           //  Last received reply
    int heartbeat;              //  Heartbeat interval
    int64_t expires_at;         //  Server expires at
};

static client_t *
client_new (zctx_t *ctx, void *pipe)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
    self->ctx = ctx;
    self->pipe = pipe;
    self->config = fmq_config_new ("root", NULL);
    self->subs = zlist_new ();
    self->connected = false;  
    return self;
}

static void
client_destroy (client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        client_t *self = *self_p;
        fmq_config_destroy (&self->config);
        fmq_msg_destroy (&self->request);
        fmq_msg_destroy (&self->reply);
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
    //  Get standard client configuration
    self->heartbeat = atoi (
        fmq_config_resolve (self->config, "client/heartbeat", "1")) * 1000;

    //  Apply echo commands and class methods
    fmq_config_t *section = fmq_config_child (self->config);
    while (section) {
        fmq_config_t *entry = fmq_config_child (section);
        while (entry) {
            if (streq (fmq_config_name (entry), "echo"))
                puts (fmq_config_value (entry));
            entry = fmq_config_next (entry);
        }
        if (streq (fmq_config_name (section), "subscribe")) {
            char *path = fmq_config_resolve (section, "path", "?");
            //  Store subscription along with any previous ones         
            //  Check we don't already have a subscription for this path
            sub_t *sub = (sub_t *) zlist_first (self->subs);            
            while (sub) {                                               
                if (streq (path, sub->path))                            
                    return;                                             
                sub = (sub_t *) zlist_next (self->subs);                
            }                                                           
            //  Subscription path must start with '/'                   
            //  We'll do better error handling later                    
            assert (*path == '/');                                      
                                                                        
            //  New subscription, so store it for later replay          
            sub = sub_new (self, path);                                 
            zlist_append (self->subs, sub);                             
                                                                        
            //  If we're connected, then also send to server            
            if (self->connected) {                                      
                fmq_msg_path_set (self->request, path);                 
                self->next_event = subscribe_event;                     
            }                                                           
        }
        section = fmq_config_next (section);
    }
}

//  Custom actions for state machine

static void
initialize_the_client (client_t *self)
{
    self->next_event = ready_event;
}

static void
try_security_mechanism (client_t *self)
{
    char *login = fmq_config_resolve (self->config, "security/plain/login", "guest"); 
    char *password = fmq_config_resolve (self->config, "security/plain/password", "");
    zframe_t *frame = fmq_sasl_plain_encode (login, password);                        
    fmq_msg_mechanism_set (self->request, "PLAIN");                                   
    fmq_msg_response_set  (self->request, frame);                                     
}

static void
connected_to_server (client_t *self)
{
    self->connected = true;
}

static void
get_first_subscription (client_t *self)
{
    sub_t *sub = (sub_t *) zlist_first (self->subs);
    if (sub) {                                      
        fmq_msg_path_set (self->request, sub->path);
        self->next_event = ok_event;                
    }                                               
    else                                            
        self->next_event = finished_event;          
}

static void
get_next_subscription (client_t *self)
{
    sub_t *sub = (sub_t *) zlist_next (self->subs); 
    if (sub) {                                      
        fmq_msg_path_set (self->request, sub->path);
        self->next_event = ok_event;                
    }                                               
    else                                            
        self->next_event = finished_event;          
}

static void
refill_credit_as_needed (client_t *self)
{
    //  If credit has fallen too low, send more credit     
    size_t credit_to_send = 0;                             
    while (self->credit < CREDIT_MINIMUM) {                
        credit_to_send += CREDIT_SLICE;                    
        self->credit += CREDIT_SLICE;                      
    }                                                      
    if (credit_to_send) {                                  
        fmq_msg_credit_set (self->request, credit_to_send);
        self->next_event = send_credit_event;              
    }                                                      
}

static void
process_the_patch (client_t *self)
{
    char *rootdir = fmq_config_resolve (self->config, "client/root", "fmqroot");      
    char *filename = fmq_msg_filename (self->reply);                                  
                                                                                      
    if (fmq_msg_operation (self->reply) == FMQ_MSG_FILE_CREATE) {                     
        if (self->file == NULL) {                                                     
            zclock_log ("I: create %s", filename);                                    
            self->file = fmq_file_new (rootdir, filename);                            
            if (fmq_file_output (self->file)) {                                       
                //  File not writeable, skip patch                                    
                fmq_file_destroy (&self->file);                                       
                return;                                                               
            }                                                                         
        }                                                                             
        //  Try to write, ignore errors in this version                               
        zframe_t *frame = fmq_msg_chunk (self->reply);                                
        fmq_chunk_t *chunk = fmq_chunk_new (zframe_data (frame), zframe_size (frame));
        if (fmq_chunk_size (chunk) > 0) {                                             
            fmq_file_write (self->file, chunk, fmq_msg_offset (self->reply));         
            self->credit -= fmq_chunk_size (chunk);                                   
        }                                                                             
        else                                                                          
            //  Zero-sized chunk means end of file                                    
            fmq_file_destroy (&self->file);                                           
        fmq_chunk_destroy (&chunk);                                                   
    }                                                                                 
    else                                                                              
    if (fmq_msg_operation (self->reply) == FMQ_MSG_FILE_DELETE) {                     
        zclock_log ("I: delete %s", filename);                                        
        fmq_file_t *file = fmq_file_new (rootdir, filename);                          
        fmq_file_remove (file);                                                       
        fmq_file_destroy (&file);                                                     
    }                                                                                 
}

static void
log_access_denied (client_t *self)
{
    puts ("W: server denied us access, retrying...");
}

static void
log_invalid_message (client_t *self)
{
    puts ("E: server claims we sent an invalid message");
}

static void
log_protocol_error (client_t *self)
{
    puts ("E: protocol error");
}

static void
terminate_the_client (client_t *self)
{
    self->connected = false;           
    self->next_event = terminate_event;
}

//  Execute state machine as long as we have events

static void
client_execute (client_t *self, int event)
{
    self->next_event = event;
    while (self->next_event) {
        self->event = self->next_event;
        self->next_event = 0;
        switch (self->state) {
            case start_state:
                if (self->event == ready_event) {
                    fmq_msg_id_set (self->request, FMQ_MSG_OHAI);
                    fmq_msg_send (&self->request, self->dealer);
                    self->request = fmq_msg_new (0);
                    self->state = requesting_access_state;
                }
                else
                if (self->event == srsly_event) {
                    log_access_denied (self);
                    terminate_the_client (self);
                    self->state = start_state;
                }
                else
                if (self->event == rtfm_event) {
                    log_invalid_message (self);
                    terminate_the_client (self);
                }
                else {
                    log_protocol_error (self);
                    terminate_the_client (self);
                }
                break;

            case requesting_access_state:
                if (self->event == orly_event) {
                    try_security_mechanism (self);
                    fmq_msg_id_set (self->request, FMQ_MSG_YARLY);
                    fmq_msg_send (&self->request, self->dealer);
                    self->request = fmq_msg_new (0);
                    self->state = requesting_access_state;
                }
                else
                if (self->event == ohai_ok_event) {
                    connected_to_server (self);
                    get_first_subscription (self);
                    self->state = subscribing_state;
                }
                else
                if (self->event == srsly_event) {
                    log_access_denied (self);
                    terminate_the_client (self);
                    self->state = start_state;
                }
                else
                if (self->event == rtfm_event) {
                    log_invalid_message (self);
                    terminate_the_client (self);
                }
                else {
                    log_protocol_error (self);
                    terminate_the_client (self);
                }
                break;

            case subscribing_state:
                if (self->event == ok_event) {
                    fmq_msg_id_set (self->request, FMQ_MSG_ICANHAZ);
                    fmq_msg_send (&self->request, self->dealer);
                    self->request = fmq_msg_new (0);
                    get_next_subscription (self);
                    self->state = subscribing_state;
                }
                else
                if (self->event == finished_event) {
                    refill_credit_as_needed (self);
                    self->state = ready_state;
                }
                else
                if (self->event == srsly_event) {
                    log_access_denied (self);
                    terminate_the_client (self);
                    self->state = start_state;
                }
                else
                if (self->event == rtfm_event) {
                    log_invalid_message (self);
                    terminate_the_client (self);
                }
                else {
                    log_protocol_error (self);
                    terminate_the_client (self);
                }
                break;

            case ready_state:
                if (self->event == cheezburger_event) {
                    process_the_patch (self);
                    refill_credit_as_needed (self);
                }
                else
                if (self->event == hugz_event) {
                    fmq_msg_id_set (self->request, FMQ_MSG_HUGZ_OK);
                    fmq_msg_send (&self->request, self->dealer);
                    self->request = fmq_msg_new (0);
                }
                else
                if (self->event == subscribe_event) {
                    fmq_msg_id_set (self->request, FMQ_MSG_ICANHAZ);
                    fmq_msg_send (&self->request, self->dealer);
                    self->request = fmq_msg_new (0);
                }
                else
                if (self->event == send_credit_event) {
                    fmq_msg_id_set (self->request, FMQ_MSG_NOM);
                    fmq_msg_send (&self->request, self->dealer);
                    self->request = fmq_msg_new (0);
                }
                else
                if (self->event == icanhaz_ok_event) {
                }
                else
                if (self->event == srsly_event) {
                    log_access_denied (self);
                    terminate_the_client (self);
                    self->state = start_state;
                }
                else
                if (self->event == rtfm_event) {
                    log_invalid_message (self);
                    terminate_the_client (self);
                }
                else {
                    log_protocol_error (self);
                    terminate_the_client (self);
                }
                break;

        }
        if (self->next_event == terminate_event) {
            self->stopped = true;
            break;
        }
    }
}

//  Restart client dialog from zero

static void
client_restart (client_t *self, char *endpoint)
{
    //  Reconnect to new endpoint if specified
    if (endpoint)  {
        if (self->dealer)
            zsocket_destroy (self->ctx, self->dealer);
        self->dealer = zsocket_new (self->ctx, ZMQ_DEALER);
        zmq_connect (self->dealer, endpoint);
    }
    //  Clear out any previous request data
    fmq_msg_destroy (&self->request);
    self->request = fmq_msg_new (0);

    //  Restart dialog state machine from zero
    self->state = start_state;
    self->expires_at = 0;

    //  Application hook to reinitialize dialog
    //  Provides us with an event to kick things off
    initialize_the_client (self);
    client_execute (self, self->next_event);
}

static void
control_message (client_t *self)
{
    zmsg_t *msg = zmsg_recv (self->pipe);
    char *method = zmsg_popstr (msg);
    if (streq (method, "SUBSCRIBE")) {
        char *path = zmsg_popstr (msg);
        //  Store subscription along with any previous ones         
        //  Check we don't already have a subscription for this path
        sub_t *sub = (sub_t *) zlist_first (self->subs);            
        while (sub) {                                               
            if (streq (path, sub->path))                            
                return;                                             
            sub = (sub_t *) zlist_next (self->subs);                
        }                                                           
        //  Subscription path must start with '/'                   
        //  We'll do better error handling later                    
        assert (*path == '/');                                      
                                                                    
        //  New subscription, so store it for later replay          
        sub = sub_new (self, path);                                 
        zlist_append (self->subs, sub);                             
                                                                    
        //  If we're connected, then also send to server            
        if (self->connected) {                                      
            fmq_msg_path_set (self->request, path);                 
            self->next_event = subscribe_event;                     
        }                                                           
        free (path);
    }
    else
    if (streq (method, "CONNECT")) {
        char *endpoint = zmsg_popstr (msg);
        client_restart (self, endpoint);
        free (endpoint);
    }
    else
    if (streq (method, "CONFIG")) {
        char *config_file = zmsg_popstr (msg);
        fmq_config_destroy (&self->config);
        self->config = fmq_config_load (config_file);
        if (self->config)
            client_apply_config (self);
        else {
            printf ("E: cannot load config file '%s'\n", config_file);
            self->config = fmq_config_new ("root", NULL);
        }
        free (config_file);
    }
    else
    if (streq (method, "SETOPTION")) {
        char *path = zmsg_popstr (msg);
        char *value = zmsg_popstr (msg);
        fmq_config_path_set (self->config, path, value);
        free (path);
        free (value);
    }
    else
    if (streq (method, "STOP")) {
        zstr_send (self->pipe, "OK");
        self->stopped = true;
    }
    free (method);
    zmsg_destroy (&msg);

    if (self->next_event)
        client_execute (self, self->next_event);
}

static void
server_message (client_t *self)
{
    if (self->reply)
        fmq_msg_destroy (&self->reply);
    self->reply = fmq_msg_recv (self->dealer);
    if (!self->reply)
        return;         //  Interrupted; do nothing
    
    if (fmq_msg_id (self->reply) == FMQ_MSG_SRSLY)
        client_execute (self, srsly_event);
    else
    if (fmq_msg_id (self->reply) == FMQ_MSG_RTFM)
        client_execute (self, rtfm_event);
    else
    if (fmq_msg_id (self->reply) == FMQ_MSG_ORLY)
        client_execute (self, orly_event);
    else
    if (fmq_msg_id (self->reply) == FMQ_MSG_OHAI_OK)
        client_execute (self, ohai_ok_event);
    else
    if (fmq_msg_id (self->reply) == FMQ_MSG_CHEEZBURGER)
        client_execute (self, cheezburger_event);
    else
    if (fmq_msg_id (self->reply) == FMQ_MSG_HUGZ)
        client_execute (self, hugz_event);
    else
    if (fmq_msg_id (self->reply) == FMQ_MSG_ICANHAZ_OK)
        client_execute (self, icanhaz_ok_event);

    //  Any input from server counts as activity
    self->expires_at = zclock_time () + self->heartbeat * 2;
}


//  Finally here's the client thread itself, which polls its two
//  sockets and processes incoming messages

static void
client_thread (void *args, zctx_t *ctx, void *pipe)
{
    client_t *self = client_new (ctx, pipe);
    while (!self->stopped) {
        //  Build structure each time since self->dealer can change
        zmq_pollitem_t items [] = {
            { self->pipe, 0, ZMQ_POLLIN, 0 },
            { self->dealer, 0, ZMQ_POLLIN, 0 }
        };
        int poll_size = self->dealer? 2: 1;
        if (zmq_poll (items, poll_size, self->heartbeat * ZMQ_POLL_MSEC) == -1)
            break;              //  Context has been shut down

        //  Process incoming messages; either of these can
        //  throw events into the state machine
        if (items [0].revents & ZMQ_POLLIN)
            control_message (self);

        if (items [1].revents & ZMQ_POLLIN)
            server_message (self);

        //  Check whether server seems dead
        if (self->expires_at && zclock_time () >= self->expires_at)
            client_restart (self, NULL);
    }
    client_destroy (&self);
}


//  --------------------------------------------------------------------------
//  Selftest

int
fmq_client_test (bool verbose)
{
    printf (" * fmq_client: ");

    fmq_client_t *self;
    //  Run selftest using 'client_test.cfg' configuration
    self = fmq_client_new ();
    assert (self);
    fmq_client_configure (self, "client_test.cfg");
    fmq_client_connect (self, "tcp://localhost:6001");
    sleep (1);                                        
    fmq_client_destroy (&self);

    printf ("OK\n");
    return 0;
}
