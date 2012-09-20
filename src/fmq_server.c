/*  =========================================================================
    fmq_server.c

    Generated class for fmq_server protocol server
    =========================================================================
*/

#include <czmq.h>
#include "../include/fmq_server.h"
#include "../include/fmq_msg.h"
#include "../include/fmq_config.h"
#include "../include/fmq_sasl.h"

//  The server runs as a background thread so that we can run multiple
//  engines at once. The API talks to the server thread over an inproc
//  pipe.

static void
    server_thread (void *args, zctx_t *ctx, void *pipe);

//  ---------------------------------------------------------------------
//  Structure of our front-end API class

struct _fmq_server_t {
    zctx_t *ctx;        //  CZMQ context
    void *pipe;         //  Pipe through to server
};


//  --------------------------------------------------------------------------
//  Create a new fmq_server and a new server instance

fmq_server_t *
fmq_server_new (void)
{
    fmq_server_t *self = (fmq_server_t *) zmalloc (sizeof (fmq_server_t));
    self->ctx = zctx_new ();
    self->pipe = zthread_fork (self->ctx, server_thread, NULL);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fmq_server and stop the server

void
fmq_server_destroy (fmq_server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_server_t *self = *self_p;
        zstr_send (self->pipe, "STOP");
        char *string = zstr_recv (self->pipe);
        free (string);
        zctx_destroy (&self->ctx);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Bind to endpoint

void
fmq_server_bind (fmq_server_t *self, const char *endpoint)
{
    assert (self);
    assert (endpoint);
    zstr_sendm (self->pipe, "BIND");
    zstr_send (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------
//  Connect to endpoint

void
fmq_server_connect (fmq_server_t *self, const char *endpoint)
{
    assert (self);
    assert (endpoint);
    zstr_sendm (self->pipe, "CONNECT");
    zstr_send (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------
//  Load server configuration data
void
fmq_server_configure (fmq_server_t *self, const char *config_file)
{
    zstr_sendm (self->pipe, "CONFIG");
    zstr_send (self->pipe, config_file);
}


//  ---------------------------------------------------------------------
//  State machine constants

typedef enum {
    stopped_state = 0,
    start_state = 1,
    checking_client_state = 2,
    challenging_client_state = 3,
    ready_state = 4
} state_t;

typedef enum {
    ohai_event = 1,
    other_event = 2,
    friend_event = 3,
    foe_event = 4,
    maybe_event = 5,
    yarly_event = 6,
    icanhaz_event = 7,
    nom_event = 8,
    hugz_event = 9,
    kthxbai_event = 10,
    heartbeat_event = 11
} event_t;


//  ---------------------------------------------------------------------
//  Simple class for one client we talk to

typedef struct {
    //  Properties accessible to state machine actions
    int64_t heartbeat;          //  Heartbeat interval
    event_t next_event;         //  Next event
    fmq_config_t *config;       //  Configuration tree
    
    //  Properties you should NOT touch
    void *router;               //  Socket to client
    int64_t heartbeat_at;       //  Next heartbeat at this time
    int64_t expires;            //  Expires at this time
    state_t state;              //  Current state
    event_t event;              //  Current event
    char *hashkey;              //  Key into clients hash
    zframe_t *address;          //  Client address identity
    fmq_msg_t *request;         //  Last received request
    fmq_msg_t *reply;           //  Reply to send out, if any
} client_t;

static client_t *
client_new (void *router, char *hashkey, zframe_t *address)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
    self->heartbeat = 1000;
    self->router = router;
    self->hashkey = hashkey;
    self->state = start_state;
    self->address = zframe_dup (address);
    self->reply = fmq_msg_new (0);
    fmq_msg_address_set (self->reply, self->address);
    return self;
}

static void
client_destroy (client_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        client_t *self = *self_p;
        zframe_destroy (&self->address);
        fmq_msg_destroy (&self->request);
        fmq_msg_destroy (&self->reply);
        free (self->hashkey);
        free (self);
        *self_p = NULL;
    }
}

static void
try_anonymous_access_action (client_t *self) {
    if (atoi (fmq_config_resolve (self->config, "security/anonymous", "0")) == 1)
        self->next_event = friend_event;
    else
    if (atoi (fmq_config_resolve (self->config, "security/plain", "0")) == 1)
        self->next_event = maybe_event;
    else
        self->next_event = foe_event;
}

static void
list_security_mechanisms_action (client_t *self) {
    if (atoi (fmq_config_resolve (self->config, "security/anonymous", "0")) == 1)
        fmq_msg_mechanisms_append (self->reply, "ANONYMOUS");
    if (atoi (fmq_config_resolve (self->config, "security/plain", "0")) == 1)
        fmq_msg_mechanisms_append (self->reply, "PLAIN");
}

static void
try_security_mechanism_action (client_t *self) {
    self->next_event = foe_event;
    char *login, *password;
    if (streq (fmq_msg_mechanism (self->request), "PLAIN")
    &&  fmq_sasl_plain_decode (fmq_msg_response (self->request), &login, &password) == 0) {
        fmq_config_t *account = fmq_config_locate (self->config, "security/plain/account");
        while (account) {
            if (streq (fmq_config_resolve (account, "login", ""), login)
            &&  streq (fmq_config_resolve (account, "password", ""), password)) {
                self->next_event = friend_event;
                break;
            }
            account = fmq_config_next (account);
        }
    }
    free (login);
    free (password);
}

//  Execute state machine as long as we have events

static void
client_execute (client_t *self, fmq_config_t *config, int event)
{
    self->next_event = event;
    self->config = config;
    while (self->next_event) {
        self->event = self->next_event;
        self->next_event = 0;
        switch (self->state) {
            case start_state:
                if (self->event == ohai_event) {
                    try_anonymous_access_action (self);
                    self->state = checking_client_state;
                }
                else {
                    fmq_msg_id_set (self->reply, FMQ_MSG_RTFM);
                }
                break;

            case checking_client_state:
                if (self->event == friend_event) {
                    fmq_msg_id_set (self->reply, FMQ_MSG_OHAI_OK);
                    self->state = ready_state;
                }
                else
                if (self->event == foe_event) {
                    fmq_msg_id_set (self->reply, FMQ_MSG_SRSLY);
                    self->state = start_state;
                }
                else
                if (self->event == maybe_event) {
                    list_security_mechanisms_action (self);
                    fmq_msg_id_set (self->reply, FMQ_MSG_ORLY);
                    self->state = challenging_client_state;
                }
                else {
                    fmq_msg_id_set (self->reply, FMQ_MSG_RTFM);
                }
                break;

            case challenging_client_state:
                if (self->event == yarly_event) {
                    try_security_mechanism_action (self);
                    self->state = checking_client_state;
                }
                else {
                    fmq_msg_id_set (self->reply, FMQ_MSG_RTFM);
                }
                break;

            case ready_state:
                if (self->event == icanhaz_event) {
                    fmq_msg_id_set (self->reply, FMQ_MSG_ICANHAZ_OK);
                }
                else
                if (self->event == nom_event) {
                    fmq_msg_id_set (self->reply, FMQ_MSG_CHEEZBURGER);
                }
                else
                if (self->event == hugz_event) {
                    fmq_msg_id_set (self->reply, FMQ_MSG_HUGZ_OK);
                }
                else
                if (self->event == kthxbai_event) {
                    self->state = start_state;
                }
                else
                if (self->event == heartbeat_event) {
                    fmq_msg_id_set (self->reply, FMQ_MSG_HUGZ);
                }
                else {
                    fmq_msg_id_set (self->reply, FMQ_MSG_RTFM);
                }
                break;

            case stopped_state:
                //  Discard all events silently
                break;
        }
        if (fmq_msg_id (self->reply)) {
            fmq_msg_send (&self->reply, self->router);
            self->reply = fmq_msg_new (0);
            fmq_msg_address_set (self->reply, self->address);
        }
    }
}

static void
client_set_request (client_t *self, fmq_msg_t *request)
{
    if (self->request)
        fmq_msg_destroy (&self->request);
    self->request = request;

    //  Any input from client counts as activity
    self->heartbeat_at = zclock_time () + self->heartbeat;
    self->expires = zclock_time () + self->heartbeat * 3;
}

//  Client hash function that checks if client is alive
static int
client_ping (const char *key, void *client, void *argument)
{
    client_t *self = (client_t *) client;
    if (zclock_time () >= self->heartbeat_at) {
        client_execute (self, (fmq_config_t *) argument, heartbeat_event);
        self->heartbeat_at = zclock_time () + self->heartbeat;
    }
    return 0;
}

//  Client hash function that calculates tickless timer
static int
client_tickless (const char *key, void *client, void *arg)
{
    client_t *self = (client_t *) client;
    uint64_t *tickless = (uint64_t *) arg;
    if (*tickless > self->heartbeat_at)
        *tickless = self->heartbeat_at;
    return 0;
}

//  Callback when we remove client from 'clients' hash table
static void
client_free (void *argument)
{
    client_t *client = (client_t *) argument;
    client_destroy (&client);
}


//  ---------------------------------------------------------------------
//  Context for the server thread

typedef struct {
    zctx_t *ctx;                //  Own CZMQ context
    void *pipe;                 //  Socket to back to caller
    void *router;               //  Socket to talk to clients
    zhash_t *clients;           //  Clients we've connected to
    bool stopped;               //  Has server stopped?
    fmq_config_t *config;       //  Configuration tree
} server_t;

static server_t *
server_new (zctx_t *ctx, void *pipe)
{
    server_t *self = (server_t *) zmalloc (sizeof (server_t));
    self->ctx = ctx;
    self->pipe = pipe;
    self->router = zsocket_new (self->ctx, ZMQ_ROUTER);
    self->clients = zhash_new ();
    return self;
}

static void
server_destroy (server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        server_t *self = *self_p;
        fmq_config_destroy (&self->config);
        zhash_destroy (&self->clients);
        free (self);
        *self_p = NULL;
    }
}

static void
server_control_message (server_t *self)
{
    zmsg_t *msg = zmsg_recv (self->pipe);
    char *method = zmsg_popstr (msg);
    if (streq (method, "BIND")) {
        char *endpoint = zmsg_popstr (msg);
        int rc = zmq_bind (self->router, endpoint);
        free (endpoint);
    }
    else
    if (streq (method, "CONNECT")) {
        char *endpoint = zmsg_popstr (msg);
        int rc = zmq_connect (self->router, endpoint);
        free (endpoint);
    }
    else
    if (streq (method, "CONFIG")) {
        char *config_file = zmsg_popstr (msg);
        fmq_config_destroy (&self->config);
        self->config = fmq_config_load (config_file);
        //  Always provide application with a config tree, even if empty
        if (!self->config)
            self->config = fmq_config_new ("root", NULL);
        free (config_file);
    }
    else
    if (streq (method, "STOP")) {
        zstr_send (self->pipe, "OK");
        self->stopped = true;
    }
    free (method);
    zmsg_destroy (&msg);
}

static void
server_client_message (server_t *self)
{
    fmq_msg_t *request = fmq_msg_recv (self->router);
    if (!request)
        return;         //  Interrupted; do nothing

    char *hashkey = zframe_strhex (fmq_msg_address (request));
    client_t *client = zhash_lookup (self->clients, hashkey);
    if (client == NULL) {
        client = client_new (self->router, hashkey, fmq_msg_address (request));
        zhash_insert (self->clients, hashkey, client);
        zhash_freefn (self->clients, hashkey, client_free);
    }
    else
        free (hashkey);

    client_set_request (client, request);
    if (fmq_msg_id (request) == FMQ_MSG_OHAI)
        client_execute (client, self->config, ohai_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_YARLY)
        client_execute (client, self->config, yarly_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_ICANHAZ)
        client_execute (client, self->config, icanhaz_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_NOM)
        client_execute (client, self->config, nom_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_HUGZ)
        client_execute (client, self->config, hugz_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_KTHXBAI)
        client_execute (client, self->config, kthxbai_event);
}

//  Finally here's the server thread itself, which polls its two
//  sockets and processes incoming messages

static void
server_thread (void *args, zctx_t *ctx, void *pipe)
{
    server_t *self = server_new (ctx, pipe);
    zmq_pollitem_t items [] = {
        { self->pipe, 0, ZMQ_POLLIN, 0 },
        { self->router, 0, ZMQ_POLLIN, 0 }
    };
    while (!self->stopped) {
        //  Calculate tickless timer, up to 1 hour
        uint64_t tickless = zclock_time () + 1000 * 3600;
        zhash_foreach (self->clients, client_tickless, &tickless);

        //  Poll until at most next timer event
        int rc = zmq_poll (items, 2,
            (tickless - zclock_time ()) * ZMQ_POLL_MSEC);
        if (rc == -1)
            break;              //  Context has been shut down

        //  Process incoming message from either socket
        if (items [0].revents & ZMQ_POLLIN)
            server_control_message (self);

        if (items [1].revents & ZMQ_POLLIN)
            server_client_message (self);

        //  Send heartbeats to idle clients as needed
        zhash_foreach (self->clients, client_ping, self->config);
    }
    server_destroy (&self);
}


//  --------------------------------------------------------------------------
//  Selftest

int
fmq_server_test (bool verbose)
{
    printf (" * fmq_server: ");
    fflush (stdout);
    zctx_t *ctx = zctx_new ();
    
    fmq_server_t *self;
    void *dealer = zsocket_new (ctx, ZMQ_DEALER);
    zsocket_set_rcvtimeo (dealer, 2000);
    zsocket_connect (dealer, "tcp://localhost:6000");
    
    fmq_msg_t *request, *reply;
    
    //  Run selftest using '' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_bind (self, "tcp://*:6000");
    fmq_server_configure (self, "");

    request = fmq_msg_new (FMQ_MSG_OHAI);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_SRSLY);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_RTFM);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_RTFM);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_HUGZ);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_RTFM);
    fmq_msg_destroy (&reply);

    fmq_server_destroy (&self);
    
    //  Run selftest using 'anonymous.cfg' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_bind (self, "tcp://*:6000");
    fmq_server_configure (self, "anonymous.cfg");

    request = fmq_msg_new (FMQ_MSG_OHAI);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_OHAI_OK);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_ICANHAZ_OK);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_CHEEZBURGER);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_HUGZ);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_HUGZ_OK);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_YARLY);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_RTFM);
    fmq_msg_destroy (&reply);

    fmq_server_destroy (&self);
    
    //  Run selftest using 'plain.cfg' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_bind (self, "tcp://*:6000");
    fmq_server_configure (self, "plain.cfg");

    request = fmq_msg_new (FMQ_MSG_OHAI);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_ORLY);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_YARLY);
    fmq_msg_mechanism_set (request, "PLAIN");
    fmq_msg_response_set (request, fmq_sasl_plain_encode ("guest", "guest"));
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_OHAI_OK);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_ICANHAZ_OK);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_CHEEZBURGER);
    fmq_msg_destroy (&reply);

    request = fmq_msg_new (FMQ_MSG_HUGZ);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_HUGZ_OK);
    fmq_msg_destroy (&reply);

    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_HUGZ);
    fmq_msg_destroy (&reply);

    fmq_server_destroy (&self);
    
    zctx_destroy (&ctx);
    printf ("OK\n");
    return 0;
}
