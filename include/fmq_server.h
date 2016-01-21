/*  =========================================================================
    fmq_server - FileMQ Server

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: fmq_server.xml, or
     * The code generation script that built this file: zproto_server_c
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of FileMQ, a C implemenation of the protocol:    
    https://github.com/danriegsecker/filemq2.                          
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef FMQ_SERVER_H_INCLUDED
#define FMQ_SERVER_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  To work with fmq_server, use the CZMQ zactor API:
//
//  Create new fmq_server instance, passing logging prefix:
//
//      zactor_t *fmq_server = zactor_new (fmq_server, "myname");
//
//  Destroy fmq_server instance
//
//      zactor_destroy (&fmq_server);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (fmq_server, "VERBOSE");
//
//  Bind fmq_server to specified endpoint. TCP endpoints may specify
//  the port number as "*" to aquire an ephemeral port:
//
//      zstr_sendx (fmq_server, "BIND", endpoint, NULL);
//
//  Return assigned port number, specifically when BIND was done using an
//  an ephemeral port:
//
//      zstr_sendx (fmq_server, "PORT", NULL);
//      char *command, *port_str;
//      zstr_recvx (fmq_server, &command, &port_str, NULL);
//      assert (streq (command, "PORT"));
//
//  Specify configuration file to load, overwriting any previous loaded
//  configuration file or options:
//
//      zstr_sendx (fmq_server, "LOAD", filename, NULL);
//
//  Set configuration path value:
//
//      zstr_sendx (fmq_server, "SET", path, value, NULL);
//
//  Save configuration data to config file on disk:
//
//      zstr_sendx (fmq_server, "SAVE", filename, NULL);
//
//  Send zmsg_t instance to fmq_server:
//
//      zactor_send (fmq_server, &msg);
//
//  Receive zmsg_t instance from fmq_server:
//
//      zmsg_t *msg = zactor_recv (fmq_server);
//
//  This is the fmq_server constructor as a zactor_fn:
//
FILEMQ_EXPORT void
    fmq_server (zsock_t *pipe, void *args);

//  Self test of this class
FILEMQ_EXPORT void
    fmq_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
