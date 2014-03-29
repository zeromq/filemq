/*  =========================================================================
    fmq_msg - work with FILEMQ messages
    
    Codec header for fmq_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

    * The XML model used for this code generation: fmq_msg.xml
    * The code generation script that built this file: zproto_codec_c
    ************************************************************************
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

#ifndef __FMQ_MSG_H_INCLUDED__
#define __FMQ_MSG_H_INCLUDED__

/*  These are the fmq_msg messages:

    OHAI - Client opens peering
        protocol            string      
        version             number 2    

    OHAI_OK - Server grants the client access

    ICANHAZ - Client subscribes to a path
        path                string      
        options             dictionary  
        cache               dictionary  

    ICANHAZ_OK - Server confirms the subscription

    NOM - Client sends credit to the server
        credit              number 8    
        sequence            number 8    

    CHEEZBURGER - The server sends a file chunk
        sequence            number 8    
        operation           number 1    
        filename            string      
        offset              number 8    
        eof                 number 1    
        headers             dictionary  
        chunk               chunk       

    HUGZ - Client or server sends a heartbeat

    HUGZ_OK - Client or server answers a heartbeat

    KTHXBAI - Client closes the peering

    SRSLY - Server refuses client due to access rights
        reason              string      

    RTFM - Server tells client it sent an invalid message
        reason              string      
*/

#define FMQ_MSG_VERSION                     1
#define FMQ_MSG_FILE_CREATE                 1
#define FMQ_MSG_FILE_DELETE                 2

#define FMQ_MSG_OHAI                        1
#define FMQ_MSG_OHAI_OK                     4
#define FMQ_MSG_ICANHAZ                     5
#define FMQ_MSG_ICANHAZ_OK                  6
#define FMQ_MSG_NOM                         7
#define FMQ_MSG_CHEEZBURGER                 8
#define FMQ_MSG_HUGZ                        9
#define FMQ_MSG_HUGZ_OK                     10
#define FMQ_MSG_KTHXBAI                     11
#define FMQ_MSG_SRSLY                       128
#define FMQ_MSG_RTFM                        129

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_msg_t fmq_msg_t;

//  @interface
//  Create a new fmq_msg
fmq_msg_t *
    fmq_msg_new (int id);

//  Destroy the fmq_msg
void
    fmq_msg_destroy (fmq_msg_t **self_p);

//  Parse a fmq_msg from zmsg_t. Returns a new object, or NULL if
//  the message could not be parsed, or was NULL. If the socket type is
//  ZMQ_ROUTER, then parses the first frame as a routing_id. Destroys msg
//  and nullifies the msg refernce.
fmq_msg_t *
    fmq_msg_decode (zmsg_t **msg_p, int socket_type);

//  Encode fmq_msg into zmsg and destroy it. Returns a newly created
//  object or NULL if error. Use when not in control of sending the message.
//  If the socket_type is ZMQ_ROUTER, then stores the routing_id as the
//  first frame of the resulting message.
zmsg_t *
    fmq_msg_encode (fmq_msg_t *self, int socket_type);

//  Receive and parse a fmq_msg from the socket. Returns new object, 
//  or NULL if error. Will block if there's no message waiting.
fmq_msg_t *
    fmq_msg_recv (void *input);

//  Receive and parse a fmq_msg from the socket. Returns new object, 
//  or NULL either if there was no input waiting, or the recv was interrupted.
fmq_msg_t *
    fmq_msg_recv_nowait (void *input);

//  Send the fmq_msg to the output, and destroy it
int
    fmq_msg_send (fmq_msg_t **self_p, void *output);

//  Send the fmq_msg to the output, and do not destroy it
int
    fmq_msg_send_again (fmq_msg_t *self, void *output);

//  Send the OHAI to the output in one step
int
    fmq_msg_send_ohai (void *output);
    
//  Send the OHAI_OK to the output in one step
int
    fmq_msg_send_ohai_ok (void *output);
    
//  Send the ICANHAZ to the output in one step
int
    fmq_msg_send_icanhaz (void *output,
        const char *path,
        zhash_t *options,
        zhash_t *cache);
    
//  Send the ICANHAZ_OK to the output in one step
int
    fmq_msg_send_icanhaz_ok (void *output);
    
//  Send the NOM to the output in one step
int
    fmq_msg_send_nom (void *output,
        uint64_t credit,
        uint64_t sequence);
    
//  Send the CHEEZBURGER to the output in one step
int
    fmq_msg_send_cheezburger (void *output,
        uint64_t sequence,
        byte operation,
        const char *filename,
        uint64_t offset,
        byte eof,
        zhash_t *headers,
        zchunk_t *chunk);
    
//  Send the HUGZ to the output in one step
int
    fmq_msg_send_hugz (void *output);
    
//  Send the HUGZ_OK to the output in one step
int
    fmq_msg_send_hugz_ok (void *output);
    
//  Send the KTHXBAI to the output in one step
int
    fmq_msg_send_kthxbai (void *output);
    
//  Send the SRSLY to the output in one step
int
    fmq_msg_send_srsly (void *output,
        const char *reason);
    
