
#include "filemq_classes.h"

int main (int argc, char *argv [])
{
	zactor_t *server;
    if (argc < 2) {
        puts ("usage: filemq_server publish-from");
        return 0;
    }
    server = zactor_new (fmq_server, "filemq_server");

    //zstr_send (server, "VERBOSE");
    zstr_sendx (server, "PUBLISH", argv [1], "/", NULL);
    zstr_sendx (server, "BIND", "tcp://*:5670", NULL);

    while (!zsys_interrupted)
        zclock_sleep (1000);
    puts ("interrupted");

    zactor_destroy (&server);
    return 0;
}
