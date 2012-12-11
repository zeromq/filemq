/*  =========================================================================
    fmq_selftest - run self tests

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

#include "czmq.h"
#include "../include/fmq.h"

int main (int argc, char *argv [])
{
    Bool verbose;
    if (argc == 2 && streq (argv [1], "-v")) {
        argc--;
        verbose = TRUE;
    }
    else
        verbose = FALSE;

    //  Do normal checks if run without arguments
    if (argc < 2) {
        printf ("Running self tests...\n");
        fmq_patch_test (verbose);
        fmq_chunk_test (verbose);
        fmq_file_test (verbose);
        fmq_dir_test (verbose);
        fmq_msg_test (verbose);
        fmq_sasl_test (verbose);
        fmq_hash_test (verbose);
        fmq_config_test (verbose);
        fmq_server_test (verbose);
        fmq_client_test (verbose);
        printf ("Tests passed OK\n");
        return 0;
    }

    //  Else run as FILEMQ server or client
    if (streq (argv [1], "-s")) {
        fmq_server_t *server = fmq_server_new ();
        fmq_server_configure (server, "server_test.cfg");
        fmq_server_publish (server, "./fmqroot/send", "/");
        fmq_server_publish (server, "./fmqroot/logs", "/logs");
        //  We do this last
        fmq_server_bind (server, "tcp://*:5670");
        while (!zctx_interrupted)
            zclock_sleep (1000);
        fmq_server_destroy (&server);
    }
    else
    if (streq (argv [1], "-c")) {
        fmq_client_t *client = fmq_client_new ();
        fmq_client_configure (client, "client_test.cfg");
        fmq_client_setoption (client, "client/inbox", "./fmqroot/recv");
        fmq_client_connect (client, "tcp://localhost:5670");
        fmq_client_set_resync (client, true);
        fmq_client_subscribe (client, "/photos");
        fmq_client_subscribe (client, "/logs");

        while (true) {
            //  Get message from fmq_client API
            zmsg_t *msg = fmq_client_recv (client);
            if (!msg)
                break;              //  Interrupted
            char *command = zmsg_popstr (msg);
            if (streq (command, "DELIVER")) {
                char *filename = zmsg_popstr (msg);
                char *fullname = zmsg_popstr (msg);
                printf ("I: received %s (%s)\n", filename, fullname);
                free (filename);
                free (fullname);
            }
            free (command);
            zmsg_destroy (&msg);
        }
        fmq_client_destroy (&client);
    }
    return 0;
}
