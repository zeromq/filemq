/*  =========================================================================
    fmq_client_engine - FILEMQ Client engine

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: fmq_client.xml, or
     * The code generation script that built this file: zproto_client_c
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of FileMQ, a C implemenation of the protocol:    
    https://github.com/danriegsecker/filemq2.                          
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/


//  ---------------------------------------------------------------------------
//  State machine constants

typedef enum {
    start_state = 1,
    connecting_state = 2,
    connected_state = 3,
    subscribing_state = 4,
    subscribed_state = 5,
    defaults_state = 6
} state_t;

typedef enum {
    NULL_event = 0,
    connect_event = 1,
    connect_error_event = 2,
    ohai_ok_event = 3,
    expired_event = 4,
    set_inbox_event = 5,
    subscribe_event = 6,
    destructor_event = 7,
    subscribe_error_event = 8,
    icanhaz_ok_event = 9,
    send_credit_event = 10,
    cheezburger_event = 11,
    finished_event = 12,
    srsly_event = 13,
    rtfm_event = 14,
    hugz_ok_event = 15,
    bombcmd_event = 16,
    bombmsg_event = 17
} event_t;

//  Names for state machine logging and error reporting
static char *
s_state_name [] = {
    "(NONE)",
    "start",
    "connecting",
    "connected",
    "subscribing",
    "subscribed",
    "defaults"
};

static char *
s_event_name [] = {
    "(NONE)",
    "connect",
    "connect_error",
    "OHAI_OK",
    "expired",
    "set_inbox",
    "subscribe",
    "destructor",
    "subscribe_error",
    "ICANHAZ_OK",
    "send_credit",
    "CHEEZBURGER",
    "finished",
    "SRSLY",
    "RTFM",
    "HUGZ_OK",
    "bombcmd",
    "bombmsg"
};


//  ---------------------------------------------------------------------------
//  Context for the client. This embeds the application-level client context
//  at its start (the entire structure, not a reference), so we can cast a
//  pointer between client_t and s_client_t arbitrarily.

//  These are the different method arguments we manage automatically
struct _client_args_t {
    char *endpoint;
    uint32_t timeout;
    char *path;
};

typedef struct {
    client_t client;            //  Application-level client context
    zsock_t *cmdpipe;           //  Get/send commands from caller API
    zsock_t *msgpipe;           //  Get/send messages from caller API
    zsock_t *dealer;            //  Socket to talk to server
    zloop_t *loop;              //  Listen to pipe and dealer
    fmq_msg_t *message;         //  Message received or sent
    client_args_t args;         //  Method arguments structure
    bool terminated;            //  True if client is shutdown
    bool fsm_stopped;           //  "terminate" action called
    size_t timeout;             //  inactivity timeout, msecs
    state_t state;              //  Current state
    event_t event;              //  Current event
    event_t next_event;         //  The next event
    event_t exception;          //  Exception event, if any
    int expiry_timer;           //  zloop timer for timeouts
    int wakeup_timer;           //  zloop timer for alarms
    event_t wakeup_event;       //  Wake up with this event
} s_client_t;

static int
    client_initialize (client_t *self);
static void
    client_terminate (client_t *self);
static void
    s_client_destroy (s_client_t **self_p);
static void
    s_client_execute (s_client_t *self, event_t event);
static int
    s_client_handle_wakeup (zloop_t *loop, int timer_id, void *argument);
static int
    s_client_handle_timeout (zloop_t *loop, int timer_id, void *argument);
static void
    s_satisfy_pedantic_compilers (void);
static void
    connect_to_server_endpoint (client_t *self);
static void
    use_connect_timeout (client_t *self);
static void
    handle_connect_error (client_t *self);
static void
    stayin_alive (client_t *self);
static void
    connected_to_server (client_t *self);
static void
    handle_connect_timeout (client_t *self);
static void
    setup_inbox (client_t *self);
static void
    format_icanhaz_command (client_t *self);
static void
    signal_success (client_t *self);
static void
    subscribe_failed (client_t *self);
static void
    handle_connected_timeout (client_t *self);
static void
    signal_subscribe_success (client_t *self);
static void
    handle_subscribe_timeout (client_t *self);
static void
    process_the_patch (client_t *self);
static void
    refill_credit_as_needed (client_t *self);
static void
    log_access_denied (client_t *self);
static void
    log_invalid_message (client_t *self);
static void
    log_protocol_error (client_t *self);
static void
    sync_server_not_present (client_t *self);
static void
    async_server_not_present (client_t *self);

//  Global tracing/animation indicator; we can't use a client method as
//  that only works after construction (which we often want to trace).
volatile int fmq_client_verbose = false;

//  Create a new client connection

static s_client_t *
s_client_new (zsock_t *cmdpipe, zsock_t *msgpipe)
{
    s_client_t *self = (s_client_t *) zmalloc (sizeof (s_client_t));
    if (self) {
        assert ((s_client_t *) &self->client == self);
        self->cmdpipe = cmdpipe;
        self->msgpipe = msgpipe;
        self->dealer = zsock_new (ZMQ_DEALER);
        if (self->dealer)
            self->message = fmq_msg_new ();
        if (self->message)
            self->loop = zloop_new ();
        if (self->loop) {
            //  Give application chance to initialize and set next event
            self->state = start_state;
            self->event = NULL_event;
            self->client.cmdpipe = self->cmdpipe;
            self->client.msgpipe = self->msgpipe;
            self->client.dealer = self->dealer;
            self->client.message = self->message;
            self->client.args = &self->args;
            if (client_initialize (&self->client))
                s_client_destroy (&self);
        }
        else
            s_client_destroy (&self);
    }
    s_satisfy_pedantic_compilers ();
    return self;
}

//  Destroy the client connection

static void
s_client_destroy (s_client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        s_client_t *self = *self_p;
        zstr_free (&self->args.endpoint);
        zstr_free (&self->args.path);
        client_terminate (&self->client);
        fmq_msg_destroy (&self->message);
        zsock_destroy (&self->msgpipe);
        zsock_destroy (&self->dealer);
        zloop_destroy (&self->loop);
        free (self);
        *self_p = NULL;
    }
}

//  ---------------------------------------------------------------------------
//  These methods are an internal API for actions

//  Set the next event, needed in at least one action in an internal
//  state; otherwise the state machine will wait for a message on the
//  dealer socket and treat that as the event.

static void
engine_set_next_event (client_t *client, event_t event)
{
    if (client) {
        s_client_t *self = (s_client_t *) client;
        self->next_event = event;
    }
}

//  Raise an exception with 'event', halting any actions in progress.
//  Continues execution of actions defined for the exception event.

static void
engine_set_exception (client_t *client, event_t event)
{
    if (client) {
        s_client_t *self = (s_client_t *) client;
        self->exception = event;
    }
}

//  Set wakeup alarm after 'delay' msecs. The next state should handle the
//  wakeup event. The alarm is cancelled on any other event.

static void
engine_set_wakeup_event (client_t *client, size_t delay, event_t event)
{
    if (client) {
        s_client_t *self = (s_client_t *) client;
        if (self->wakeup_timer) {
            zloop_timer_end (self->loop, self->wakeup_timer);
            self->wakeup_timer = 0;
        }
        self->wakeup_timer = zloop_timer (
            self->loop, delay, 1, s_client_handle_wakeup, self);
        self->wakeup_event = event;
    }
}

//  Set timeout for next protocol read. By default, will wait forever
//  or until the process is interrupted. The timeout is in milliseconds.
//  The state machine must handle the "expired" event.

static void
engine_set_timeout (client_t *client, size_t timeout)
{
    if (client) {
        s_client_t *self = (s_client_t *) client;
        self->timeout = timeout;
        if (self->timeout)
            self->expiry_timer = zloop_timer (
                self->loop, self->timeout, 1, s_client_handle_timeout, self);
    }
}

//  Poll socket for activity, invoke handler on any received message.
//  Handler must be a CZMQ zloop_fn function; receives client as arg.

static void
engine_handle_socket (client_t *client, zsock_t *sock, zloop_reader_fn handler)
{
    if (client && sock) {
        s_client_t *self = (s_client_t *) client;
        if (handler != NULL) {
            int rc = zloop_reader (self->loop, sock, handler, self);
            assert (rc == 0);
            zloop_reader_set_tolerant (self->loop, sock);
        }
        else
            zloop_reader_end (self->loop, sock);
    }
}

//  Pedantic compilers don't like unused functions, so we call the whole
//  API, passing null references. It's nasty and horrid and sufficient.

static void
s_satisfy_pedantic_compilers (void)
{
    engine_set_next_event (NULL, NULL_event);
    engine_set_exception (NULL, NULL_event);
    engine_set_timeout (NULL, 0);
    engine_set_wakeup_event (NULL, 0, NULL_event);
    engine_handle_socket (NULL, 0, NULL);
}


//  ---------------------------------------------------------------------------
//  Generic methods on protocol messages
//  TODO: replace with lookup table, since ID is one byte

static event_t
s_protocol_event (s_client_t *self, fmq_msg_t *message)
{
    assert (message);
    switch (fmq_msg_id (message)) {
        case FMQ_MSG_OHAI_OK:
            return ohai_ok_event;
            break;
        case FMQ_MSG_ICANHAZ_OK:
            return icanhaz_ok_event;
            break;
        case FMQ_MSG_CHEEZBURGER:
            return cheezburger_event;
            break;
        case FMQ_MSG_HUGZ_OK:
            return hugz_ok_event;
            break;
        case FMQ_MSG_SRSLY:
            return srsly_event;
            break;
        case FMQ_MSG_RTFM:
            return rtfm_event;
            break;
        default:
            zsys_error ("fmq_client: unknown command %s, halting", fmq_msg_command (message));
            self->terminated = true;
            return NULL_event;
    }
}


//  Execute state machine as long as we have events; if event is NULL_event,
//  or state machine is stopped, do nothing.

static void
s_client_execute (s_client_t *self, event_t event)
{
    self->next_event = event;
    //  Cancel wakeup timer, if any was pending
    if (self->wakeup_timer) {
        zloop_timer_end (self->loop, self->wakeup_timer);
        self->wakeup_timer = 0;
    }
    while (!self->terminated                    //  Actor is dying
        && !self->fsm_stopped                   //  FSM has finished
        && self->next_event != NULL_event) {
        self->event = self->next_event;
        self->next_event = NULL_event;
        self->exception = NULL_event;
        if (fmq_client_verbose) {
            zsys_debug ("fmq_client: %s:", s_state_name [self->state]);
            zsys_debug ("fmq_client:        %s", s_event_name [self->event]);
        }
        switch (self->state) {
            case start_state:
                if (self->event == connect_event) {
                    if (!self->exception) {
                        //  connect to server endpoint
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ connect to server endpoint");
                        connect_to_server_endpoint (&self->client);
                    }
                    if (!self->exception) {
                        //  use connect timeout
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ use connect timeout");
                        use_connect_timeout (&self->client);
                    }
                    if (!self->exception) {
                        //  send OHAI
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send OHAI");
                        fmq_msg_set_id (self->message, FMQ_MSG_OHAI);
                        fmq_msg_send (self->message, self->dealer);
                    }
                    if (!self->exception)
                        self->state = connecting_state;
                }
                else
                if (self->event == connect_error_event) {
                    if (!self->exception) {
                        //  handle connect error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ handle connect error");
                        handle_connect_error (&self->client);
                    }
                }
                else
                if (self->event == srsly_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log access denied
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log access denied");
                        log_access_denied (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == rtfm_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log invalid message
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log invalid message");
                        log_invalid_message (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == hugz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                }
                else
                if (self->event == bombcmd_event) {
                    if (!self->exception) {
                        //  sync server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ sync server not present");
                        sync_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == bombmsg_event) {
                    if (!self->exception) {
                        //  async server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ async server not present");
                        async_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else {
                    //  Handle unexpected protocol events
                    if (!self->exception) {
                        //  log protocol error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log protocol error");
                        log_protocol_error (&self->client);
                    }
                }
                break;

            case connecting_state:
                if (self->event == ohai_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  connected to server
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ connected to server");
                        connected_to_server (&self->client);
                    }
                    if (!self->exception)
                        self->state = connected_state;
                }
                else
                if (self->event == expired_event) {
                    if (!self->exception) {
                        //  handle connect timeout
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ handle connect timeout");
                        handle_connect_timeout (&self->client);
                    }
                }
                else
                if (self->event == srsly_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log access denied
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log access denied");
                        log_access_denied (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == rtfm_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log invalid message
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log invalid message");
                        log_invalid_message (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == hugz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                }
                else
                if (self->event == bombcmd_event) {
                    if (!self->exception) {
                        //  sync server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ sync server not present");
                        sync_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == bombmsg_event) {
                    if (!self->exception) {
                        //  async server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ async server not present");
                        async_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else {
                    //  Handle unexpected protocol events
                    if (!self->exception) {
                        //  log protocol error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log protocol error");
                        log_protocol_error (&self->client);
                    }
                }
                break;

            case connected_state:
                if (self->event == set_inbox_event) {
                    if (!self->exception) {
                        //  setup inbox
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ setup inbox");
                        setup_inbox (&self->client);
                    }
                }
                else
                if (self->event == subscribe_event) {
                    if (!self->exception) {
                        //  format icanhaz command
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ format icanhaz command");
                        format_icanhaz_command (&self->client);
                    }
                    if (!self->exception) {
                        //  send ICANHAZ
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send ICANHAZ");
                        fmq_msg_set_id (self->message, FMQ_MSG_ICANHAZ);
                        fmq_msg_send (self->message, self->dealer);
                    }
                    if (!self->exception)
                        self->state = subscribing_state;
                }
                else
                if (self->event == destructor_event) {
                    if (!self->exception) {
                        //  send KTHXBAI
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send KTHXBAI");
                        fmq_msg_set_id (self->message, FMQ_MSG_KTHXBAI);
                        fmq_msg_send (self->message, self->dealer);
                    }
                    if (!self->exception) {
                        //  signal success
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ signal success");
                        signal_success (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == subscribe_error_event) {
                    if (!self->exception) {
                        //  subscribe failed
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ subscribe failed");
                        subscribe_failed (&self->client);
                    }
                }
                else
                if (self->event == expired_event) {
                    if (!self->exception) {
                        //  handle connected timeout
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ handle connected timeout");
                        handle_connected_timeout (&self->client);
                    }
                    if (!self->exception) {
                        //  send HUGZ
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send HUGZ");
                        fmq_msg_set_id (self->message, FMQ_MSG_HUGZ);
                        fmq_msg_send (self->message, self->dealer);
                    }
                }
                else
                if (self->event == srsly_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log access denied
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log access denied");
                        log_access_denied (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == rtfm_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log invalid message
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log invalid message");
                        log_invalid_message (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == hugz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                }
                else
                if (self->event == bombcmd_event) {
                    if (!self->exception) {
                        //  sync server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ sync server not present");
                        sync_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == bombmsg_event) {
                    if (!self->exception) {
                        //  async server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ async server not present");
                        async_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else {
                    //  Handle unexpected protocol events
                    if (!self->exception) {
                        //  log protocol error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log protocol error");
                        log_protocol_error (&self->client);
                    }
                }
                break;

            case subscribing_state:
                if (self->event == icanhaz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  signal subscribe success
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ signal subscribe success");
                        signal_subscribe_success (&self->client);
                    }
                    if (!self->exception)
                        self->state = subscribed_state;
                }
                else
                if (self->event == expired_event) {
                    if (!self->exception) {
                        //  handle subscribe timeout
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ handle subscribe timeout");
                        handle_subscribe_timeout (&self->client);
                    }
                }
                else
                if (self->event == srsly_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log access denied
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log access denied");
                        log_access_denied (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == rtfm_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log invalid message
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log invalid message");
                        log_invalid_message (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == hugz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                }
                else
                if (self->event == bombcmd_event) {
                    if (!self->exception) {
                        //  sync server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ sync server not present");
                        sync_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == bombmsg_event) {
                    if (!self->exception) {
                        //  async server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ async server not present");
                        async_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else {
                    //  Handle unexpected protocol events
                    if (!self->exception) {
                        //  log protocol error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log protocol error");
                        log_protocol_error (&self->client);
                    }
                }
                break;

            case subscribed_state:
                if (self->event == send_credit_event) {
                    if (!self->exception) {
                        //  send NOM
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send NOM");
                        fmq_msg_set_id (self->message, FMQ_MSG_NOM);
                        fmq_msg_send (self->message, self->dealer);
                    }
                }
                else
                if (self->event == cheezburger_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  process the patch
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ process the patch");
                        process_the_patch (&self->client);
                    }
                    if (!self->exception) {
                        //  refill credit as needed
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ refill credit as needed");
                        refill_credit_as_needed (&self->client);
                    }
                }
                else
                if (self->event == finished_event) {
                    if (!self->exception) {
                        //  refill credit as needed
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ refill credit as needed");
                        refill_credit_as_needed (&self->client);
                    }
                }
                else
                if (self->event == destructor_event) {
                    if (!self->exception) {
                        //  send KTHXBAI
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send KTHXBAI");
                        fmq_msg_set_id (self->message, FMQ_MSG_KTHXBAI);
                        fmq_msg_send (self->message, self->dealer);
                    }
                    if (!self->exception) {
                        //  signal success
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ signal success");
                        signal_success (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == expired_event) {
                    if (!self->exception) {
                        //  handle connected timeout
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ handle connected timeout");
                        handle_connected_timeout (&self->client);
                    }
                    if (!self->exception) {
                        //  send HUGZ
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ send HUGZ");
                        fmq_msg_set_id (self->message, FMQ_MSG_HUGZ);
                        fmq_msg_send (self->message, self->dealer);
                    }
                }
                else
                if (self->event == srsly_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log access denied
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log access denied");
                        log_access_denied (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == rtfm_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log invalid message
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log invalid message");
                        log_invalid_message (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == hugz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                }
                else
                if (self->event == bombcmd_event) {
                    if (!self->exception) {
                        //  sync server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ sync server not present");
                        sync_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == bombmsg_event) {
                    if (!self->exception) {
                        //  async server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ async server not present");
                        async_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else {
                    //  Handle unexpected protocol events
                    if (!self->exception) {
                        //  log protocol error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log protocol error");
                        log_protocol_error (&self->client);
                    }
                }
                break;

            case defaults_state:
                if (self->event == srsly_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log access denied
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log access denied");
                        log_access_denied (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == rtfm_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                    if (!self->exception) {
                        //  log invalid message
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log invalid message");
                        log_invalid_message (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == hugz_ok_event) {
                    if (!self->exception) {
                        //  stayin alive
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ stayin alive");
                        stayin_alive (&self->client);
                    }
                }
                else
                if (self->event == bombcmd_event) {
                    if (!self->exception) {
                        //  sync server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ sync server not present");
                        sync_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else
                if (self->event == bombmsg_event) {
                    if (!self->exception) {
                        //  async server not present
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ async server not present");
                        async_server_not_present (&self->client);
                    }
                    if (!self->exception) {
                        //  terminate
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ terminate");
                        self->fsm_stopped = true;
                    }
                }
                else {
                    //  Handle unexpected protocol events
                    if (!self->exception) {
                        //  log protocol error
                        if (fmq_client_verbose)
                            zsys_debug ("fmq_client:            $ log protocol error");
                        log_protocol_error (&self->client);
                    }
                }
                break;
        }
        //  If we had an exception event, interrupt normal programming
        if (self->exception) {
            if (fmq_client_verbose)
                zsys_debug ("fmq_client:            ! %s", s_event_name [self->exception]);
            self->next_event = self->exception;
        }
        else
        if (fmq_client_verbose)
            zsys_debug ("fmq_client:            > %s", s_state_name [self->state]);
    }
}

//  zloop callback when client inactivity timer expires

static int
s_client_handle_timeout (zloop_t *loop, int timer_id, void *argument)
{
    s_client_t *self = (s_client_t *) argument;
    s_client_execute (self, expired_event);
    return self->terminated? -1: 0;
}

//  zloop callback when client wakeup timer expires

static int
s_client_handle_wakeup (zloop_t *loop, int timer_id, void *argument)
{
    s_client_t *self = (s_client_t *) argument;
    s_client_execute (self, self->wakeup_event);
    return 0;
}


//  Handle command pipe to/from calling API

static int
s_client_handle_cmdpipe (zloop_t *loop, zsock_t *reader, void *argument)
{
    s_client_t *self = (s_client_t *) argument;
    char *method = zstr_recv (self->cmdpipe);
    if (!method)
        return -1;                  //  Interrupted; exit zloop
    if (fmq_client_verbose)
        zsys_debug ("fmq_client:        API command=%s", method);

    if (streq (method, "$TERM"))
        self->terminated = true;    //  Shutdown the engine
    else
    if (streq (method, "CONNECT")) {
        zstr_free (&self->args.endpoint);
        zsock_recv (self->cmdpipe, "s4", &self->args.endpoint, &self->args.timeout);
        s_client_execute (self, connect_event);
    }
    else
    if (streq (method, "DESTRUCTOR")) {
        s_client_execute (self, destructor_event);
    }
    else
    if (streq (method, "SUBSCRIBE")) {
        zstr_free (&self->args.path);
        zsock_recv (self->cmdpipe, "s", &self->args.path);
        s_client_execute (self, subscribe_event);
    }
    else
    if (streq (method, "SET INBOX")) {
        zstr_free (&self->args.path);
        zsock_recv (self->cmdpipe, "s", &self->args.path);
        s_client_execute (self, set_inbox_event);
    }
    //  Cleanup pipe if any argument frames are still waiting to be eaten
    if (zsock_rcvmore (self->cmdpipe)) {
        zsys_error ("fmq_client: trailing API command frames (%s)", method);
        zmsg_t *more = zmsg_recv (self->cmdpipe);
        zmsg_print (more);
        zmsg_destroy (&more);
    }
    zstr_free (&method);
    return self->terminated? -1: 0;
}


//  Handle message pipe to/from calling API

static int
s_client_handle_msgpipe (zloop_t *loop, zsock_t *reader, void *argument)
{
    s_client_t *self = (s_client_t *) argument;

    //  We will process as many messages as we can, to reduce the overhead
    //  of polling and the reactor:
    while (zsock_events (self->msgpipe) & ZMQ_POLLIN) {
        char *method = zstr_recv (self->msgpipe);
        if (!method)
            return -1;              //  Interrupted; exit zloop
        if (fmq_client_verbose)
            zsys_debug ("fmq_client:        API message=%s", method);

        //  Front-end shuts down msgpipe before cmdpipe, this little
        //  handshake just ensures all traffic on the msgpipe has been
        //  flushed before the calling thread continues with destroying
        //  the actor.
        if (streq (method, "$FLUSH"))
            zsock_signal (self->cmdpipe, 0);
        //  Cleanup pipe if any argument frames are still waiting to be eaten
        if (zsock_rcvmore (self->msgpipe)) {
            zsys_error ("fmq_client: trailing API message frames (%s)", method);
            zmsg_t *more = zmsg_recv (self->msgpipe);
            zmsg_print (more);
            zmsg_destroy (&more);
        }
        zstr_free (&method);
    }
    return 0;
}


//  Handle a message (a protocol reply) from the server

static int
s_client_handle_protocol (zloop_t *loop, zsock_t *reader, void *argument)
{
    s_client_t *self = (s_client_t *) argument;

    //  We will process as many messages as we can, to reduce the overhead
    //  of polling and the reactor:
    while (zsock_events (self->dealer) & ZMQ_POLLIN) {
        if (fmq_msg_recv (self->message, self->dealer))
            return -1;              //  Interrupted; exit zloop

        //  Any input from server counts as activity
        if (self->expiry_timer) {
            zloop_timer_end (self->loop, self->expiry_timer);
            self->expiry_timer = 0;
        }
        //  Reset expiry timer if timeout is not zero
        if (self->timeout)
            self->expiry_timer = zloop_timer (
                self->loop, self->timeout, 1, s_client_handle_timeout, self);
        s_client_execute (self, s_protocol_event (self, self->message));
        if (self->terminated)
            return -1;
    }
    return 0;
}


//  ---------------------------------------------------------------------------
//  This is the client actor, which polls its two sockets and processes
//  incoming messages

void
fmq_client (zsock_t *cmdpipe, void *msgpipe)
{
    //  Initialize
    s_client_t *self = s_client_new (cmdpipe, (zsock_t *) msgpipe);
    if (self) {
        zsock_signal (cmdpipe, 0);

        //  Set up handler for the sockets the client uses
        engine_handle_socket ((client_t *) self, self->cmdpipe, s_client_handle_cmdpipe);
        engine_handle_socket ((client_t *) self, self->msgpipe, s_client_handle_msgpipe);
        engine_handle_socket ((client_t *) self, self->dealer, s_client_handle_protocol);

        //  Run reactor until there's a termination signal
        zloop_start (self->loop);

        //  Reactor has ended
        s_client_destroy (&self);
    }
    else
        zsock_signal (cmdpipe, -1);
}


//  ---------------------------------------------------------------------------
//  Class interface

struct _fmq_client_t {
    zactor_t *actor;            //  Client actor
    zsock_t *msgpipe;           //  Pipe for async message flow
    uint8_t status;             //  Returned by actor reply
    char *reason;               //  Returned by actor reply
};


//  ---------------------------------------------------------------------------
//  Create a new fmq_client

fmq_client_t *
fmq_client_new (void)
{
    fmq_client_t *self = (fmq_client_t *) zmalloc (sizeof (fmq_client_t));
    if (self) {
        zsock_t *backend;
        self->msgpipe = zsys_create_pipe (&backend);
        if (self->msgpipe)
            self->actor = zactor_new (fmq_client, backend);
        if (!self->actor)
            fmq_client_destroy (&self);
    }
    return self;
}


//  ---------------------------------------------------------------------------
//  Destroy the fmq_client
//  Disconnect from server.                                                         

static uint8_t 
fmq_client_destructor (fmq_client_t *self);

void
fmq_client_destroy (fmq_client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_client_t *self = *self_p;
        if (self->actor && !zsys_interrupted) {
            //  Before destroying the actor we have to flush any pending
            //  traffic on the msgpipe, otherwise it gets lost in a fire and
            //  forget scenario. We do this by sending $FLUSH to the msgpipe
            //  and waiting for a signal back on the cmdpipe.
            if (zstr_send (self->msgpipe, "$FLUSH") == 0)
                zsock_wait (self->actor);
            fmq_client_destructor (self);
        }
        zactor_destroy (&self->actor);
        zsock_destroy (&self->msgpipe);
        zstr_free (&self->reason);
        free (self);
        *self_p = NULL;
    }
}


//  ---------------------------------------------------------------------------
//  Return actor, when caller wants to work with multiple actors and/or
//  input sockets asynchronously.

zactor_t *
fmq_client_actor (fmq_client_t *self)
{
    assert (self);
    return self->actor;
}


//  ---------------------------------------------------------------------------
//  Return message pipe for asynchronous message I/O. In the high-volume case,
//  we send methods and get replies to the actor, in a synchronous manner, and
//  we send/recv high volume message data to a second pipe, the msgpipe. In
//  the low-volume case we can do everything over the actor pipe, if traffic
//  is never ambiguous.

zsock_t *
fmq_client_msgpipe (fmq_client_t *self)
{
    assert (self);
    return self->msgpipe;
}


//  ---------------------------------------------------------------------------
//  Get valid reply from actor; discard replies that does not match. Current
//  implementation filters on first frame of message. Blocks until a valid
//  reply is received, and properties can be loaded from it. Returns 0 if
//  matched, -1 if interrupted or timed-out.

static int
s_accept_reply (fmq_client_t *self, ...)
{
    assert (self);
    while (!zsys_interrupted) {
        char *reply = zstr_recv (self->actor);
        if (!reply)
            break;              //  Interrupted or timed-out

        va_list args;
        va_start (args, self);
        char *filter = va_arg (args, char *);
        while (filter) {
            if (streq (reply, filter)) {
                if (streq (reply, "SUCCESS")) {
                    zsock_recv (self->actor, "1", &self->status);
                }
                else
                if (streq (reply, "FAILURE")) {
                    zstr_free (&self->reason);
                    zsock_recv (self->actor, "1s", &self->status, &self->reason);
                }
                break;
            }
            filter = va_arg (args, char *);
        }
        va_end (args);
        //  If anything was remaining on pipe, flush it
        zsock_flush (self->actor);
        if (filter) {
            zstr_free (&reply);
            return 0;           //  We matched one of the filters
        }
    }
    return -1;          //  Interrupted or timed-out
}


//  ---------------------------------------------------------------------------
//  Connect to server endpoint, with specified timeout in msecs (zero means wait    
//  forever). Connect succeeds if connection is successful.                         
//  Returns >= 0 if successful, -1 if interrupted.

uint8_t 
fmq_client_connect (fmq_client_t *self, const char *endpoint, uint32_t timeout)
{
    assert (self);

    zsock_send (self->actor, "ss4", "CONNECT", endpoint, timeout);
    if (s_accept_reply (self, "SUCCESS", "FAILURE", NULL))
        return -1;              //  Interrupted or timed-out
    return self->status;
}


//  ---------------------------------------------------------------------------
//  Disconnect from server.                                                         
//  Returns >= 0 if successful, -1 if interrupted.

uint8_t 
fmq_client_destructor (fmq_client_t *self)
{
    assert (self);

    zsock_send (self->actor, "s", "DESTRUCTOR");
    if (s_accept_reply (self, "SUCCESS", "FAILURE", NULL))
        return -1;              //  Interrupted or timed-out
    return self->status;
}


//  ---------------------------------------------------------------------------
//  Subscribe to a directory on the server, directory specified by path.            
//  Returns >= 0 if successful, -1 if interrupted.

uint8_t 
fmq_client_subscribe (fmq_client_t *self, const char *path)
{
    assert (self);

    zsock_send (self->actor, "ss", "SUBSCRIBE", path);
    if (s_accept_reply (self, "SUCCESS", "FAILURE", NULL))
        return -1;              //  Interrupted or timed-out
    return self->status;
}


//  ---------------------------------------------------------------------------
//  Tell the api where to store files. This should be done before subscribing to    
//  anything.                                                                       
//  Returns >= 0 if successful, -1 if interrupted.

uint8_t 
fmq_client_set_inbox (fmq_client_t *self, const char *path)
{
    assert (self);

    zsock_send (self->actor, "ss", "SET INBOX", path);
    if (s_accept_reply (self, "SUCCESS", "FAILURE", NULL))
        return -1;              //  Interrupted or timed-out
    return self->status;
}


//  ---------------------------------------------------------------------------
//  Return last received status

uint8_t 
fmq_client_status (fmq_client_t *self)
{
    assert (self);
    return self->status;
}


//  ---------------------------------------------------------------------------
//  Return last received reason

const char *
fmq_client_reason (fmq_client_t *self)
{
    assert (self);
    return self->reason;
}