//  Send the RTFM to the output in one step
int
    fmq_msg_send_rtfm (void *output,
        const char *reason);
    
//  Duplicate the fmq_msg message
fmq_msg_t *
    fmq_msg_dup (fmq_msg_t *self);

//  Print contents of message to stdout
void
    fmq_msg_dump (fmq_msg_t *self);

//  Get/set the message routing id
zframe_t *
    fmq_msg_routing_id (fmq_msg_t *self);
void
    fmq_msg_set_routing_id (fmq_msg_t *self, zframe_t *routing_id);

//  Get the fmq_msg id and printable command
int
    fmq_msg_id (fmq_msg_t *self);
void
    fmq_msg_set_id (fmq_msg_t *self, int id);
const char *
    fmq_msg_command (fmq_msg_t *self);

//  Get/set the path field
const char *
    fmq_msg_path (fmq_msg_t *self);
void
    fmq_msg_set_path (fmq_msg_t *self, const char *format, ...);

//  Get/set the options field
zhash_t *
    fmq_msg_options (fmq_msg_t *self);
//  Get the options field and transfer ownership to caller
zhash_t *
    fmq_msg_get_options (fmq_msg_t *self);
//  Set the options field, transferring ownership from caller
void
    fmq_msg_set_options (fmq_msg_t *self, zhash_t **options_p);
    
//  Get/set a value in the options dictionary
const char *
    fmq_msg_options_string (fmq_msg_t *self,
        const char *key, const char *default_value);
uint64_t
    fmq_msg_options_number (fmq_msg_t *self,
        const char *key, uint64_t default_value);
void
    fmq_msg_options_insert (fmq_msg_t *self,
        const char *key, const char *format, ...);
size_t
    fmq_msg_options_size (fmq_msg_t *self);

//  Get/set the cache field
zhash_t *
    fmq_msg_cache (fmq_msg_t *self);
//  Get the cache field and transfer ownership to caller
zhash_t *
    fmq_msg_get_cache (fmq_msg_t *self);
//  Set the cache field, transferring ownership from caller
void
    fmq_msg_set_cache (fmq_msg_t *self, zhash_t **cache_p);
    
//  Get/set a value in the cache dictionary
const char *
    fmq_msg_cache_string (fmq_msg_t *self,
        const char *key, const char *default_value);
uint64_t
    fmq_msg_cache_number (fmq_msg_t *self,
        const char *key, uint64_t default_value);
void
    fmq_msg_cache_insert (fmq_msg_t *self,
        const char *key, const char *format, ...);
size_t
    fmq_msg_cache_size (fmq_msg_t *self);

//  Get/set the credit field
uint64_t
    fmq_msg_credit (fmq_msg_t *self);
void
    fmq_msg_set_credit (fmq_msg_t *self, uint64_t credit);

//  Get/set the sequence field
uint64_t
    fmq_msg_sequence (fmq_msg_t *self);
void
    fmq_msg_set_sequence (fmq_msg_t *self, uint64_t sequence);

//  Get/set the operation field
byte
    fmq_msg_operation (fmq_msg_t *self);
void
    fmq_msg_set_operation (fmq_msg_t *self, byte operation);

//  Get/set the filename field
const char *
    fmq_msg_filename (fmq_msg_t *self);
void
    fmq_msg_set_filename (fmq_msg_t *self, const char *format, ...);

//  Get/set the offset field
uint64_t
    fmq_msg_offset (fmq_msg_t *self);
void
    fmq_msg_set_offset (fmq_msg_t *self, uint64_t offset);

//  Get/set the eof field
byte
    fmq_msg_eof (fmq_msg_t *self);
void
    fmq_msg_set_eof (fmq_msg_t *self, byte eof);

//  Get/set the headers field
zhash_t *
    fmq_msg_headers (fmq_msg_t *self);
//  Get the headers field and transfer ownership to caller
zhash_t *
    fmq_msg_get_headers (fmq_msg_t *self);
//  Set the headers field, transferring ownership from caller
void
    fmq_msg_set_headers (fmq_msg_t *self, zhash_t **headers_p);
    
//  Get/set a value in the headers dictionary
const char *
    fmq_msg_headers_string (fmq_msg_t *self,
        const char *key, const char *default_value);
uint64_t
    fmq_msg_headers_number (fmq_msg_t *self,
        const char *key, uint64_t default_value);
void
    fmq_msg_headers_insert (fmq_msg_t *self,
        const char *key, const char *format, ...);
size_t
    fmq_msg_headers_size (fmq_msg_t *self);

//  Get a copy of the chunk field
zchunk_t *
    fmq_msg_chunk (fmq_msg_t *self);
//  Get the chunk field and transfer ownership to caller
zchunk_t *
    fmq_msg_get_chunk (fmq_msg_t *self);
//  Set the chunk field, transferring ownership from caller
void
    fmq_msg_set_chunk (fmq_msg_t *self, zchunk_t **chunk_p);

//  Get/set the reason field
const char *
    fmq_msg_reason (fmq_msg_t *self);
void
    fmq_msg_set_reason (fmq_msg_t *self, const char *format, ...);

//  Self test of this class
int
    fmq_msg_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
