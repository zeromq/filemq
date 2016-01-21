/*  =========================================================================
    fmq_client - FileMQ Client

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: fmq_client.xml, or
     * The code generation script that built this file: zproto_client_c
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of FileMQ, a C implemenation of the protocol:    
    https://github.com/danriegsecker/filemq2.                          
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef FMQ_CLIENT_H_INCLUDED
#define FMQ_CLIENT_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef FMQ_CLIENT_T_DEFINED
typedef struct _fmq_client_t fmq_client_t;
#define FMQ_CLIENT_T_DEFINED
#endif

//  @interface
//  Create a new fmq_client, return the reference if successful, or NULL
//  if construction failed due to lack of available memory.
FILEMQ_EXPORT fmq_client_t *
    fmq_client_new (void);

//  Destroy the fmq_client and free all memory used by the object.
FILEMQ_EXPORT void
    fmq_client_destroy (fmq_client_t **self_p);

//  Return actor, when caller wants to work with multiple actors and/or
//  input sockets asynchronously.
FILEMQ_EXPORT zactor_t *
    fmq_client_actor (fmq_client_t *self);

//  Return message pipe for asynchronous message I/O. In the high-volume case,
//  we send methods and get replies to the actor, in a synchronous manner, and
//  we send/recv high volume message data to a second pipe, the msgpipe. In
//  the low-volume case we can do everything over the actor pipe, if traffic
//  is never ambiguous.
FILEMQ_EXPORT zsock_t *
    fmq_client_msgpipe (fmq_client_t *self);

//  Return true if client is currently connected, else false. Note that the
//  client will automatically re-connect if the server dies and restarts after
//  a successful first connection.
FILEMQ_EXPORT bool
    fmq_client_connected (fmq_client_t *self);

//  Connect to server endpoint, with specified timeout in msecs (zero means wait    
//  forever). Connect succeeds if connection is successful.                         
//  Returns >= 0 if successful, -1 if interrupted.
FILEMQ_EXPORT uint8_t 
    fmq_client_connect (fmq_client_t *self, const char *endpoint, uint32_t timeout);

//  Subscribe to a directory on the server, directory specified by path.            
//  Returns >= 0 if successful, -1 if interrupted.
FILEMQ_EXPORT uint8_t 
    fmq_client_subscribe (fmq_client_t *self, const char *path);

//  Tell the api where to store files. This should be done before subscribing to    
//  anything.                                                                       
//  Returns >= 0 if successful, -1 if interrupted.
FILEMQ_EXPORT uint8_t 
    fmq_client_set_inbox (fmq_client_t *self, const char *path);

//  Return last received status
FILEMQ_EXPORT uint8_t 
    fmq_client_status (fmq_client_t *self);

//  Return last received reason
FILEMQ_EXPORT const char *
    fmq_client_reason (fmq_client_t *self);

//  Self test of this class
FILEMQ_EXPORT void
    fmq_client_test (bool verbose);

//  To enable verbose tracing (animation) of fmq_client instances, set
//  this to true. This lets you trace from and including construction.
FILEMQ_EXPORT extern volatile int
    fmq_client_verbose;
//  @end

#ifdef __cplusplus
}
#endif

#endif
