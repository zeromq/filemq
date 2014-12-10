/*  =========================================================================
    fmq_client - FILEMQ Client

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

#ifndef __FMQ_CLIENT_H_INCLUDED__
#define __FMQ_CLIENT_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef FMQ_CLIENT_T_DEFINED
typedef struct _fmq_client_t fmq_client_t;
#define FMQ_CLIENT_T_DEFINED
#endif

//  @interface
//  Create a new fmq_client
//  Connect to server endpoint, with specified timeout in msecs (zero means wait    
//  forever). Constructor succeeds if connection is successful.                     
fmq_client_t *
    fmq_client_new (const char *endpoint, int timeout);

//  Destroy the fmq_client
void
    fmq_client_destroy (fmq_client_t **self_p);

//  Enable verbose logging of client activity
void
    fmq_client_verbose (fmq_client_t *self);

//  Return actor, when caller wants to work with multiple actors and/or
//  input sockets asynchronously.
zactor_t *
    fmq_client_actor (fmq_client_t *self);

//  Return message pipe for asynchronous message I/O. In the high-volume case,
//  we send methods and get replies to the actor, in a synchronous manner, and
//  we send/recv high volume message data to a second pipe, the msgpipe. In
//  the low-volume case we can do everything over the actor pipe, if traffic
//  is never ambiguous.
zsock_t *
    fmq_client_msgpipe (fmq_client_t *self);

//  Subscribe to a directory on the server, directory specified by path.            
//  Returns >= 0 if successful, -1 if interrupted.
int
    fmq_client_subscribe (fmq_client_t *self, const char *path);

//  Tell the api where to store files. This should be done before subscribing to    
//  anything.                                                                       
//  Returns >= 0 if successful, -1 if interrupted.
int
    fmq_client_set_inbox (fmq_client_t *self, const char *path);

//  Return last received status
int 
    fmq_client_status (fmq_client_t *self);

//  Return last received reason
const char *
    fmq_client_reason (fmq_client_t *self);

//  Self test of this class
void
    fmq_client_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
