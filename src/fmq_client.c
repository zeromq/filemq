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
//  Open connection to server

void
fmq_client_connect (fmq_client_t *self, const char *endpoint)
{
    zstr_sendm (self->pipe, "CONNECT");
    zstr_send  (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------

void
fmq_client_subscribe (fmq_client_t *self, const char *virtual, const char *local)
{
    assert (self);
    assert (virtual);
    assert (local);
    zstr_sendm (self->pipe, "SUBSCRIBE");
    zstr_sendm (self->pipe, virtual);
    zstr_send (self->pipe, local);
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
    icanhaz_ok_event = 11
} event_t;

//  Names for animation
static char *
s_state_name [] = {
    "",
    "Start",
    "Requesting Access",
    "Subscribing",
    "Ready"
};

static char *
s_event_name [] = {
    "",
    "ready",
    "SRSLY",
    "RTFM",
    "$other",
    "ORLY",
    "OHAI-OK",
    "ok",
    "finished",
    "CHEEZBURGER",
    "HUGZ",
    "ICANHAZ-OK"
};


//  Forward declarations
typedef struct _client_t client_t;





//  ---------------------------------------------------------------------
//  Context for the client thread

struct _client_t {
    //  Properties accessible to client actions
    event_t next_event;         //  Next event
    
    //  Properties you should NOT touch
    zctx_t *ctx;                //  Own CZMQ context
    void *pipe;                 //  Socket to back to caller
    void *dealer;               //  Socket to talk to server
    bool ready;                 //  Client connected
    bool stopped;               //  Has client stopped?
    fmq_config_t *config;       //  Configuration tree
    state_t state;              //  Current state
    event_t event;              //  Current event
    fmq_msg_t *request;         //  Next message to send
    fmq_msg_t *reply;           //  Last received reply
};

static client_t *
client_new (zctx_t *ctx, void *pipe)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
    self->ctx = ctx;
    self->pipe = pipe;
    self->config = fmq_config_new ("root", NULL);
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
            char *virtual = fmq_config_resolve (section, "virtual", "?");
            char *local = fmq_config_resolve (section, "local", "?");
            printf ("SUBSCRIBE: %s -> %s\n", virtual, local);
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
        fmq_msg_response_set (self->request, frame);                                      
}

static void
get_next_subscription (client_t *self)
{
        self->next_event = finished_event;
}

static void
refill_pipeline (client_t *self)
{
    
}

static void
store_file_data (client_t *self)
{
    
}

static void
log_access_denied (client_t *self)
{
    
}

static void
log_invalid_message (client_t *self)
{
    
}

static void
log_protocol_error (client_t *self)
{
    
}

static void
terminate_the_client (client_t *self)
{
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
        zclock_log ("C: %s:", s_state_name [self->state]);
        zclock_log ("C: (%s)", s_event_name [self->event]);
        switch (self->state) {
            case start_state:
                if (self->event == ready_event) {
                    zclock_log ("C:    + send OHAI");
                    fmq_msg_id_set (self->request, FMQ_MSG_OHAI);
                    self->state = requesting_access_state;
                }
                else
                if (self->event == srsly_event) {
                    zclock_log ("C:    + log access denied");
                    log_access_denied (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else
                if (self->event == rtfm_event) {
                    zclock_log ("C:    + log invalid message");
                    log_invalid_message (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else {
                    zclock_log ("C:    + log protocol error");
                    log_protocol_error (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                break;

            case requesting_access_state:
                if (self->event == orly_event) {
                    zclock_log ("C:    + try security mechanism");
                    try_security_mechanism (self);
                    zclock_log ("C:    + send YARLY");
                    fmq_msg_id_set (self->request, FMQ_MSG_YARLY);
                    self->state = requesting_access_state;
                }
                else
                if (self->event == ohai_ok_event) {
                    zclock_log ("C:    + get next subscription");
                    get_next_subscription (self);
                    self->state = subscribing_state;
                }
                else
                if (self->event == srsly_event) {
                    zclock_log ("C:    + log access denied");
                    log_access_denied (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else
                if (self->event == rtfm_event) {
                    zclock_log ("C:    + log invalid message");
                    log_invalid_message (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else {
                    zclock_log ("C:    + log protocol error");
                    log_protocol_error (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                break;

            case subscribing_state:
                if (self->event == ok_event) {
                    zclock_log ("C:    + send ICANHAZ");
                    fmq_msg_id_set (self->request, FMQ_MSG_ICANHAZ);
                    zclock_log ("C:    + get next subscription");
                    get_next_subscription (self);
                    self->state = subscribing_state;
                }
                else
                if (self->event == finished_event) {
                    zclock_log ("C:    + refill pipeline");
                    refill_pipeline (self);
                    zclock_log ("C:    + send NOM");
                    fmq_msg_id_set (self->request, FMQ_MSG_NOM);
                    self->state = ready_state;
                }
                else
                if (self->event == srsly_event) {
                    zclock_log ("C:    + log access denied");
                    log_access_denied (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else
                if (self->event == rtfm_event) {
                    zclock_log ("C:    + log invalid message");
                    log_invalid_message (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else {
                    zclock_log ("C:    + log protocol error");
                    log_protocol_error (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                break;

            case ready_state:
                if (self->event == cheezburger_event) {
                    zclock_log ("C:    + store file data");
                    store_file_data (self);
                    zclock_log ("C:    + refill pipeline");
                    refill_pipeline (self);
                    zclock_log ("C:    + send NOM");
                    fmq_msg_id_set (self->request, FMQ_MSG_NOM);
                }
                else
                if (self->event == hugz_event) {
                    zclock_log ("C:    + send HUGZ_OK");
                    fmq_msg_id_set (self->request, FMQ_MSG_HUGZ_OK);
                }
                else
                if (self->event == icanhaz_ok_event) {
                }
                else
                if (self->event == srsly_event) {
                    zclock_log ("C:    + log access denied");
                    log_access_denied (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else
                if (self->event == rtfm_event) {
                    zclock_log ("C:    + log invalid message");
                    log_invalid_message (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                else {
                    zclock_log ("C:    + log protocol error");
                    log_protocol_error (self);
                    zclock_log ("C:    + terminate the client");
                    terminate_the_client (self);
                }
                break;

        }
        zclock_log ("C:      -------------------> %s", s_state_name [self->state]);

        if (fmq_msg_id (self->request)) {
            puts ("Send request to server");
            fmq_msg_dump (self->request);
            fmq_msg_send (&self->request, self->dealer);
            self->request = fmq_msg_new (0);
        }
        if (self->next_event == terminate_event) {
            self->stopped = true;
            break;
        }
    }
}

static void
server_message (client_t *self)
{
    if (self->reply)
        fmq_msg_destroy (&self->reply);
    self->reply = fmq_msg_recv (self->dealer);
    if (!self->reply)
        return;         //  Interrupted; do nothing

    puts ("Received reply from server");
    fmq_msg_dump (self->reply);
    
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
}

//  Restart client dialog from zero

static void
client_restart (client_t *self, char *endpoint)
{
    //  Free dialog-specific properties
    if (self->dealer)
        zsocket_destroy (self->ctx, self->dealer);
    fmq_msg_destroy (&self->request);

    //  Restart dialog state machine from zero
    self->dealer = zsocket_new (self->ctx, ZMQ_DEALER);
    zmq_connect (self->dealer, endpoint);
    self->state = start_state;
    self->request = fmq_msg_new (0);

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
        char *virtual = zmsg_popstr (msg);
        char *local = zmsg_popstr (msg);
        printf ("SUBSCRIBE: %s -> %s\n", virtual, local);
        free (virtual);
        free (local);
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
    if (streq (method, "STOP")) {
        zstr_send (self->pipe, "OK");
        self->stopped = true;
        self->ready = true;
    }
    free (method);
    zmsg_destroy (&msg);
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
        if (zmq_poll (items, poll_size, 1000 * ZMQ_POLL_MSEC) == -1)
            break;              //  Context has been shut down

        //  Process incoming message from either socket
        if (items [0].revents & ZMQ_POLLIN)
            control_message (self);

        if (items [1].revents & ZMQ_POLLIN)
            server_message (self);
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
