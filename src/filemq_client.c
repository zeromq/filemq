
#include "filemq_classes.h"

int main (int argc, char *argv [])
{
    if (argc < 2) {
        puts ("usage: filemq_client inbox-dir");
        return 0;
    }

    //  Create the client
    fmq_client_t *client = fmq_client_new ("tcp://localhost:5670", 1000);
    assert (client);
    fmq_client_verbose (client);

    //  Set the clients storage location
    int rc = fmq_client_set_inbox (client, argv [1]);
    assert (rc >= 0);

    //  Subscribe to the server's root
    rc = fmq_client_subscribe (client, "/");
    assert (rc >= 0);

    //  Get a reference to the msgpipe
    zsock_t *msgpipe = fmq_client_msgpipe (client);
    assert (msgpipe);

    //  Setup a poller
    zpoller_t *poller = zpoller_new ( (void *) msgpipe, NULL);
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
