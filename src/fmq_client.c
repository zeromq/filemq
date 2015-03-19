/*  =========================================================================
    fmq_client - FILEMQ Client

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of FileMQ, a C implemenation of the protocol:    
    https://github.com/danriegsecker/filemq2.                          
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*
@header
    Description of class for man page.
@discuss
    Detailed discussion of the class, if any.
@end
*/

//  TODO: Change these to match your project's needs
#include "filemq_classes.h"

//  Forward reference to method arguments structure
typedef struct _client_args_t client_args_t;

//  Additional forward declarations
typedef struct _sub_t sub_t;

//  There's no point making these configurable
#define CREDIT_SLICE    1000000
#define CREDIT_MINIMUM  (CREDIT_SLICE * 4) + 1

//  This structure defines the context for a client connection
typedef struct {
    //  These properties must always be present in the client_t
    //  and are set by the generated engine. The cmdpipe gets
    //  messages sent to the actor; the msgpipe may be used for
    //  faster asynchronous message flows.
    zsock_t *cmdpipe;           //  Command pipe to/from caller API
    zsock_t *msgpipe;           //  Message pipe to/from caller API
    zsock_t *dealer;            //  Socket to talk to server
    fmq_msg_t *message;         //  Message to/from server
    client_args_t *args;        //  Arguments from methods

    //  TODO: Add specific properties for your application
    size_t credit;              //  Current credit pending
    zfile_t *file;              //  File we're currently writing
    char *inbox;                //  Path where files will be stored
    zlist_t *subs;              //  Our subscriptions
    sub_t *sub;                 //  Subscription we're sending
    int timeouts;               //  Count the timeouts
} client_t;

//  Include the generated client engine
#include "fmq_client_engine.inc"

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

//  Allocate properties and structures for a new client instance.
//  Return 0 if OK, -1 if failed

static int
client_initialize (client_t *self)
{
    zsys_info ("client is initializing");
    self->subs = zlist_new ();
    self->credit = 0;
    self->inbox = NULL;
    self->timeouts = 0;
    return 0;
}

//  Free properties and structures for a client instance

static void
client_terminate (client_t *self)
{
    //  Destroy properties here
    zsys_info ("client_terminate: client is terminating");
    while (zlist_size (self->subs)) {
        sub_t *sub = (sub_t *) zlist_pop (self->subs);
        zsys_debug ("destroy sub %s", sub->path);
        sub_destroy (&sub);
    }
    zlist_destroy (&self->subs);
    zsys_debug ("client_terminate: subscription list destroyed");
    if (self->inbox) {
        free (self->inbox);
        zsys_debug ("client_terminate: inbox freed");
    }
}


//  ---------------------------------------------------------------------------
//  connect_to_server_endpoint
//

static void
connect_to_server_endpoint (client_t *self)
{
    if (zsock_connect (self->dealer, "%s", self->args->endpoint)) {
        engine_set_exception (self, connect_error_event);
        zsys_warning ("could not connect to %s", self->args->endpoint);
    }
}


//  ---------------------------------------------------------------------------
//  use_connect_timeout
//

static void
use_connect_timeout (client_t *self)
{
    engine_set_timeout (self, self->args->timeout);
}


//  ---------------------------------------------------------------------------
//  handle_connect_error
//

static void
handle_connect_error (client_t *self)
{
    zsys_warning ("unable to connect to the server");
    engine_set_next_event (self, bombcmd_event);
}


//  ---------------------------------------------------------------------------
//  stayin_alive
//

static void
stayin_alive (client_t *self)
{
    self->timeouts = 0;
}


//  ---------------------------------------------------------------------------
//  connected_to_server
//

static void
connected_to_server (client_t *self)
{
    zsys_debug ("connected to server");
    zsock_send (self->cmdpipe, "si", "SUCCESS", 0);
}


//  ---------------------------------------------------------------------------
//  handle_connect_timeout
//

