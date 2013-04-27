/*  =========================================================================
    fmq_client - a FILEMQ client

    Generated header for fmq_client protocol client
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

#ifndef __FMQ_CLIENT_H_INCLUDED__
#define __FMQ_CLIENT_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_client_t fmq_client_t;

//  @interface
//  Create a new fmq_client
fmq_client_t *
    fmq_client_new (void);

//  Destroy the fmq_client
void
    fmq_client_destroy (fmq_client_t **self_p);

//  Load client configuration data
void
    fmq_client_configure (fmq_client_t *self, const char *config_file);

//  Set one configuration key value
void
    fmq_client_setoption (fmq_client_t *self, const char *path, const char *value);

//  Create outgoing connection to server
void
    fmq_client_connect (fmq_client_t *self, const char *endpoint);

//  Wait for message from API
zmsg_t *
    fmq_client_recv (fmq_client_t *self);

//  Return API pipe handle for polling
void *
    fmq_client_handle (fmq_client_t *self);

//  
void
    fmq_client_subscribe (fmq_client_t *self, const char *path);

//  
void
    fmq_client_set_inbox (fmq_client_t *self, const char *path);

//  
void
    fmq_client_set_resync (fmq_client_t *self, long enabled);

//  Self test of this class
int
    fmq_client_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
