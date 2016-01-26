
#include "filemq_classes.h"

int main (int argc, char *argv [])
{
	fmq_client_t *client;
	int rc;
	zsock_t *msgpipe;
	zpoller_t *poller;

    if (argc < 2) {
        puts ("usage: filemq_client inbox-dir");
        return 0;
    }

    //  Create the client
    client = fmq_client_new ();
    assert (client);
    fmq_client_verbose = 1;

    rc = fmq_client_connect (client, "tcp://localhost:5670", 1000);
    assert (rc == 0);

    //  Set the clients storage location
    rc = fmq_client_set_inbox (client, argv [1]);
    assert (rc >= 0);

    //  Subscribe to the server's root
    rc = fmq_client_subscribe (client, "/");
    assert (rc >= 0);

    //  Get a reference to the msgpipe
    msgpipe = fmq_client_msgpipe (client);
    assert (msgpipe);

    //  Setup a poller
    poller = zpoller_new ( (void *) msgpipe, NULL);
    assert (poller);

    while (!zsys_interrupted) {
        void *sock = zpoller_wait (poller, 100);

        if (sock == msgpipe) {
            zmsg_t *msg = zmsg_recv ( (void *) msgpipe);
            zmsg_print (msg);
            zmsg_destroy (&msg);
        }
        else
        if (zpoller_terminated (poller)) {
            puts ("the poller terminated");
            break;
        }
    }
    puts ("interrupted");

    zpoller_destroy (&poller);
    fmq_client_destroy (&client);
    return 0;
}
