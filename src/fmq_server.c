/*  =========================================================================
    fmq_server - a filemq server

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

/*
@header
    the fmq_server class implements a generic filemq server.
@discuss
@end
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

int
fmq_server_bind (fmq_server_t *self, const char *endpoint)
{
    assert (self);
    assert (endpoint);
    zstr_sendm (self->pipe, "BIND");
    zstr_sendf (self->pipe, "%s", endpoint);
    char *reply = zstr_recv (self->pipe);
    int rc = atoi (reply);
    free (reply);
    return rc;
}


//  --------------------------------------------------------------------------

void
fmq_server_publish (fmq_server_t *self, const char *location, const char *alias)
{
    assert (self);
    assert (location);
    assert (alias);
    zstr_sendm (self->pipe, "PUBLISH");
    zstr_sendfm (self->pipe, "%s", location);
    zstr_sendf (self->pipe, "%s", alias);
}


//  --------------------------------------------------------------------------

void
fmq_server_set_anonymous (fmq_server_t *self, long enabled)
{
    assert (self);
    zstr_sendm (self->pipe, "SET ANONYMOUS");
    zstr_sendf (self->pipe, "%ld", enabled);
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
    send_chunk_event = 14,
    send_delete_event = 15,
    next_patch_event = 16,
    no_credit_event = 17,
    finished_event = 18
} event_t;


//  Forward declarations
typedef struct _server_t server_t;
typedef struct _client_t client_t;

//  There's no point making these configurable
#define CHUNK_SIZE      1000000

//  Forward declarations
typedef struct _sub_t sub_t;

//  Forward declarations
typedef struct _mount_t mount_t;


//  ---------------------------------------------------------------------
//  Context for the server thread

struct _server_t {
    //  Properties accessible to server actions
    zlist_t *mounts;            //  Mount points
    int port;                   //  Server port 
    //  Properties you should NOT touch
    zctx_t *ctx;                //  Own CZMQ context
    void *pipe;                 //  Socket to back to caller
    void *router;               //  Socket to talk to clients
    zhash_t *clients;           //  Clients we've connected to
    bool stopped;               //  Has server stopped?
    zconfig_t *config;          //  Configuration tree
    int monitor;                //  Monitor interval
    int64_t monitor_at;         //  Next monitor at this time
    int heartbeat;              //  Client heartbeat interval
};

//  ---------------------------------------------------------------------
//  Context for each client connection

struct _client_t {
    //  Properties accessible to client actions
    int heartbeat;              //  Client heartbeat interval
    event_t next_event;         //  Next event
    size_t credit;              //  Credit remaining           
    zlist_t *patches;           //  Patches to send            
    fmq_patch_t *patch;         //  Current patch              
    fmq_file_t *file;           //  Current file we're sending 
    off_t offset;               //  Offset of next read in file
    int64_t sequence;           //  Sequence number for chunk  
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
};

static void
server_client_execute (server_t *server, client_t *client, int event);

//  Client hash function that checks if client is alive
static int
client_dispatch (const char *key, void *client, void *server)
{
    server_client_execute ((server_t *) server, (client_t *) client, dispatch_event);
    return 0;
}

//  --------------------------------------------------------------------------
//  Subscription object

struct _sub_t {
    client_t *client;           //  Always refers to live client
    char *path;                 //  Path client is subscribed to
    zhash_t *cache;             //  Client's cache list
};

static int
s_resolve_cache_path (const char *key, void *item, void *argument);

//  --------------------------------------------------------------------------
//  Constructor

static sub_t *
sub_new (client_t *client, char *path, zhash_t *cache)
{
    sub_t *self = (sub_t *) zmalloc (sizeof (sub_t));
    self->client = client;
    self->path = strdup (path);
    self->cache = zhash_dup (cache);
    zhash_foreach (self->cache, s_resolve_cache_path, self);
    return self;
}

//  Cached filenames may be local, in which case prefix them with
//  the subscription path so we can do a consistent match.

static int
s_resolve_cache_path (const char *key, void *item, void *argument)
{
    sub_t *self = (sub_t *) argument;
    if (*key != '/') {
        char *new_key = malloc (strlen (self->path) + strlen (key) + 2);
        sprintf (new_key, "%s/%s", self->path, key);
        zhash_rename (self->cache, key, new_key);
        free (new_key);
    }
    return 0;
}


//  --------------------------------------------------------------------------
//  Destructor

static void
sub_destroy (sub_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sub_t *self = *self_p;
        zhash_destroy (&self->cache);
        free (self->path);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Add patch to sub client patches list

static void
sub_patch_add (sub_t *self, fmq_patch_t *patch)
{
    //  Skip file creation if client already has identical file
    fmq_patch_digest_set (patch);
    if (fmq_patch_op (patch) == patch_create) {
        char *digest = zhash_lookup (self->cache, fmq_patch_virtual (patch));
        if (digest && strcasecmp (digest, fmq_patch_digest (patch)) == 0)
            return;             //  Just skip patch for this client
    }
    //  Remove any previous patches for the same file
    fmq_patch_t *existing = (fmq_patch_t *) zlist_first (self->client->patches);
    while (existing) {
        if (streq (fmq_patch_virtual (patch), fmq_patch_virtual (existing))) {
            zlist_remove (self->client->patches, existing);
            fmq_patch_destroy (&existing);
            break;
        }
        existing = (fmq_patch_t *) zlist_next (self->client->patches);
    }
    if (fmq_patch_op (patch) == patch_create)
        zhash_insert (self->cache,
            fmq_patch_digest (patch), fmq_patch_virtual (patch));
    //  Track that we've queued patch for client, so we don't do it twice
    zlist_append (self->client->patches, fmq_patch_dup (patch));
}

//  --------------------------------------------------------------------------
//  Mount point in memory

struct _mount_t {
    char *location;         //  Physical location
    char *alias;            //  Alias into our tree
    fmq_dir_t *dir;         //  Directory snapshot
    zlist_t *subs;          //  Client subscriptions
};


//  --------------------------------------------------------------------------
//  Constructor
//  Loads directory tree if possible

static mount_t *
mount_new (char *location, char *alias)
{
    //  Mount path must start with '/'
    //  We'll do better error handling later
    assert (*alias == '/');
    
    mount_t *self = (mount_t *) zmalloc (sizeof (mount_t));
    self->location = strdup (location);
    self->alias = strdup (alias);
    self->dir = fmq_dir_new (self->location, NULL);
    self->subs = zlist_new ();
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
        free (self->location);
        free (self->alias);
        //  Destroy subscriptions
        while (zlist_size (self->subs)) {
            sub_t *sub = (sub_t *) zlist_pop (self->subs);
            sub_destroy (&sub);
        }
        zlist_destroy (&self->subs);
        fmq_dir_destroy (&self->dir);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Reloads directory tree and returns true if activity, false if the same

static bool
mount_refresh (mount_t *self, server_t *server)
{
    bool activity = false;

    //  Get latest snapshot and build a patches list for any changes
    fmq_dir_t *latest = fmq_dir_new (self->location, NULL);
    zlist_t *patches = fmq_dir_diff (self->dir, latest, self->alias);

    //  Drop old directory and replace with latest version
    fmq_dir_destroy (&self->dir);
    self->dir = latest;

    //  Copy new patches to clients' patches list
    sub_t *sub = (sub_t *) zlist_first (self->subs);
    while (sub) {
        fmq_patch_t *patch = (fmq_patch_t *) zlist_first (patches);
        while (patch) {
            sub_patch_add (sub, patch);
            patch = (fmq_patch_t *) zlist_next (patches);
            activity = true;
        }
        sub = (sub_t *) zlist_next (self->subs);
    }
    
    //  Destroy patches, they've all been copied
    while (zlist_size (patches)) {
        fmq_patch_t *patch = (fmq_patch_t *) zlist_pop (patches);
        fmq_patch_destroy (&patch);
    }
    zlist_destroy (&patches);
    return activity;
}


//  --------------------------------------------------------------------------
//  Store subscription for mount point

static void
mount_sub_store (mount_t *self, client_t *client, fmq_msg_t *request)
{
    //  Store subscription along with any previous ones
    //  Coalesce subscriptions that are on same path
    char *path = fmq_msg_path (request);
    sub_t *sub = (sub_t *) zlist_first (self->subs);
    while (sub) {
        if (client == sub->client) {
            //  If old subscription is superset/same as new, ignore new
            if (strncmp (path, sub->path, strlen (sub->path)) == 0)
                return;
            else
            //  If new subscription is superset of old one, remove old
            if (strncmp (sub->path, path, strlen (path)) == 0) {
                zlist_remove (self->subs, sub);
                sub_destroy (&sub);
                sub = (sub_t *) zlist_first (self->subs);
            }
            else
                sub = (sub_t *) zlist_next (self->subs);
        }
        else
            sub = (sub_t *) zlist_next (self->subs);
    }
    //  New subscription for this client, append to our list
    sub = sub_new (client, path, fmq_msg_cache (request));
    zlist_append (self->subs, sub);

    //  If client requested resync, send full mount contents now
    if (fmq_msg_options_number (client->request, "RESYNC", 0) == 1) {
        zlist_t *patches = fmq_dir_resync (self->dir, self->alias);
        while (zlist_size (patches)) {
            fmq_patch_t *patch = (fmq_patch_t *) zlist_pop (patches);
            sub_patch_add (sub, patch);
            fmq_patch_destroy (&patch);
        }
        zlist_destroy (&patches);
    }
}


//  --------------------------------------------------------------------------
//  Purge subscriptions for a specified client

static void
mount_sub_purge (mount_t *self, client_t *client)
{
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
}


//  Client methods

static client_t *
client_new (zframe_t *address)
{
    client_t *self = (client_t *) zmalloc (sizeof (client_t));
    self->state = start_state;
    self->hashkey = zframe_strhex (address);
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

//  Callback when we remove client from 'clients' hash table
static void
client_free (void *argument)
{
    client_t *client = (client_t *) argument;
    client_destroy (&client);
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

//  Server methods

static void
server_config_self (server_t *self)
{
    //  Get standard server configuration
    self->monitor = atoi (
        zconfig_resolve (self->config, "server/monitor", "1")) * 1000;
    self->heartbeat = atoi (
        zconfig_resolve (self->config, "server/heartbeat", "1")) * 1000;
    self->monitor_at = zclock_time () + self->monitor;
}

static server_t *
server_new (zctx_t *ctx, void *pipe)
{
    server_t *self = (server_t *) zmalloc (sizeof (server_t));
    self->ctx = ctx;
    self->pipe = pipe;
    self->router = zsocket_new (self->ctx, ZMQ_ROUTER);
    self->clients = zhash_new ();
    self->config = zconfig_new ("root", NULL);
    server_config_self (self);
    self->mounts = zlist_new ();
    return self;
}

static void
server_destroy (server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        server_t *self = *self_p;
        zsocket_destroy (self->ctx, self->router);
        zconfig_destroy (&self->config);
        zhash_destroy (&self->clients);
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
    //  Apply echo commands and class methods
    zconfig_t *section = zconfig_child (self->config);
    while (section) {
        zconfig_t *entry = zconfig_child (section);
        while (entry) {
            if (streq (zconfig_name (entry), "echo"))
                zclock_log (zconfig_value (entry));
            entry = zconfig_next (entry);
        }
        if (streq (zconfig_name (section), "bind")) {
            char *endpoint = zconfig_resolve (section, "endpoint", "?");
            self->port = zsocket_bind (self->router, endpoint);
        }
        else
        if (streq (zconfig_name (section), "publish")) {
            char *location = zconfig_resolve (section, "location", "?");
            char *alias = zconfig_resolve (section, "alias", "?");
            mount_t *mount = mount_new (location, alias);
            zlist_append (self->mounts, mount);          
        }
        else
        if (streq (zconfig_name (section), "set_anonymous")) {
            long enabled = atoi (zconfig_resolve (section, "enabled", ""));
            //  Enable anonymous access without a config file                        
            zconfig_path_set (self->config, "security/anonymous", enabled? "1" :"0");
        }
        section = zconfig_next (section);
    }
    server_config_self (self);
}

//  Process message from pipe
static void
server_control_message (server_t *self)
{
    zmsg_t *msg = zmsg_recv (self->pipe);
    char *method = zmsg_popstr (msg);
    if (streq (method, "BIND")) {
        char *endpoint = zmsg_popstr (msg);
        self->port = zsocket_bind (self->router, endpoint);
        zstr_sendf (self->pipe, "%d", self->port);
        free (endpoint);
    }
    else
    if (streq (method, "PUBLISH")) {
        char *location = zmsg_popstr (msg);
        char *alias = zmsg_popstr (msg);
        mount_t *mount = mount_new (location, alias);
        zlist_append (self->mounts, mount);          
        free (location);
        free (alias);
    }
    else
    if (streq (method, "SET ANONYMOUS")) {
        char *enabled_string = zmsg_popstr (msg);
        long enabled = atoi (enabled_string);
        free (enabled_string);
        //  Enable anonymous access without a config file                        
        zconfig_path_set (self->config, "security/anonymous", enabled? "1" :"0");
    }
    else
    if (streq (method, "CONFIG")) {
        char *config_file = zmsg_popstr (msg);
        zconfig_destroy (&self->config);
        self->config = zconfig_load (config_file);
        if (self->config)
            server_apply_config (self);
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
        server_config_self (self);
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
    mount_t *mount = (mount_t *) zlist_first (self->mounts);
    while (mount) {                                         
        mount_sub_purge (mount, client);                    
        mount = (mount_t *) zlist_next (self->mounts);      
    }                                                       
    client->next_event = terminate_event;                   
}

static void
try_anonymous_access (server_t *self, client_t *client)
{
    if (atoi (zconfig_resolve (self->config, "security/anonymous", "0")) == 1)
        client->next_event = friend_event;                                    
    else                                                                      
    if (atoi (zconfig_resolve (self->config, "security/plain", "0")) == 1)    
        client->next_event = maybe_event;                                     
    else                                                                      
        client->next_event = foe_event;                                       
}

static void
list_security_mechanisms (server_t *self, client_t *client)
{
    if (atoi (zconfig_resolve (self->config, "security/anonymous", "0")) == 1)
        fmq_msg_mechanisms_append (client->reply, "ANONYMOUS");               
    if (atoi (zconfig_resolve (self->config, "security/plain", "0")) == 1)    
        fmq_msg_mechanisms_append (client->reply, "PLAIN");                   
}

static void
try_security_mechanism (server_t *self, client_t *client)
{
    client->next_event = foe_event;                                                          
    char *login, *password;                                                                  
    if (streq (fmq_msg_mechanism (client->request), "PLAIN")                                 
    &&  fmq_sasl_plain_decode (fmq_msg_response (client->request), &login, &password) == 0) {
        zconfig_t *account = zconfig_locate (self->config, "security/plain/account");        
        while (account) {                                                                    
            if (streq (zconfig_resolve (account, "login", ""), login)                        
            &&  streq (zconfig_resolve (account, "password", ""), password)) {               
                client->next_event = friend_event;                                           
                break;                                                                       
            }                                                                                
            account = zconfig_next (account);                                                
        }                                                                                    
    }                                                                                        
    free (login);                                                                            
    free (password);                                                                         
}

static void
store_client_subscription (server_t *self, client_t *client)
{
    //  Find mount point with longest match to subscription         
    char *path = fmq_msg_path (client->request);                    
                                                                    
    mount_t *check = (mount_t *) zlist_first (self->mounts);        
    mount_t *mount = check;                                         
    while (check) {                                                 
        //  If check->alias is prefix of path and alias is          
        //  longer than current mount then we have a new mount      
        if (strncmp (path, check->alias, strlen (check->alias)) == 0
        &&  strlen (check->alias) > strlen (mount->alias))          
            mount = check;                                          
        check = (mount_t *) zlist_next (self->mounts);              
    }                                                               
    mount_sub_store (mount, client, client->request);               
}

static void
store_client_credit (server_t *self, client_t *client)
{
    client->credit += fmq_msg_credit (client->request);
}

static void
monitor_the_server (server_t *self, client_t *client)
{
    bool activity = false;                                   
    mount_t *mount = (mount_t *) zlist_first (self->mounts); 
    while (mount) {                                          
        if (mount_refresh (mount, self))                     
            activity = true;                                 
        mount = (mount_t *) zlist_next (self->mounts);       
    }                                                        
    if (activity)                                            
        zhash_foreach (self->clients, client_dispatch, self);
}

static void
get_next_patch_for_client (server_t *self, client_t *client)
{
    //  Get next patch for client if we're not doing one already                      
    if (client->patch == NULL)                                                        
        client->patch = (fmq_patch_t *) zlist_pop (client->patches);                  
    if (client->patch == NULL) {                                                      
        client->next_event = finished_event;                                          
        return;                                                                       
    }                                                                                 
    //  Get virtual filename from patch                                               
    fmq_msg_filename_set (client->reply, fmq_patch_virtual (client->patch));          
                                                                                      
    //  We can process a delete patch right away                                      
    if (fmq_patch_op (client->patch) == patch_delete) {                               
        fmq_msg_sequence_set (client->reply, client->sequence++);                     
        fmq_msg_operation_set (client->reply, FMQ_MSG_FILE_DELETE);                   
        client->next_event = send_delete_event;                                       
                                                                                      
        //  No reliability in this version, assume patch delivered safely             
        fmq_patch_destroy (&client->patch);                                           
    }                                                                                 
    else                                                                              
    if (fmq_patch_op (client->patch) == patch_create) {                               
        //  Create patch refers to file, open that for input if needed                
        if (client->file == NULL) {                                                   
            client->file = fmq_file_dup (fmq_patch_file (client->patch));             
            if (fmq_file_input (client->file)) {                                      
                //  File no longer available, skip it                                 
                fmq_patch_destroy (&client->patch);                                   
                fmq_file_destroy (&client->file);                                     
                client->next_event = next_patch_event;                                
                return;                                                               
            }                                                                         
            client->offset = 0;                                                       
        }                                                                             
        //  Get next chunk for file                                                   
        fmq_chunk_t *chunk = fmq_file_read (client->file, CHUNK_SIZE, client->offset);
        assert (chunk);                                                               
                                                                                      
        //  Check if we have the credit to send chunk                                 
        if (fmq_chunk_size (chunk) <= client->credit) {                               
            fmq_msg_sequence_set (client->reply, client->sequence++);                 
            fmq_msg_operation_set (client->reply, FMQ_MSG_FILE_CREATE);               
            fmq_msg_offset_set (client->reply, client->offset);                       
            fmq_msg_chunk_set (client->reply, zframe_new (                            
                fmq_chunk_data (chunk),                                               
                fmq_chunk_size (chunk)));                                             
                                                                                      
            client->offset += fmq_chunk_size (chunk);                                 
            client->credit -= fmq_chunk_size (chunk);                                 
            client->next_event = send_chunk_event;                                    
                                                                                      
            //  Zero-sized chunk means end of file                                    
            if (fmq_chunk_size (chunk) == 0) {                                        
                fmq_file_destroy (&client->file);                                     
                fmq_patch_destroy (&client->patch);                                   
            }                                                                         
        }                                                                             
        else                                                                          
            client->next_event = no_credit_event;                                     
                                                                                      
        fmq_chunk_destroy (&chunk);                                                   
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
        switch (client->state) {
            case start_state:
                if (client->event == ohai_event) {
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
                    //  Process all other events
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    terminate_the_client (self, client);
                }
                break;

            case checking_client_state:
                if (client->event == friend_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_OHAI_OK);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    client->state = ready_state;
                }
                else
                if (client->event == foe_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_SRSLY);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    terminate_the_client (self, client);
                }
                else
                if (client->event == maybe_event) {
                    list_security_mechanisms (self, client);
                    fmq_msg_id_set (client->reply, FMQ_MSG_ORLY);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    client->state = challenging_client_state;
                }
                else
                if (client->event == heartbeat_event) {
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else
                if (client->event == ohai_event) {
                    try_anonymous_access (self, client);
                    client->state = checking_client_state;
                }
                else {
                    //  Process all other events
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
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
                else
                if (client->event == ohai_event) {
                    try_anonymous_access (self, client);
                    client->state = checking_client_state;
                }
                else {
                    //  Process all other events
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    terminate_the_client (self, client);
                }
                break;

            case ready_state:
                if (client->event == icanhaz_event) {
                    store_client_subscription (self, client);
                    fmq_msg_id_set (client->reply, FMQ_MSG_ICANHAZ_OK);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
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
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
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
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                }
                else
                if (client->event == expired_event) {
                    terminate_the_client (self, client);
                }
                else
                if (client->event == ohai_event) {
                    try_anonymous_access (self, client);
                    client->state = checking_client_state;
                }
                else {
                    //  Process all other events
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    terminate_the_client (self, client);
                }
                break;

            case dispatching_state:
                if (client->event == send_chunk_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_CHEEZBURGER);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    get_next_patch_for_client (self, client);
                }
                else
                if (client->event == send_delete_event) {
                    fmq_msg_id_set (client->reply, FMQ_MSG_CHEEZBURGER);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    get_next_patch_for_client (self, client);
                }
                else
                if (client->event == next_patch_event) {
                    get_next_patch_for_client (self, client);
                }
                else
                if (client->event == no_credit_event) {
                    client->state = ready_state;
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
                else
                if (client->event == ohai_event) {
                    try_anonymous_access (self, client);
                    client->state = checking_client_state;
                }
                else {
                    //  Process all other events
                    fmq_msg_id_set (client->reply, FMQ_MSG_RTFM);
                    fmq_msg_send (&client->reply, client->router);
                    client->reply = fmq_msg_new (0);
                    fmq_msg_address_set (client->reply, client->address);
                    terminate_the_client (self, client);
                }
                break;

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
        client = client_new (fmq_msg_address (request));
        client->heartbeat = self->heartbeat;
        client->router = self->router;
        zhash_insert (self->clients, hashkey, client);
        zhash_freefn (self->clients, hashkey, client_free);
    }
    free (hashkey);
    if (client->request)
        fmq_msg_destroy (&client->request);
    client->request = request;

    //  Any input from client counts as heartbeat
    client->heartbeat_at = zclock_time () + client->heartbeat;
    //  Any input from client counts as activity
    client->expires_at = zclock_time () + client->heartbeat * 3;
    
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
    while (!self->stopped && !zctx_interrupted) {
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
    zsocket_connect (dealer, "tcp://localhost:5670");
    
    fmq_msg_t *request, *reply;
    
    //  Run selftest using '' configuration
    self = fmq_server_new ();
    assert (self);
    int port = fmq_server_bind (self, "tcp://*:5670");
    assert (port == 5670);                            
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
    port = fmq_server_bind (self, "tcp://*:5670");
    assert (port == 5670);                        
    request = fmq_msg_new (FMQ_MSG_OHAI);
    fmq_msg_send (&request, dealer);
    reply = fmq_msg_recv (dealer);
    assert (reply);
    assert (fmq_msg_id (reply) == FMQ_MSG_OHAI_OK);
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
    //  Run selftest using 'server_test.cfg' configuration
    self = fmq_server_new ();
    assert (self);
    fmq_server_configure (self, "server_test.cfg");
    port = fmq_server_bind (self, "tcp://*:5670");
    assert (port == 5670);                        
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

    zctx_destroy (&ctx);
    //  @end

    //  No clean way to wait for a background thread to exit
    //  Under valgrind this will randomly show as leakage
    //  Reduce this by giving server thread time to exit
    zclock_sleep (200);
    printf ("OK\n");
    return 0;
}