static void
handle_connect_timeout (client_t *self)
{
    if (self->timeouts <= 3)
        self->timeouts++;
    else {
        zsys_warning ("server did not respond to the request to communicate");
        engine_set_next_event (self, bombcmd_event);
    }
}


//  ---------------------------------------------------------------------------
//  setup_inbox
//

static void
setup_inbox (client_t *self)
{
    if (!self->inbox) {
        self->inbox = strdup (self->args->path);
        zsock_send (self->cmdpipe, "si", "SUCCESS", 0);
    }
    else
        zsock_send (self->cmdpipe, "sis", "FAILURE", -1, "inbox already set");
}


//  ---------------------------------------------------------------------------
//  format_icanhaz_command
//

static void
format_icanhaz_command (client_t *self)
{
    if (!self->inbox) {
        engine_set_exception (self, subscribe_error_event);
        zsys_error ("can't subscribe without inbox set");
        return;
    }
    char *path = strdup (self->args->path);
    if (*path != '/') {
        engine_set_exception (self, subscribe_error_event);
        zsys_error ("unable to subscribe path %s, must start with /", path);
        free (path);
        return;
    }

    self->sub = (sub_t *) zlist_first (self->subs);
    while (self->sub) {
        if (streq (path, self->sub->path)) {
            zsys_warning ("already subscribed to %s", path);
            free (path);
            return;
        }
        self->sub = (sub_t *) zlist_next (self->subs);
    }
    self->sub = sub_new (self, self->inbox, path);
    zlist_append (self->subs, self->sub);
    zsys_debug ("%s added to subscription list", path);
    free (path);

    fmq_msg_set_path (self->message, self->sub->path);
}


//  ---------------------------------------------------------------------------
//  signal_success
//

static void
signal_success (client_t *self)
{
    zsock_send (self->cmdpipe, "si", "SUCCESS", 0);
}


//  ---------------------------------------------------------------------------
//  subscribe_failed
//

static void
subscribe_failed (client_t *self)
{
    zsock_send (self->cmdpipe, "sis", "FAILURE", -1, "subscription failed");
}


//  ---------------------------------------------------------------------------
//  handle_connected_timeout
//

static void
handle_connected_timeout (client_t *self)
{
    if (self->timeouts <= 3)
        self->timeouts++;
    else
        engine_set_next_event (self, bombmsg_event);
}


//  ---------------------------------------------------------------------------
//  signal_subscribe_success
//

static void
signal_subscribe_success (client_t *self)
{
    zsock_send (self->cmdpipe, "si", "SUCCESS", 0);
    size_t credit_to_send = 0;
    while (self->credit < CREDIT_MINIMUM) {
        credit_to_send += CREDIT_SLICE;
        self->credit += CREDIT_SLICE;
    }
    if (credit_to_send) {
        fmq_msg_set_credit (self->message, credit_to_send);
        engine_set_next_event (self, send_credit_event);
    }
}


//  ---------------------------------------------------------------------------
//  handle_subscribe_timeout
//

static void
handle_subscribe_timeout (client_t *self)
{
    if (self->timeouts <= 3)
        self->timeouts++;
    else {
        zsys_warning ("server did not respond to subscription request");
        engine_set_next_event (self, bombcmd_event);
    }
}


//  ---------------------------------------------------------------------------
//  process_the_patch
//

