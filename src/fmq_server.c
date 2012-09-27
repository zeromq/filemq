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
//  Set one configuration key value

void
fmq_server_setoption (fmq_server_t *self, const char *path, const char *value)
{
    zstr_sendm (self->pipe, "SETOPTION");
    zstr_sendm (self->pipe, path);
    zstr_send  (self->pipe, value);
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
fmq_server_mount (fmq_server_t *self, const char *path)
{
    assert (self);
    assert (path);
    zstr_sendm (self->pipe, "MOUNT");
    zstr_send (self->pipe, path);
}


//  ---------------------------------------------------------------------
//  State machine constants

typedef enum {
    start_state = 1,
    checking_client_state = 2,
    challenging_client_state = 3,
    ready_state = 4,
    dispatching_state = 5
} state_t;

typedef enum {
    terminate_event = -1,
    ohai_event = 1,
    heartbeat_event = 2,
    expired_event = 3,
    _other_event = 4,
    friend_event = 5,
    foe_event = 6,
    maybe_event = 7,
    yarly_event = 8,
    icanhaz_event = 9,
    nom_event = 10,
    hugz_event = 11,
    kthxbai_event = 12,
    dispatch_event = 13,
    ok_event = 14,
    finished_event = 15
} event_t;


//  ---------------------------------------------------------------------
//  Context for the server thread

typedef struct {
    //  Properties accessible to client actions
    zlist_t *subs;              //  Client subscriptions
    zlist_t *mounts;            //  Mount points        

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
} server_t;

//  ---------------------------------------------------------------------
//  Context for each client connection

typedef struct {
    //  Properties accessible to client actions
    int64_t heartbeat;          //  Heartbeat interval
    event_t next_event;         //  Next event
    byte identity [FMQ_MSG_IDENTITY_SIZE];          
    size_t credit;              //  Credit remaining
    zlist_t *patches;           //  Patches to send 
    size_t patch_nbr;           //  Sequence number 

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
} client_t;


static void
server_client_execute (server_t *server, client_t *client, int event);

//  --------------------------------------------------------------------------
//  Subscription object

typedef struct {
    client_t *client;           //  Always refers to live client
    char *path;                 //  Path client is subscribed to
} sub_t;


//  --------------------------------------------------------------------------
//  Constructor

static sub_t *
sub_new (client_t *client, char *path)
{
    sub_t *self = (sub_t *) zmalloc (sizeof (sub_t));
    self->client = client;
    self->path = strdup (path);
    return self;
}


//  --------------------------------------------------------------------------
//  Destructor

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

//  --------------------------------------------------------------------------
//  Mount point in memory

typedef struct {
    char *fullpath;         //  Root + path
    char *path;             //  Path (after root)
    fmq_dir_t *dir;         //  Directory tree
} mount_t;


//  --------------------------------------------------------------------------
//  Constructor
//  Loads directory tree if possible

static mount_t *
mount_new (char *root, char *path)
{
    //  Mount path must start with '/'
    //  We'll do better error handling later
    assert (*path == '/');
    
    mount_t *self = (mount_t *) zmalloc (sizeof (mount_t));
    self->fullpath = (char *) malloc (strlen (root) + strlen (path) + 1);
    sprintf (self->fullpath, "%s%s", root, path);
    self->path = strdup (path);
    self->dir = fmq_dir_new (self->fullpath, NULL);
    return self;
}


//  --------------------------------------------------------------------------
//  Destructor

static void
mount_destroy (mount_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        mount_t *self = *self_p;
        free (self->fullpath);
        free (self->path);
        fmq_dir_destroy (&self->dir);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Reloads directory tree and returns true if changed, false if the same

static bool
mount_refresh (mount_t *self, server_t *server)
{
    bool changed = false;

    //  Get latest snapshot and build a patches list if it's changed
    fmq_dir_t *latest = fmq_dir_new (self->fullpath, NULL);
    zlist_t *patches = fmq_dir_diff (self->dir, latest);

    //  Drop old directory and replace with latest version
    fmq_dir_destroy (&self->dir);
    self->dir = latest;

    //  Copy new patches to clients' patches list
    sub_t *sub = (sub_t *) zlist_first (server->subs);
    while (sub) {
        //  Is subscription path a strict prefix of the mount path?
        if (strncmp (self->path, sub->path, strlen (sub->path)) == 0) {
            fmq_patch_t *patch = (fmq_patch_t *) zlist_first (patches);
            while (patch) {
                zlist_append (sub->client->patches, fmq_patch_dup (patch));
                fmq_patch_set_number (patch, sub->client->patch_nbr++);
                patch = (fmq_patch_t *) zlist_next (patches);
                changed = true;
            }
        }
        sub = (sub_t *) zlist_next (server->subs);
    }
    zlist_destroy (&patches);
    return changed;
}

//  Client hash function that checks if client is alive
static int
client_dispatch (const char *key, void *client, void *server)
{
    server_client_execute ((server_t *) server, (client_t *) client, dispatch_event);
    return 0;
}


//  Client methods

static client_t *
client_new (char *hashkey, zframe_t *address)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
    self->hashkey = hashkey;
    self->state = start_state;
    self->address = zframe_dup (address);
    self->reply = fmq_msg_new (0);
    fmq_msg_address_set (self->reply, self->address);
    self->patches = zlist_new ();
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
        while (zlist_size (self->patches)) {                               
            fmq_patch_t *patch = (fmq_patch_t *) zlist_pop (self->patches);
            fmq_patch_destroy (&patch);                                    
        }                                                                  
        zlist_destroy (&self->patches);                                    
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

//  Client hash function that checks if client is alive
static int
client_ping (const char *key, void *client, void *argument)
{
    client_t *self = (client_t *) client;
    //  Expire client if it's not answered us in a while
    if (zclock_time () >= self->expires_at && self->expires_at) {
        //  In case dialog doesn't handle expired_event by destroying
        //  client, set expires_at to zero to prevent busy looping
        self->expires_at = 0;
        server_client_execute ((server_t *) argument, self, expired_event);
    }
    else
    //  Check whether to send heartbeat to client
    if (zclock_time () >= self->heartbeat_at) {
        server_client_execute ((server_t *) argument, self, heartbeat_event);
        self->heartbeat_at = zclock_time () + self->heartbeat;
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


//  Server methods

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
    //  Default root            
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
            char *path = fmq_config_resolve (section, "path", "?");
            mount_t *mount = mount_new (                                             
                fmq_config_resolve (self->config, "server/root", "./fmqroot"), path);
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
        char *path = zmsg_popstr (msg);
        mount_t *mount = mount_new (                                             
            fmq_config_resolve (self->config, "server/root", "./fmqroot"), path);
        zlist_append (self->mounts, mount);                                      
        free (path);
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
}

//  Custom actions for state machine

static void
terminate_the_client (server_t *self, client_t *client)
{
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
track_client_identity (server_t *self, client_t *client)
{
    memcpy (client->identity, fmq_msg_identity (client->request), FMQ_MSG_IDENTITY_SIZE);
}

static void
try_anonymous_access (server_t *self, client_t *client)
{
    if (atoi (fmq_config_resolve (self->config, "security/anonymous", "0")) == 1)
        client->next_event = friend_event;                                       
    else                                                                         
    if (atoi (fmq_config_resolve (self->config, "security/plain", "0")) == 1)    
        client->next_event = maybe_event;                                        
    else                                                                         
        client->next_event = foe_event;                                          
}

static void
list_security_mechanisms (server_t *self, client_t *client)
{
    if (atoi (fmq_config_resolve (self->config, "security/anonymous", "0")) == 1)
        fmq_msg_mechanisms_append (client->reply, "ANONYMOUS");                  
    if (atoi (fmq_config_resolve (self->config, "security/plain", "0")) == 1)    
        fmq_msg_mechanisms_append (client->reply, "PLAIN");                      
}

static void
try_security_mechanism (server_t *self, client_t *client)
{
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
store_client_subscription (server_t *self, client_t *client)
{
    //  Store subscription along with any previous ones                
    //  Check we don't already have a subscription for this client/path
    sub_t *sub = (sub_t *) zlist_first (self->subs);                   
    char *path = fmq_msg_path (client->request);                       
    while (sub) {                                                      
        if (client == sub->client && streq (path, sub->path))          
            return;                                                    
        sub = (sub_t *) zlist_next (self->subs);                       
    }                                                                  
                                                                       
    //  New subscription for this client, append to our list           
    sub = sub_new (client, fmq_msg_path (client->request));            
    zlist_append (self->subs, sub);                                    
}

static void
store_client_credit (server_t *self, client_t *client)
{
    client->credit += fmq_msg_credit (client->request);
}

static void
monitor_the_server (server_t *self, client_t *client)
{
    mount_t *mount = (mount_t *) zlist_first (self->mounts);     
    while (mount) {                                              
        if (mount_refresh (mount, self))                         
            zhash_foreach (self->clients, client_dispatch, self);
        mount = (mount_t *) zlist_next (self->mounts);           
    }                                                            
}

static void
get_next_patch_for_client (server_t *self, client_t *client)
{
    fmq_patch_t *patch = (fmq_patch_t *) zlist_pop (client->patches);                
    if (patch) {                                                                     
        client->next_event = ok_event;                                               
        switch (fmq_patch_op (patch)) {                                              
            case patch_create:                                                       
                fmq_msg_operation_set (client->reply, FMQ_MSG_FILE_CREATE);          
                printf ("I: created: %s\n", fmq_file_name (fmq_patch_file (patch))); 
                break;                                                               
            case patch_delete:                                                       
                fmq_msg_operation_set (client->reply, FMQ_MSG_FILE_DELETE);          
                printf ("I: deleted: %s\n", fmq_file_name (fmq_patch_file (patch))); 
                break;                                                               
            case patch_update:                                                       
                fmq_msg_operation_set (client->reply, FMQ_MSG_FILE_UPDATE);          
                printf ("I: updated: %s\n", fmq_file_name (fmq_patch_file (patch))); 
                break;                                                               
        }                                                                            
        fmq_msg_filename_set (client->reply, fmq_file_name (fmq_patch_file (patch)));
    }                                                                                
    else                                                                             
        client->next_event = finished_event;                                         
}

//  Execute state machine as long as we have events

static void
server_client_execute (server_t *self, client_t *client, int event)
{
    client->next_event = event;
    while (client->next_event) {
        client->event = client->next_event;
        client->next_event = 0;
        switch (client->state) {
            case start_state:
                if (client->event == ohai_event) {
                    track_client_identity (self, client);
                    try_anonymous_access (self, client);
                    client->state = checking_client_state;
                }
                else
                if (client->event == heartbeat_event) {
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else {
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    terminate_the_client (self, client);
                }
                break;

            case checking_client_state:
                if (client->event == friend_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_OHAI_OK);
                    client->state = ready_state;
                }
                else
                if (client->event == foe_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_SRSLY);
                    terminate_the_client (self, client);
                }
                else
                if (client->event == maybe_event) {
                    list_security_mechanisms (self, client);
                    fmq_msg_id_set (client->reply, FMQ_MSG_ORLY);
                    client->state = challenging_client_state;
                }
                else
                if (client->event == heartbeat_event) {
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else {
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    terminate_the_client (self, client);
                }
                break;

            case challenging_client_state:
                if (client->event == yarly_event) {
                    try_security_mechanism (self, client);
                    client->state = checking_client_state;
                }
                else
                if (client->event == heartbeat_event) {
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else {
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    terminate_the_client (self, client);
                }
                break;

            case ready_state:
                if (client->event == icanhaz_event) {
                    store_client_subscription (self, client);
                    fmq_msg_id_set (client->reply, FMQ_MSG_ICANHAZ_OK);
                }
                else
                if (client->event == nom_event) {
                    store_client_credit (self, client);
                    get_next_patch_for_client (self, client);
                    client->state = dispatching_state;
                }
                else
                if (client->event == hugz_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_HUGZ_OK);
                }
                else
                if (client->event == kthxbai_event) {
                    terminate_the_client (self, client);
                }
                else
                if (client->event == dispatch_event) {
                    get_next_patch_for_client (self, client);
                    client->state = dispatching_state;
                }
                else
                if (client->event == heartbeat_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_HUGZ);
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else {
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    terminate_the_client (self, client);
                }
                break;

            case dispatching_state:
                if (client->event == ok_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_CHEEZBURGER);
                    get_next_patch_for_client (self, client);
                }
                else
                if (client->event == finished_event) {
                    client->state = ready_state;
                }
                else
                if (client->event == heartbeat_event) {
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else {
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    terminate_the_client (self, client);
                }
                break;

        }
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
    //  No clean way to wait for a background thread to exit
    //  Under valgrind this will randomly show as leakage
    zclock_sleep (500);
    //  Run selftest using 'anonymous.cfg' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_configure (self, "anonymous.cfg");
    fmq_server_bind (self, "tcp://*:6000");
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
    //  No clean way to wait for a background thread to exit
    //  Under valgrind this will randomly show as leakage
    zclock_sleep (500);
    //  Run selftest using 'server_test.cfg' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_configure (self, "server_test.cfg");
    fmq_server_bind (self, "tcp://*:6000");
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
    //  No clean way to wait for a background thread to exit
    //  Under valgrind this will randomly show as leakage
    zclock_sleep (500);
    zctx_destroy (&ctx);
    printf ("OK\n");
    return 0;
}
