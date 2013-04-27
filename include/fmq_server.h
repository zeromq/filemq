/*  =========================================================================
    fmq_server - a filemq server

    Generated header for fmq_server protocol server
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

#ifndef __FMQ_SERVER_H_INCLUDED__
#define __FMQ_SERVER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_server_t fmq_server_t;

//  @interface
//  Create a new fmq_server
fmq_server_t *
    fmq_server_new (void);

//  Destroy the fmq_server
void
    fmq_server_destroy (fmq_server_t **self_p);

//  Load server configuration data
void
    fmq_server_configure (fmq_server_t *self, const char *config_file);

//  Set one configuration path value
void
    fmq_server_setoption (fmq_server_t *self, const char *path, const char *value);

//  
int
    fmq_server_bind (fmq_server_t *self, const char *endpoint);

//  
void
    fmq_server_publish (fmq_server_t *self, const char *location, const char *alias);

//  
void
    fmq_server_set_anonymous (fmq_server_t *self, long enabled);

//  Self test of this class
int
    fmq_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