static void
process_the_patch (client_t *self)
{
    const char *filename = fmq_msg_filename (self->message);

    if (*filename != '/') {
        zsys_error ("filename did not start with a \'/\'");
        return;
    }

    sub_t *subscr = (sub_t *) zlist_first (self->subs);
    while (subscr) {
        if (!strncmp (filename, subscr->path, strlen (subscr->path))) {
            filename += strlen (subscr->path);
            zsys_debug ("subscription found for %s", filename);
            break;
        }
    }
    if ('/' == *filename) filename++;

    if (fmq_msg_operation (self->message) == FMQ_MSG_FILE_CREATE) {
        if (self->file == NULL) {
            zsys_debug ("creating file object for %s/%s", self->inbox,
                filename);
            self->file = zfile_new (self->inbox, filename);
            if (zfile_output (self->file)) {
                zsys_warning ("unable to write to file %s/%s", self->inbox,
                    filename);
                //  File not writeable, skip patch
                zfile_destroy (&self->file);
                return;
            }
        }
        //  Try to write, ignore errors in this version
        zchunk_t *chunk = fmq_msg_chunk (self->message);
        if (zchunk_size (chunk) > 0) {
            zsys_debug ("writing chunk at offset %u of %s/%s",
                fmq_msg_offset (self->message), self->inbox, filename);
            zfile_write (self->file, chunk, fmq_msg_offset (self->message));
            self->credit -= zchunk_size (chunk);
        }
        else {
            //  Zero-sized chunk means end of file, so report back to caller
            //  Communicate back to caller via the msgpipe
            zsys_debug ("file complete %s/%s", self->inbox, filename);
            zsock_send (self->msgpipe, "sss", "FILE UPDATED", self->inbox,
                filename);
            zfile_destroy (&self->file);
        }
    }
    else
    if (fmq_msg_operation (self->message) == FMQ_MSG_FILE_DELETE) {
        zsys_debug ("delete %s/%s", self->inbox, filename);
        zfile_t *file = zfile_new (self->inbox, filename);
        zfile_remove (file);
        zfile_destroy (&file);

        //  Report file deletion back to caller
        //  Notify the caller of deletion
        zsock_send (self->msgpipe, "sss", "FILE DELETED", self->inbox,
            filename);
    }
}


//  ---------------------------------------------------------------------------
//  refill_credit_as_needed
//

static void
refill_credit_as_needed (client_t *self)
{
    zsys_debug ("refill credit as needed");
    size_t credit_to_send = 0;
    while (self->credit < CREDIT_MINIMUM) {
        credit_to_send += CREDIT_SLICE;
        self->credit += CREDIT_SLICE;
    }
    if (credit_to_send) {
        fmq_msg_set_credit (self->message, credit_to_send);
        engine_set_next_event (self, send_credit_event);
    }
}


//  ---------------------------------------------------------------------------
//  log_access_denied
//

static void
log_access_denied (client_t *self)
{
    zsys_warning ("##### access was denied #####");
}


//  ---------------------------------------------------------------------------
//  log_invalid_message
//

static void
log_invalid_message (client_t *self)
{
    zsys_error ("????? invalid message ?????");
    fmq_msg_print (self->message);
}


//  ---------------------------------------------------------------------------
//  log_protocol_error
//

static void
log_protocol_error (client_t *self)
{
    zsys_error ("***** protocol error *****");
    fmq_msg_print (self->message);
}


//  ---------------------------------------------------------------------------
//  sync_server_not_present
//

static void
sync_server_not_present (client_t *self)
{
    zsock_send (self->cmdpipe, "sis", "FAILURE", -1, "server is not reachable");
}


//  ---------------------------------------------------------------------------
//  async_server_not_present
//

static void
async_server_not_present (client_t *self)
{
    zsock_send (self->msgpipe, "ss", "DISCONNECT", "server is not reachable");
}


//  ---------------------------------------------------------------------------
//  Selftest

