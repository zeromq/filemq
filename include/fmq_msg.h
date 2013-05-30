/*  =========================================================================
    fmq_msg - work with filemq messages
    
    Generated codec header for fmq_msg
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

#ifndef __FMQ_MSG_H_INCLUDED__
#define __FMQ_MSG_H_INCLUDED__

/*  These are the fmq_msg messages
    OHAI - Client opens peering
        protocol      string
        version       number 1
    ORLY - Server challenges the client to authenticate itself
        mechanisms    strings
        challenge     frame
    YARLY - Client responds with authentication information
        mechanism     string
        response      frame
    OHAI_OK - Server grants the client access
    ICANHAZ - Client subscribes to a path
        path          string
        options       dictionary
        cache         dictionary
    ICANHAZ_OK - Server confirms the subscription
    NOM - Client sends credit to the server
        credit        number 8
        sequence      number 8
    CHEEZBURGER - The server sends a file chunk
        sequence      number 8
        operation     number 1
        filename      string
        offset        number 8
        eof           number 1
        headers       dictionary
        chunk         frame
    HUGZ - Client or server sends a heartbeat
    HUGZ_OK - Client or server answers a heartbeat
    KTHXBAI - Client closes the peering
    SRSLY - Server refuses client due to access rights
        reason        string
    RTFM - Server tells client it sent an invalid message
        reason        string
*/

#define FMQ_MSG_VERSION                     1
#define FMQ_MSG_FILE_CREATE                 1
#define FMQ_MSG_FILE_DELETE                 2

#define FMQ_MSG_OHAI                        1
#define FMQ_MSG_ORLY                        2
#define FMQ_MSG_YARLY                       3
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

//  Receive and parse a fmq_msg from the input
fmq_msg_t *
    fmq_msg_recv (void *input);

//  Send the fmq_msg to the output, and destroy it
int
    fmq_msg_send (fmq_msg_t **self_p, void *output);

//  Send the OHAI to the output in one step
int
    fmq_msg_send_ohai (void *output);
    
//  Send the ORLY to the output in one step
int
    fmq_msg_send_orly (void *output,
        zlist_t *mechanisms,
        zframe_t *challenge);
    
//  Send the YARLY to the output in one step
int
    fmq_msg_send_yarly (void *output,
        char *mechanism,
        zframe_t *response);
    
//  Send the OHAI_OK to the output in one step
int
    fmq_msg_send_ohai_ok (void *output);
    
//  Send the ICANHAZ to the output in one step
int
    fmq_msg_send_icanhaz (void *output,
        char *path,
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
        char *filename,
        uint64_t offset,
        byte eof,
        zhash_t *headers,
        zframe_t *chunk);
    
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
        char *reason);
    
//  Send the RTFM to the output in one step
int
    fmq_msg_send_rtfm (void *output,
        char *reason);
    
//  Duplicate the fmq_msg message
fmq_msg_t *
    fmq_msg_dup (fmq_msg_t *self);

//  Print contents of message to stdout
void
    fmq_msg_dump (fmq_msg_t *self);

//  Get/set the message address
zframe_t *
    fmq_msg_address (fmq_msg_t *self);
void
    fmq_msg_set_address (fmq_msg_t *self, zframe_t *address);

//  Get the fmq_msg id and printable command
int
    fmq_msg_id (fmq_msg_t *self);
void
    fmq_msg_set_id (fmq_msg_t *self, int id);
char *
    fmq_msg_command (fmq_msg_t *self);

//  Get/set the mechanisms field
zlist_t *
    fmq_msg_mechanisms (fmq_msg_t *self);
void
    fmq_msg_set_mechanisms (fmq_msg_t *self, zlist_t *mechanisms);

//  Iterate through the mechanisms field, and append a mechanisms value
char *
    fmq_msg_mechanisms_first (fmq_msg_t *self);
char *
    fmq_msg_mechanisms_next (fmq_msg_t *self);
void
    fmq_msg_mechanisms_append (fmq_msg_t *self, char *format, ...);
size_t
    fmq_msg_mechanisms_size (fmq_msg_t *self);

//  Get/set the challenge field
zframe_t *
    fmq_msg_challenge (fmq_msg_t *self);
void
    fmq_msg_set_challenge (fmq_msg_t *self, zframe_t *frame);

//  Get/set the mechanism field
char *
    fmq_msg_mechanism (fmq_msg_t *self);
void
    fmq_msg_set_mechanism (fmq_msg_t *self, char *format, ...);

//  Get/set the response field
zframe_t *
    fmq_msg_response (fmq_msg_t *self);
void
    fmq_msg_set_response (fmq_msg_t *self, zframe_t *frame);

//  Get/set the path field
char *
    fmq_msg_path (fmq_msg_t *self);
void
    fmq_msg_set_path (fmq_msg_t *self, char *format, ...);

//  Get/set the options field
zhash_t *
    fmq_msg_options (fmq_msg_t *self);
void
    fmq_msg_set_options (fmq_msg_t *self, zhash_t *options);
    
//  Get/set a value in the options dictionary
char *
    fmq_msg_options_string (fmq_msg_t *self, char *key, char *default_value);
uint64_t
    fmq_msg_options_number (fmq_msg_t *self, char *key, uint64_t default_value);
void
    fmq_msg_options_insert (fmq_msg_t *self, char *key, char *format, ...);
size_t
    fmq_msg_options_size (fmq_msg_t *self);

//  Get/set the cache field
zhash_t *
    fmq_msg_cache (fmq_msg_t *self);
void
    fmq_msg_set_cache (fmq_msg_t *self, zhash_t *cache);
    
//  Get/set a value in the cache dictionary
char *
    fmq_msg_cache_string (fmq_msg_t *self, char *key, char *default_value);
uint64_t
    fmq_msg_cache_number (fmq_msg_t *self, char *key, uint64_t default_value);
void
    fmq_msg_cache_insert (fmq_msg_t *self, char *key, char *format, ...);
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
char *
    fmq_msg_filename (fmq_msg_t *self);
void
    fmq_msg_set_filename (fmq_msg_t *self, char *format, ...);

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
void
    fmq_msg_set_headers (fmq_msg_t *self, zhash_t *headers);
    
//  Get/set a value in the headers dictionary
char *
    fmq_msg_headers_string (fmq_msg_t *self, char *key, char *default_value);
uint64_t
    fmq_msg_headers_number (fmq_msg_t *self, char *key, uint64_t default_value);
void
    fmq_msg_headers_insert (fmq_msg_t *self, char *key, char *format, ...);
size_t
    fmq_msg_headers_size (fmq_msg_t *self);

//  Get/set the chunk field
zframe_t *
    fmq_msg_chunk (fmq_msg_t *self);
void
    fmq_msg_set_chunk (fmq_msg_t *self, zframe_t *frame);

//  Get/set the reason field
char *
    fmq_msg_reason (fmq_msg_t *self);
void
    fmq_msg_set_reason (fmq_msg_t *self, char *format, ...);

//  Self test of this class
int
    fmq_msg_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
