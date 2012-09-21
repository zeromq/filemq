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

#include <czmq.h>
#include "../include/fmq_server.h"
#include "../include/fmq_msg.h"
//  TODO: the generators and required classes should not be part of FMQ
//  as such, but a separate project so they can be reused more widely.
#include "../include/fmq.h"

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
//  Load server configuration data
void
fmq_server_configure (fmq_server_t *self, const char *config_file)
{
    zstr_sendm (self->pipe, "CONFIG");
    zstr_send (self->pipe, config_file);
}


//  --------------------------------------------------------------------------

void
fmq_server_bind (fmq_server_t *self, const char *endpoint)
{
    assert (self);
    assert (endpoint);
    zstr_sendm (self->pipe, "BIND");
    zstr_send (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------

void
fmq_server_connect (fmq_server_t *self, const char *endpoint)
{
    assert (self);
    assert (endpoint);
    zstr_sendm (self->pipe, "CONNECT");
    zstr_send (self->pipe, endpoint);
}


//  --------------------------------------------------------------------------

void
fmq_server_mount (fmq_server_t *self, const char *local, const char *virtual)
{
    assert (self);
    assert (local);
    assert (virtual);
    zstr_sendm (self->pipe, "MOUNT");
    zstr_sendm (self->pipe, local);
    zstr_send (self->pipe, virtual);
}


//  ---------------------------------------------------------------------
//  State machine constants

typedef enum {
    start_state = 1,
    checking_client_state = 2,
    challenging_client_state = 3,
    ready_state = 4
} state_t;

typedef enum {
    terminate_event = -1,
    ohai_event = 1,
    _other_event = 2,
    friend_event = 3,
    foe_event = 4,
    maybe_event = 5,
    yarly_event = 6,
    icanhaz_event = 7,
    nom_event = 8,
    hugz_event = 9,
    kthxbai_event = 10,
    heartbeat_event = 11,
    expired_event = 12
} event_t;

//  Names for animation
char *s_state_name [] = {
    "",
    "Start",
    "Checking Client",
    "Challenging Client",
    "Ready"
};

char *s_event_name [] = {
    "",
    "OHAI",
    "$other",
    "friend",
    "foe",
    "maybe",
    "YARLY",
    "ICANHAZ",
    "NOM",
    "HUGZ",
    "KTHXBAI",
    "heartbeat",
    "expired"
};


//  Forward declarations
typedef struct _server_t server_t;
typedef struct _client_t client_t;

//  Subscription in memory
typedef struct {
    client_t *client;           //  Always refers to live client
    char *path;                 //  Path we subscribe to
} sub_t;

static sub_t *
sub_new (client_t *client, char *path) {
    sub_t *self = (sub_t *) zmalloc (sizeof (sub_t));
    self->client = client;
    self->path = strdup (path);
    return self;
}

static void
sub_destroy (sub_t **self_p) {
    assert (self_p);
    if (*self_p) {
        sub_t *self = *self_p;
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}
//  Mount point in memory
typedef struct {
    char *local;            //  Local path
    char *virtual;          //  Virtual path
    fmq_dir_t *dir;         //  Directory tree

    //  Directory signature
    time_t time;            //  Modification time
    off_t  size;            //  Total file size
    size_t count;           //  Total file count

    //  List of diffs to directory
    zlist_t *diffs;         //  fmq_diff_t items
} mount_t;

//  Constructor
//  Loads directory tree if possible

static mount_t *
mount_new (char *local, char *virtual) {
    mount_t *self = (mount_t *) zmalloc (sizeof (mount_t));
    self->local = strdup (local);
    self->virtual = strdup (virtual);
    self->dir = fmq_dir_new (self->local, NULL);
    if (self->dir)
        fmq_dir_dump (self->dir, 0);
    return self;
}

//  Destructor

static void
mount_destroy (mount_t **self_p) {
    assert (self_p);
    if (*self_p) {
        mount_t *self = *self_p;
        free (self->local);
        free (self->virtual);
        fmq_dir_destroy (&self->dir);
        free (self);
        *self_p = NULL;
    }
}

//  Reloads directory tree and returns true if changed, false if the same.

static bool
mount_refresh (mount_t *self) {
    //  Get latest snapshot and if it's different, build a diffs list
    fmq_dir_t *latest = fmq_dir_new (self->local, NULL);
    zlist_t *diffs = fmq_dir_diff (self->dir, latest);
    fmq_dir_destroy (&self->dir);
    self->dir = latest;

    fmq_diff_t *diff = (fmq_diff_t *) zlist_first (diffs);
    while (diff) {
        fmq_file_t *file = fmq_diff_file (diff);
        fmq_diff_op_t op = fmq_diff_op (diff);
        switch (op) {
            case diff_create:
                printf ("CREATE %s/%s\n", fmq_file_path (file), fmq_file_name (file));
                break;
            case diff_delete:
                printf ("DELETE %s/%s\n", fmq_file_path (file), fmq_file_name (file));
                break;
            case diff_resize:
                printf ("RESIZE %s/%s\n", fmq_file_path (file), fmq_file_name (file));
                break;
            case diff_retime:
                printf ("RETIME %s/%s\n", fmq_file_path (file), fmq_file_name (file));
                break;
        }
        diff = (fmq_diff_t *) zlist_next (diffs);
    }
    while (zlist_size (diffs)) {
        diff = (fmq_diff_t *) zlist_pop (diffs);
        fmq_diff_destroy (&diff);
    }
    zlist_destroy (&diffs);
    return false;
}


//  ---------------------------------------------------------------------
//  Simple class for one client we talk to

struct _client_t {
    //  Properties accessible to client actions
    int64_t heartbeat;          //  Heartbeat interval
    event_t next_event;         //  Next event
    
    //  Properties you should NOT touch
    void *router;               //  Socket to client
    int64_t heartbeat_at;       //  Next heartbeat at this time
    int64_t expires_at;         //  Expires at this time
    state_t state;              //  Current state
    event_t event;              //  Current event
    char *hashkey;              //  Key into clients hash
    zframe_t *address;          //  Client address identity
    fmq_msg_t *request;         //  Last received request
    fmq_msg_t *reply;           //  Reply to send out, if any
    byte identity [FMQ_MSG_IDENTITY_SIZE];
};

static client_t *
client_new (char *hashkey, zframe_t *address)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
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
client_set_request (client_t *self, fmq_msg_t *request)
{
    if (self->request)
        fmq_msg_destroy (&self->request);
    self->request = request;

    //  Any input from client counts as activity
    self->expires_at = zclock_time () + self->heartbeat * 3;
}

//  Client hash function that calculates tickless timer
static int
client_tickless (const char *key, void *client, void *argument)
{
    client_t *self = (client_t *) client;
    uint64_t *tickless = (uint64_t *) argument;
    if (*tickless > self->heartbeat_at)
        *tickless = self->heartbeat_at;
    return 0;
}

static void
server_client_execute (server_t *server, client_t *client, int event);

//  Client hash function that checks if client is alive
static int
client_ping (const char *key, void *client, void *argument)
{
    client_t *self = (client_t *) client;
    //  Check whether to send heartbeat to client
    if (zclock_time () >= self->heartbeat_at) {
        server_client_execute ((server_t *) argument, self, heartbeat_event);
        self->heartbeat_at = zclock_time () + self->heartbeat;
    }
    //  Check whether to expire client, nothing received from it
    if (zclock_time () >= self->expires_at) {
        server_client_execute ((server_t *) argument, self, expired_event);
        client_destroy (&self);
    }
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

struct _server_t {
    //  Properties accessible to client actions
    
    //  Properties you should NOT touch
    zctx_t *ctx;                //  Own CZMQ context
    void *pipe;                 //  Socket to back to caller
    void *router;               //  Socket to talk to clients
    zhash_t *clients;           //  Clients we've connected to
    bool stopped;               //  Has server stopped?
    fmq_config_t *config;       //  Configuration tree
    int monitor;                //  Monitor interval
    int64_t monitor_at;         //  Next monitor at this time
    int heartbeat;              //  Heartbeat for clients
    zlist_t *subs;              //  Client subscriptions
    zlist_t *mounts;            //  Mount points
};

static server_t *
server_new (zctx_t *ctx, void *pipe)
{
    server_t *self = (server_t *) zmalloc (sizeof (server_t));
    self->ctx = ctx;
    self->pipe = pipe;
    self->router = zsocket_new (self->ctx, ZMQ_ROUTER);
    self->clients = zhash_new ();
    self->config = fmq_config_new ("root", NULL);
    self->monitor = 5000;       //  5 seconds by default
    self->subs = zlist_new ();
    self->mounts = zlist_new ();
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
        //  Destroy subscriptions
    while (zlist_size (self->subs)) {
        sub_t *sub = (sub_t *) zlist_pop (self->subs);
        sub_destroy (&sub);
    }
    zlist_destroy (&self->subs);

    //  Destroy mount points
    while (zlist_size (self->mounts)) {
        mount_t *mount = (mount_t *) zlist_pop (self->mounts);
        mount_destroy (&mount);
    }
    zlist_destroy (&self->mounts);
        free (self);
        *self_p = NULL;
    }
}

//  Apply configuration tree:
//   * apply server configuration
//   * print any echo items in top-level sections
//   * apply sections that match methods

static void
server_apply_config (server_t *self)
{
    //  Get standard server configuration
    self->monitor = atoi (
        fmq_config_resolve (self->config, "server/monitor", "5")) * 1000;
    self->heartbeat = atoi (
        fmq_config_resolve (self->config, "server/heartbeat", "1")) * 1000;
    self->monitor_at = zclock_time () + self->monitor;

    //  Apply echo commands and class methods
    fmq_config_t *section = fmq_config_child (self->config);
    while (section) {
        fmq_config_t *entry = fmq_config_child (section);
        while (entry) {
            if (streq (fmq_config_name (entry), "echo"))
                puts (fmq_config_value (entry));
            entry = fmq_config_next (entry);
        }
        if (streq (fmq_config_name (section), "bind")) {
            char *endpoint = fmq_config_resolve (section, "endpoint", "?");
            zmq_bind (self->router, endpoint);
        }
        else
        if (streq (fmq_config_name (section), "connect")) {
            char *endpoint = fmq_config_resolve (section, "endpoint", "?");
            zmq_connect (self->router, endpoint);
        }
        else
        if (streq (fmq_config_name (section), "mount")) {
            char *local = fmq_config_resolve (section, "local", "?");
            char *virtual = fmq_config_resolve (section, "virtual", "?");
            mount_t *mount = mount_new (local, virtual);
    zlist_append (self->mounts, mount);
        }
        section = fmq_config_next (section);
    }
}

static void
server_control_message (server_t *self)
{
    zmsg_t *msg = zmsg_recv (self->pipe);
    char *method = zmsg_popstr (msg);
    if (streq (method, "BIND")) {
        char *endpoint = zmsg_popstr (msg);
        zmq_bind (self->router, endpoint);
        free (endpoint);
    }
    else
    if (streq (method, "CONNECT")) {
        char *endpoint = zmsg_popstr (msg);
        zmq_connect (self->router, endpoint);
        free (endpoint);
    }
    else
    if (streq (method, "MOUNT")) {
        char *local = zmsg_popstr (msg);
        char *virtual = zmsg_popstr (msg);
        mount_t *mount = mount_new (local, virtual);
    zlist_append (self->mounts, mount);
        free (local);
        free (virtual);
    }
    else
    if (streq (method, "CONFIG")) {
        char *config_file = zmsg_popstr (msg);
        fmq_config_destroy (&self->config);
        self->config = fmq_config_load (config_file);
        if (self->config)
            server_apply_config (self);
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
    }
    free (method);
    zmsg_destroy (&msg);
}

//  Custom actions for state machine

static void
terminate_the_client (server_t *self, client_t *client) {
    //  Remove all subscriptions for this client
    sub_t *sub = (sub_t *) zlist_first (self->subs);
    while (sub) {
        if (sub->client == client) {
            sub_t *next = (sub_t *) zlist_next (self->subs);
            zlist_remove (self->subs, sub);
            sub_destroy (&sub);
            sub = next;
        }
        else
            sub = (sub_t *) zlist_next (self->subs);
    }
    client->next_event = terminate_event;
}

static void
track_client_identity (server_t *self, client_t *client) {
    memcpy (client->identity, fmq_msg_identity (client->request), FMQ_MSG_IDENTITY_SIZE);
}

static void
try_anonymous_access (server_t *self, client_t *client) {
    if (atoi (fmq_config_resolve (self->config, "security/anonymous", "0")) == 1)
        client->next_event = friend_event;
    else
    if (atoi (fmq_config_resolve (self->config, "security/plain", "0")) == 1)
        client->next_event = maybe_event;
    else
        client->next_event = foe_event;
}

static void
list_security_mechanisms (server_t *self, client_t *client) {
    if (atoi (fmq_config_resolve (self->config, "security/anonymous", "0")) == 1)
        fmq_msg_mechanisms_append (client->reply, "ANONYMOUS");
    if (atoi (fmq_config_resolve (self->config, "security/plain", "0")) == 1)
        fmq_msg_mechanisms_append (client->reply, "PLAIN");
}

static void
try_security_mechanism (server_t *self, client_t *client) {
    client->next_event = foe_event;
    char *login, *password;
    if (streq (fmq_msg_mechanism (client->request), "PLAIN")
    &&  fmq_sasl_plain_decode (fmq_msg_response (client->request), &login, &password) == 0) {
        fmq_config_t *account = fmq_config_locate (self->config, "security/plain/account");
        while (account) {
            if (streq (fmq_config_resolve (account, "login", ""), login)
            &&  streq (fmq_config_resolve (account, "password", ""), password)) {
                client->next_event = friend_event;
                break;
            }
            account = fmq_config_next (account);
        }
    }
    free (login);
    free (password);
}

static void
store_client_subscription (server_t *self, client_t *client) {
    sub_t *sub = sub_new (client, fmq_msg_path (client->request));
    zlist_append (self->subs, sub);
}

static void
monitor_the_server (server_t *self, client_t *client) {
    mount_t *mount = (mount_t *) zlist_first (self->mounts);
    while (mount) {
        if (mount_refresh (mount)) {

            //printf ("- %s/%s\n", fmq_file_path (old), fmq_file_name (old));
            //printf ("+ %s/%s\n", fmq_file_path (new), fmq_file_name (new));
            //printf ("# %s/%s\n", fmq_file_path (new), fmq_file_name (new));
            //printf ("@ %s/%s\n", fmq_file_path (new), fmq_file_name (new));


            //  Changed
        }
        mount = (mount_t *) zlist_next (self->mounts);
    }
}

//  Execute state machine as long as we have events

static void
server_client_execute (server_t *self, client_t *client, int event)
{
    client->next_event = event;
    while (client->next_event) {
        client->event = client->next_event;
        client->next_event = 0;
        zclock_log ("%s:", s_state_name [client->state]);
        zclock_log (" (%s)", s_event_name [client->event]);
        switch (client->state) {
            case start_state:
                if (client->event == ohai_event) {
                    zclock_log ("    + track client identity");
                    track_client_identity (self, client);
                    zclock_log ("    + try anonymous access");
                    try_anonymous_access (self, client);
                    client->state = checking_client_state;
                }
                else {
                    zclock_log ("    + send RTFM");
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                break;

            case checking_client_state:
                if (client->event == friend_event) {
                    zclock_log ("    + send OHAI_OK");
                    fmq_msg_id_set (client->reply, FMQ_MSG_OHAI_OK);
                    client->state = ready_state;
                }
                else
                if (client->event == foe_event) {
                    zclock_log ("    + send SRSLY");
                    fmq_msg_id_set (client->reply, FMQ_MSG_SRSLY);
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                else
                if (client->event == maybe_event) {
                    zclock_log ("    + list security mechanisms");
                    list_security_mechanisms (self, client);
                    zclock_log ("    + send ORLY");
                    fmq_msg_id_set (client->reply, FMQ_MSG_ORLY);
                    client->state = challenging_client_state;
                }
                else {
                    zclock_log ("    + send RTFM");
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                break;

            case challenging_client_state:
                if (client->event == yarly_event) {
                    zclock_log ("    + try security mechanism");
                    try_security_mechanism (self, client);
                    client->state = checking_client_state;
                }
                else {
                    zclock_log ("    + send RTFM");
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                break;

            case ready_state:
                if (client->event == icanhaz_event) {
                    zclock_log ("    + store client subscription");
                    store_client_subscription (self, client);
                    zclock_log ("    + send ICANHAZ_OK");
                    fmq_msg_id_set (client->reply, FMQ_MSG_ICANHAZ_OK);
                }
                else
                if (client->event == nom_event) {
                    zclock_log ("    + send CHEEZBURGER");
                    fmq_msg_id_set (client->reply, FMQ_MSG_CHEEZBURGER);
                }
                else
                if (client->event == hugz_event) {
                    zclock_log ("    + send HUGZ_OK");
                    fmq_msg_id_set (client->reply, FMQ_MSG_HUGZ_OK);
                }
                else
                if (client->event == kthxbai_event) {
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                else
                if (client->event == heartbeat_event) {
                    zclock_log ("    + send HUGZ");
                    fmq_msg_id_set (client->reply, FMQ_MSG_HUGZ);
                }
                else
                if (client->event == expired_event) {
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                else {
                    zclock_log ("    + send RTFM");
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    zclock_log ("    + terminate the client");
                    terminate_the_client (self, client);
                }
                break;

        }
        zclock_log ("      -------------------> %s", s_state_name [client->state]);

        if (fmq_msg_id (client->reply)) {
            fmq_msg_send (&client->reply, client->router);
            client->reply = fmq_msg_new (0);
            fmq_msg_address_set (client->reply, client->address);
            
            //  Any output from client counts as heartbeat
            client->heartbeat_at = zclock_time () + client->heartbeat;
        }
        if (client->next_event == terminate_event) {
            //  Automatically calls client_destroy
            zhash_delete (self->clients, client->hashkey);
            break;
        }
    }
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
        client = client_new (hashkey, fmq_msg_address (request));
        client->heartbeat = self->heartbeat;
        client->router = self->router;
        zhash_insert (self->clients, hashkey, client);
        zhash_freefn (self->clients, hashkey, client_free);
    }
    else
        free (hashkey);

    client_set_request (client, request);
    if (fmq_msg_id (request) == FMQ_MSG_OHAI)
        server_client_execute (self, client, ohai_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_YARLY)
        server_client_execute (self, client, yarly_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_ICANHAZ)
        server_client_execute (self, client, icanhaz_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_NOM)
        server_client_execute (self, client, nom_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_HUGZ)
        server_client_execute (self, client, hugz_event);
    else
    if (fmq_msg_id (request) == FMQ_MSG_KTHXBAI)
        server_client_execute (self, client, kthxbai_event);
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
    
    self->monitor_at = zclock_time () + self->monitor;
    while (!self->stopped) {
        //  Calculate tickless timer, up to interval seconds
        uint64_t tickless = zclock_time () + self->monitor;
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
        zhash_foreach (self->clients, client_ping, self);

        //  If clock went past timeout, then monitor server
        if (zclock_time () >= self->monitor_at) {
            monitor_the_server (self, NULL);
            self->monitor_at = zclock_time () + self->monitor;
        }
    }
    server_destroy (&self);
}


//  --------------------------------------------------------------------------
//  Selftest

int
fmq_server_test (bool verbose)
{
    printf (" * fmq_server: ");
    printf ("\n");
    zctx_t *ctx = zctx_new ();
    
    fmq_server_t *self;
    void *dealer = zsocket_new (ctx, ZMQ_DEALER);
    zsocket_set_rcvtimeo (dealer, 2000);
    zsocket_connect (dealer, "tcp://localhost:6000");
    
    fmq_msg_t *request, *reply;
    
    //  Run selftest using '' configuration
    self = fmq_server_new ();
    assert (self);
    //  We don't use a config file so we have to do this ourselves
    fmq_server_bind (self, "tcp://*:6000");
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
    
    //  Run selftest using 'example.cfg' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_configure (self, "example.cfg");
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