void
fmq_client_test (bool verbose)
{
    printf (" * fmq_client:");
    if (verbose)
        printf ("\n");

    //  @selftest
    //  Start a server to test against, and bind to endpoint
    zactor_t *server = zactor_new (fmq_server, "fmq_server");
    if (verbose)
        zstr_send (server, "VERBOSE");
    zstr_sendx (server, "BIND", "ipc://filemq", NULL);

    //  Create directories used for the test.
    zsys_debug ("attempting to create directory");
    int rc = zsys_dir_create ("./fmqserver");
    if (rc == 0)
        zsys_debug ("./fmqserver created");
    else
        zsys_error ("./fmqserver NOT created");
    assert (rc == 0);
    rc = zsys_dir_create ("./fmqclient");
    if (rc == 0)
        zsys_debug ("./fmqclient created");
    else
        zsys_error ("./fmqclient NOT created");
    assert (rc == 0);

    //  Tell the server to publish from directory just created
    zsys_debug ("attempting to publish");
    zstr_sendx (server, "PUBLISH", "./fmqserver", "/", NULL);
    zsys_debug ("publish sent, attempt to get response");
    char *response = zstr_recv (server);
    assert (streq (response, "SUCCESS"));
    zsys_debug ("fmq_client_test: received %s", response);
    zstr_free (&response);

    //  Create the client
    fmq_client_t *client = fmq_client_new ();
    assert (client);
	fmq_client_verbose = verbose;

    rc = fmq_client_connect (client, "ipc://filemq", 5000);
    assert (rc == 0);

    //  Set the clients storage location
    rc = fmq_client_set_inbox (client, "./fmqclient");
    assert (rc >= 0);

    //  Subscribe to the server's root
    rc = fmq_client_subscribe (client, "/");
    assert (rc >= 0);
    zsys_debug ("fmq_client_test: subscribed to server root");

    //  Get a reference to the msgpipe
    zsock_t *pipe = fmq_client_msgpipe (client);

    //  Create and populate file that will be shared
    zfile_t *sfile = zfile_new ("./fmqserver", "test_file.txt");
    assert (sfile);
    rc = zfile_output (sfile);
    assert (rc == 0);
    const char *data = "This is a test file for FileMQ.\n\tThat's all...\n";
    zchunk_t *chunk = zchunk_new ((const void *) data, strlen (data));
    assert (chunk);
    rc = zfile_write (sfile, chunk, 0);
    assert (rc == 0);
    zchunk_destroy (&chunk);
    zfile_close (sfile);
    zfile_restat (sfile);
    const char *sdigest = zfile_digest (sfile);
    assert (sdigest);
    zsys_info ("fmq_client_test: Server file digest %s", sdigest);

    //  Wait for notification of file update
    zmsg_t *pipemsg = zmsg_recv ( (void *) pipe);
    zmsg_print (pipemsg);
    zmsg_destroy (&pipemsg);

    //  See if the server and client files match
    zfile_t *cfile = zfile_new ("./fmqclient", "test_file.txt");
    assert (cfile);
    zfile_restat (cfile);
    const char *cdigest = zfile_digest (cfile);
    assert (cdigest);
    zsys_info ("fmq_client_test: Client file digest %s", cdigest);
    assert (streq (sdigest, cdigest));

    //  Delete the file the server is sharing
    zfile_remove (sfile);
    zfile_destroy (&sfile);

    //  Wait for notification of file update
    pipemsg = zmsg_recv ( (void *) pipe);
    zmsg_print (pipemsg);
    zmsg_destroy (&pipemsg);

    //  Kill the client
    fmq_client_destroy (&client);
    zsys_debug ("fmq_client_test: client destroyed");

    //  Kill the server
    zactor_destroy (&server);
    zsys_debug ("fmq_client_test: server destroyed");

    //  Delete the file the client has
    zfile_remove (cfile);
    zfile_destroy (&cfile);

    //  Delete the directory used by the server
    rc = zsys_dir_delete ("./fmqserver");
    if (rc == 0)
        zsys_debug ("./fmqserver has been deleted");
    else
        zsys_error ("./fmqserver was not deleted");

    //  Delete the directory used by the client
    rc = zsys_dir_delete ("./fmqclient");
    if (rc == 0)
        zsys_debug ("./fmqclient has been deleted");
    else
        zsys_error ("./fmqclient was not deleted");

    //  @end
    printf ("OK\n");
}
