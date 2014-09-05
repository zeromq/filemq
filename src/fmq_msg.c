/*  =========================================================================
    fmq_msg - work with FILEMQ messages

    Codec class for fmq_msg.

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

/*
@header
    fmq_msg - work with FILEMQ messages
@discuss
@end
*/

#include <czmq.h>
#include "../include/fmq_msg.h"

//  Structure of our class

struct _fmq_msg_t {
    zframe_t *routing_id;               //  Routing_id from ROUTER, if any
    int id;                             //  fmq_msg message ID
    byte *needle;                       //  Read/write pointer for serialization
    byte *ceiling;                      //  Valid upper limit for read pointer
    char *protocol;                     //  Constant "FILEMQ"
    uint16_t version;                   //  Protocol version 2
    char *path;                         //  Full path or path prefix
    zhash_t *options;                   //  Subscription options
    size_t options_bytes;               //  Size of dictionary content
    zhash_t *cache;                     //  File SHA-1 signatures
    size_t cache_bytes;                 //  Size of dictionary content
    uint64_t credit;                    //  Credit, in bytes
    uint64_t sequence;                  //  Chunk sequence, 0 and up
    byte operation;                     //  Create=%d1 delete=%d2
    char *filename;                     //  Relative name of file
    uint64_t offset;                    //  File offset in bytes
    byte eof;                           //  Last chunk in file?
    zhash_t *headers;                   //  File properties
    size_t headers_bytes;               //  Size of dictionary content
    zchunk_t *chunk;                    //  Data chunk
    char *reason;                       //  Printable explanation
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Put a block of octets to the frame
#define PUT_OCTETS(host,size) { \
    memcpy (self->needle, (host), size); \
    self->needle += size; \
}

//  Get a block of octets from the frame
#define GET_OCTETS(host,size) { \
    if (self->needle + size > self->ceiling) \
        goto malformed; \
    memcpy ((host), self->needle, size); \
    self->needle += size; \
}

//  Put a 1-byte number to the frame
#define PUT_NUMBER1(host) { \
    *(byte *) self->needle = (host); \
    self->needle++; \
}

//  Put a 2-byte number to the frame
#define PUT_NUMBER2(host) { \
    self->needle [0] = (byte) (((host) >> 8)  & 255); \
    self->needle [1] = (byte) (((host))       & 255); \
    self->needle += 2; \
}

//  Put a 4-byte number to the frame
#define PUT_NUMBER4(host) { \
    self->needle [0] = (byte) (((host) >> 24) & 255); \
    self->needle [1] = (byte) (((host) >> 16) & 255); \
    self->needle [2] = (byte) (((host) >> 8)  & 255); \
    self->needle [3] = (byte) (((host))       & 255); \
    self->needle += 4; \
}

//  Put a 8-byte number to the frame
#define PUT_NUMBER8(host) { \
    self->needle [0] = (byte) (((host) >> 56) & 255); \
    self->needle [1] = (byte) (((host) >> 48) & 255); \
    self->needle [2] = (byte) (((host) >> 40) & 255); \
    self->needle [3] = (byte) (((host) >> 32) & 255); \
    self->needle [4] = (byte) (((host) >> 24) & 255); \
    self->needle [5] = (byte) (((host) >> 16) & 255); \
    self->needle [6] = (byte) (((host) >> 8)  & 255); \
    self->needle [7] = (byte) (((host))       & 255); \
    self->needle += 8; \
}

//  Get a 1-byte number from the frame
#define GET_NUMBER1(host) { \
    if (self->needle + 1 > self->ceiling) \
        goto malformed; \
    (host) = *(byte *) self->needle; \
    self->needle++; \
}

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) \
        goto malformed; \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
}

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) \
        goto malformed; \
    (host) = ((uint32_t) (self->needle [0]) << 24) \
           + ((uint32_t) (self->needle [1]) << 16) \
           + ((uint32_t) (self->needle [2]) << 8) \
           +  (uint32_t) (self->needle [3]); \
    self->needle += 4; \
}

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (self->needle + 8 > self->ceiling) \
        goto malformed; \
    (host) = ((uint64_t) (self->needle [0]) << 56) \
           + ((uint64_t) (self->needle [1]) << 48) \
           + ((uint64_t) (self->needle [2]) << 40) \
           + ((uint64_t) (self->needle [3]) << 32) \
           + ((uint64_t) (self->needle [4]) << 24) \
           + ((uint64_t) (self->needle [5]) << 16) \
           + ((uint64_t) (self->needle [6]) << 8) \
           +  (uint64_t) (self->needle [7]); \
    self->needle += 8; \
}

//  Put a string to the frame
#define PUT_STRING(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a string from the frame
#define GET_STRING(host) { \
    size_t string_size; \
    GET_NUMBER1 (string_size); \
    if (self->needle + string_size > (self->ceiling)) \
        goto malformed; \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}

//  Put a long string to the frame
#define PUT_LONGSTR(host) { \
    size_t string_size = strlen (host); \
    PUT_NUMBER4 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
}

//  Get a long string from the frame
#define GET_LONGSTR(host) { \
    size_t string_size; \
    GET_NUMBER4 (string_size); \
    if (self->needle + string_size > (self->ceiling)) \
        goto malformed; \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}


//  --------------------------------------------------------------------------
//  Create a new fmq_msg

fmq_msg_t *
fmq_msg_new (int id)
{
    fmq_msg_t *self = (fmq_msg_t *) zmalloc (sizeof (fmq_msg_t));
    self->id = id;
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fmq_msg

void
fmq_msg_destroy (fmq_msg_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_msg_t *self = *self_p;

        //  Free class properties
        zframe_destroy (&self->routing_id);
        free (self->protocol);
        free (self->path);
        zhash_destroy (&self->options);
        zhash_destroy (&self->cache);
        free (self->filename);
        zhash_destroy (&self->headers);
        zchunk_destroy (&self->chunk);
        free (self->reason);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Parse a fmq_msg from zmsg_t. Returns a new object, or NULL if
//  the message could not be parsed, or was NULL. Destroys msg and 
//  nullifies the msg reference.

fmq_msg_t *
fmq_msg_decode (zmsg_t **msg_p)
{
    assert (msg_p);
    zmsg_t *msg = *msg_p;
    if (msg == NULL)
        return NULL;
        
    fmq_msg_t *self = fmq_msg_new (0);
    //  Read and parse command in frame
    zframe_t *frame = zmsg_pop (msg);
    if (!frame) 
        goto empty;             //  Malformed or empty

    //  Get and check protocol signature
    self->needle = zframe_data (frame);
    self->ceiling = self->needle + zframe_size (frame);
    uint16_t signature;
    GET_NUMBER2 (signature);
    if (signature != (0xAAA0 | 3))
        goto empty;             //  Invalid signature

    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            GET_STRING (self->protocol);
            if (strneq (self->protocol, "FILEMQ"))
                goto malformed;
            GET_NUMBER2 (self->version);
            if (self->version != FMQ_MSG_VERSION)
                goto malformed;
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            GET_STRING (self->path);
            {
                size_t hash_size;
                GET_NUMBER4 (hash_size);
                self->options = zhash_new ();
                zhash_autofree (self->options);
                while (hash_size--) {
                    char *key, *value;
                    GET_STRING (key);
                    GET_LONGSTR (value);
                    zhash_insert (self->options, key, value);
                    free (key);
                    free (value);
                }
            }
            {
                size_t hash_size;
                GET_NUMBER4 (hash_size);
                self->cache = zhash_new ();
                zhash_autofree (self->cache);
                while (hash_size--) {
                    char *key, *value;
                    GET_STRING (key);
                    GET_LONGSTR (value);
                    zhash_insert (self->cache, key, value);
                    free (key);
                    free (value);
                }
            }
            break;

        case FMQ_MSG_ICANHAZ_OK:
            break;

        case FMQ_MSG_NOM:
            GET_NUMBER8 (self->credit);
            GET_NUMBER8 (self->sequence);
            break;

        case FMQ_MSG_CHEEZBURGER:
            GET_NUMBER8 (self->sequence);
            GET_NUMBER1 (self->operation);
            GET_STRING (self->filename);
            GET_NUMBER8 (self->offset);
            GET_NUMBER1 (self->eof);
            {
                size_t hash_size;
                GET_NUMBER4 (hash_size);
                self->headers = zhash_new ();
                zhash_autofree (self->headers);
                while (hash_size--) {
                    char *key, *value;
                    GET_STRING (key);
                    GET_LONGSTR (value);
                    zhash_insert (self->headers, key, value);
                    free (key);
                    free (value);
                }
            }
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling))
                    goto malformed;
                self->chunk = zchunk_new (self->needle, chunk_size);
                self->needle += chunk_size;
            }
            break;

        case FMQ_MSG_HUGZ:
            break;

        case FMQ_MSG_HUGZ_OK:
            break;

        case FMQ_MSG_KTHXBAI:
            break;

        case FMQ_MSG_SRSLY:
            GET_STRING (self->reason);
            break;

        case FMQ_MSG_RTFM:
            GET_STRING (self->reason);
            break;

        default:
            goto malformed;
    }
    //  Successful return
    zframe_destroy (&frame);
    zmsg_destroy (msg_p);
    return self;

    //  Error returns
    malformed:
        zsys_error ("malformed message '%d'\n", self->id);
    empty:
        zframe_destroy (&frame);
        zmsg_destroy (msg_p);
        fmq_msg_destroy (&self);
        return (NULL);
}


//  --------------------------------------------------------------------------
//  Encode fmq_msg into zmsg and destroy it. Returns a newly created
//  object or NULL if error. Use when not in control of sending the message.

zmsg_t *
fmq_msg_encode (fmq_msg_t **self_p)
{
    assert (self_p);
    assert (*self_p);
    
    fmq_msg_t *self = *self_p;
    zmsg_t *msg = zmsg_new ();

    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case FMQ_MSG_OHAI:
            //  protocol is a string with 1-byte length
            frame_size += 1 + strlen ("FILEMQ");
            //  version is a 2-byte integer
            frame_size += 2;
            break;
            
        case FMQ_MSG_OHAI_OK:
            break;
            
        case FMQ_MSG_ICANHAZ:
            //  path is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->path)
                frame_size += strlen (self->path);
            //  options is an array of key=value strings
            frame_size += 4;    //  Size is 4 octets
            if (self->options) {
                self->options_bytes = 0;
                //  Add up size of dictionary contents
                char *item = (char *) zhash_first (self->options);
                while (item) {
                    self->options_bytes += 1 + strlen (zhash_cursor (self->options));
                    self->options_bytes += 4 + strlen (item);
                    item = (char *) zhash_next (self->options);
                }
            }
            frame_size += self->options_bytes;
            //  cache is an array of key=value strings
            frame_size += 4;    //  Size is 4 octets
            if (self->cache) {
                self->cache_bytes = 0;
                //  Add up size of dictionary contents
                char *item = (char *) zhash_first (self->cache);
                while (item) {
                    self->cache_bytes += 1 + strlen (zhash_cursor (self->cache));
                    self->cache_bytes += 4 + strlen (item);
                    item = (char *) zhash_next (self->cache);
                }
            }
            frame_size += self->cache_bytes;
            break;
            
        case FMQ_MSG_ICANHAZ_OK:
            break;
            
        case FMQ_MSG_NOM:
            //  credit is a 8-byte integer
            frame_size += 8;
            //  sequence is a 8-byte integer
            frame_size += 8;
            break;
            
        case FMQ_MSG_CHEEZBURGER:
            //  sequence is a 8-byte integer
            frame_size += 8;
            //  operation is a 1-byte integer
            frame_size += 1;
            //  filename is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->filename)
                frame_size += strlen (self->filename);
            //  offset is a 8-byte integer
            frame_size += 8;
            //  eof is a 1-byte integer
            frame_size += 1;
            //  headers is an array of key=value strings
            frame_size += 4;    //  Size is 4 octets
            if (self->headers) {
                self->headers_bytes = 0;
                //  Add up size of dictionary contents
                char *item = (char *) zhash_first (self->headers);
                while (item) {
                    self->headers_bytes += 1 + strlen (zhash_cursor (self->headers));
                    self->headers_bytes += 4 + strlen (item);
                    item = (char *) zhash_next (self->headers);
                }
            }
            frame_size += self->headers_bytes;
            //  chunk is a chunk with 4-byte length
            frame_size += 4;
            if (self->chunk)
                frame_size += zchunk_size (self->chunk);
            break;
            
        case FMQ_MSG_HUGZ:
            break;
            
        case FMQ_MSG_HUGZ_OK:
            break;
            
        case FMQ_MSG_KTHXBAI:
            break;
            
        case FMQ_MSG_SRSLY:
            //  reason is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->reason)
                frame_size += strlen (self->reason);
            break;
            
        case FMQ_MSG_RTFM:
            //  reason is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->reason)
                frame_size += strlen (self->reason);
            break;
            
        default:
            zsys_error ("bad message type '%d', not sent\n", self->id);
            //  No recovery, this is a fatal application error
            assert (false);
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (NULL, frame_size);
    self->needle = zframe_data (frame);
    PUT_NUMBER2 (0xAAA0 | 3);
    PUT_NUMBER1 (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            PUT_STRING ("FILEMQ");
            PUT_NUMBER2 (FMQ_MSG_VERSION);
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            if (self->path) {
                PUT_STRING (self->path);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->options) {
                PUT_NUMBER4 (zhash_size (self->options));
                char *item = (char *) zhash_first (self->options);
                while (item) {
                    PUT_STRING (zhash_cursor (self->options));
                    PUT_LONGSTR (item);
                    item = (char *) zhash_next (self->options);
                }
            }
            else
                PUT_NUMBER4 (0);    //  Empty dictionary
            if (self->cache) {
                PUT_NUMBER4 (zhash_size (self->cache));
                char *item = (char *) zhash_first (self->cache);
                while (item) {
                    PUT_STRING (zhash_cursor (self->cache));
                    PUT_LONGSTR (item);
                    item = (char *) zhash_next (self->cache);
                }
            }
            else
                PUT_NUMBER4 (0);    //  Empty dictionary
            break;

        case FMQ_MSG_ICANHAZ_OK:
            break;

        case FMQ_MSG_NOM:
            PUT_NUMBER8 (self->credit);
            PUT_NUMBER8 (self->sequence);
            break;

        case FMQ_MSG_CHEEZBURGER:
            PUT_NUMBER8 (self->sequence);
            PUT_NUMBER1 (self->operation);
            if (self->filename) {
                PUT_STRING (self->filename);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER8 (self->offset);
            PUT_NUMBER1 (self->eof);
            if (self->headers) {
                PUT_NUMBER4 (zhash_size (self->headers));
                char *item = (char *) zhash_first (self->headers);
                while (item) {
                    PUT_STRING (zhash_cursor (self->headers));
                    PUT_LONGSTR (item);
                    item = (char *) zhash_next (self->headers);
                }
            }
            else
                PUT_NUMBER4 (0);    //  Empty dictionary
            if (self->chunk) {
                PUT_NUMBER4 (zchunk_size (self->chunk));
                memcpy (self->needle,
                        zchunk_data (self->chunk),
                        zchunk_size (self->chunk));
                self->needle += zchunk_size (self->chunk);
            }
            else
                PUT_NUMBER4 (0);    //  Empty chunk
            break;

        case FMQ_MSG_HUGZ:
            break;

        case FMQ_MSG_HUGZ_OK:
            break;

        case FMQ_MSG_KTHXBAI:
            break;

        case FMQ_MSG_SRSLY:
            if (self->reason) {
                PUT_STRING (self->reason);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;

        case FMQ_MSG_RTFM:
            if (self->reason) {
                PUT_STRING (self->reason);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;

    }
    //  Now send the data frame
    if (zmsg_append (msg, &frame)) {
        zmsg_destroy (&msg);
        fmq_msg_destroy (self_p);
        return NULL;
    }
    //  Destroy fmq_msg object
    fmq_msg_destroy (self_p);
    return msg;
}


//  --------------------------------------------------------------------------
//  Receive and parse a fmq_msg from the socket. Returns new object or
//  NULL if error. Will block if there's no message waiting.

fmq_msg_t *
fmq_msg_recv (void *input)
{
    assert (input);
    zmsg_t *msg = zmsg_recv (input);
    //  If message came from a router socket, first frame is routing_id
    zframe_t *routing_id = NULL;
    if (zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER) {
        routing_id = zmsg_pop (msg);
        //  If message was not valid, forget about it
        if (!routing_id || !zmsg_next (msg))
            return NULL;        //  Malformed or empty
    }
    fmq_msg_t *fmq_msg = fmq_msg_decode (&msg);
    if (fmq_msg && zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER)
        fmq_msg->routing_id = routing_id;

    return fmq_msg;
}


//  --------------------------------------------------------------------------
//  Receive and parse a fmq_msg from the socket. Returns new object,
//  or NULL either if there was no input waiting, or the recv was interrupted.

fmq_msg_t *
fmq_msg_recv_nowait (void *input)
{
    assert (input);
    zmsg_t *msg = zmsg_recv_nowait (input);
    //  If message came from a router socket, first frame is routing_id
    zframe_t *routing_id = NULL;
    if (zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER) {
        routing_id = zmsg_pop (msg);
        //  If message was not valid, forget about it
        if (!routing_id || !zmsg_next (msg))
            return NULL;        //  Malformed or empty
    }
    fmq_msg_t *fmq_msg = fmq_msg_decode (&msg);
    if (fmq_msg && zsocket_type (zsock_resolve (input)) == ZMQ_ROUTER)
        fmq_msg->routing_id = routing_id;

    return fmq_msg;
}


//  --------------------------------------------------------------------------
//  Send the fmq_msg to the socket, and destroy it
//  Returns 0 if OK, else -1

int
fmq_msg_send (fmq_msg_t **self_p, void *output)
{
    assert (self_p);
    assert (*self_p);
    assert (output);

    //  Save routing_id if any, as encode will destroy it
    fmq_msg_t *self = *self_p;
    zframe_t *routing_id = self->routing_id;
    self->routing_id = NULL;

    //  Encode fmq_msg message to a single zmsg
    zmsg_t *msg = fmq_msg_encode (&self);
    
    //  If we're sending to a ROUTER, send the routing_id first
    if (zsocket_type (zsock_resolve (output)) == ZMQ_ROUTER) {
        assert (routing_id);
        zmsg_prepend (msg, &routing_id);
    }
    else
        zframe_destroy (&routing_id);
        
    if (msg && zmsg_send (&msg, output) == 0)
        return 0;
    else
        return -1;              //  Failed to encode, or send
}


//  --------------------------------------------------------------------------
//  Send the fmq_msg to the output, and do not destroy it

int
fmq_msg_send_again (fmq_msg_t *self, void *output)
{
    assert (self);
    assert (output);
    self = fmq_msg_dup (self);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Encode OHAI message

zmsg_t * 
fmq_msg_encode_ohai (
)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_OHAI);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode OHAI_OK message

zmsg_t * 
fmq_msg_encode_ohai_ok (
)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_OHAI_OK);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode ICANHAZ message

zmsg_t * 
fmq_msg_encode_icanhaz (
    const char *path,
    zhash_t *options,
    zhash_t *cache)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_set_path (self, path);
    zhash_t *options_copy = zhash_dup (options);
    fmq_msg_set_options (self, &options_copy);
    zhash_t *cache_copy = zhash_dup (cache);
    fmq_msg_set_cache (self, &cache_copy);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode ICANHAZ_OK message

zmsg_t * 
fmq_msg_encode_icanhaz_ok (
)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_ICANHAZ_OK);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode NOM message

zmsg_t * 
fmq_msg_encode_nom (
    uint64_t credit,
    uint64_t sequence)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_set_credit (self, credit);
    fmq_msg_set_sequence (self, sequence);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode CHEEZBURGER message

zmsg_t * 
fmq_msg_encode_cheezburger (
    uint64_t sequence,
    byte operation,
    const char *filename,
    uint64_t offset,
    byte eof,
    zhash_t *headers,
    zchunk_t *chunk)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_CHEEZBURGER);
    fmq_msg_set_sequence (self, sequence);
    fmq_msg_set_operation (self, operation);
    fmq_msg_set_filename (self, filename);
    fmq_msg_set_offset (self, offset);
    fmq_msg_set_eof (self, eof);
    zhash_t *headers_copy = zhash_dup (headers);
    fmq_msg_set_headers (self, &headers_copy);
    zchunk_t *chunk_copy = zchunk_dup (chunk);
    fmq_msg_set_chunk (self, &chunk_copy);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode HUGZ message

zmsg_t * 
fmq_msg_encode_hugz (
)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_HUGZ);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode HUGZ_OK message

zmsg_t * 
fmq_msg_encode_hugz_ok (
)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_HUGZ_OK);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode KTHXBAI message

zmsg_t * 
fmq_msg_encode_kthxbai (
)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_KTHXBAI);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode SRSLY message

zmsg_t * 
fmq_msg_encode_srsly (
    const char *reason)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_SRSLY);
    fmq_msg_set_reason (self, reason);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Encode RTFM message

zmsg_t * 
fmq_msg_encode_rtfm (
    const char *reason)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_RTFM);
    fmq_msg_set_reason (self, reason);
    return fmq_msg_encode (&self);
}


//  --------------------------------------------------------------------------
//  Send the OHAI to the socket in one step

int
fmq_msg_send_ohai (
    void *output)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_OHAI);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the OHAI_OK to the socket in one step

int
fmq_msg_send_ohai_ok (
    void *output)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_OHAI_OK);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the ICANHAZ to the socket in one step

int
fmq_msg_send_icanhaz (
    void *output,
    const char *path,
    zhash_t *options,
    zhash_t *cache)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_set_path (self, path);
    zhash_t *options_copy = zhash_dup (options);
    fmq_msg_set_options (self, &options_copy);
    zhash_t *cache_copy = zhash_dup (cache);
    fmq_msg_set_cache (self, &cache_copy);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the ICANHAZ_OK to the socket in one step

int
fmq_msg_send_icanhaz_ok (
    void *output)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_ICANHAZ_OK);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the NOM to the socket in one step

int
fmq_msg_send_nom (
    void *output,
    uint64_t credit,
    uint64_t sequence)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_set_credit (self, credit);
    fmq_msg_set_sequence (self, sequence);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the CHEEZBURGER to the socket in one step

int
fmq_msg_send_cheezburger (
    void *output,
    uint64_t sequence,
    byte operation,
    const char *filename,
    uint64_t offset,
    byte eof,
    zhash_t *headers,
    zchunk_t *chunk)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_CHEEZBURGER);
    fmq_msg_set_sequence (self, sequence);
    fmq_msg_set_operation (self, operation);
    fmq_msg_set_filename (self, filename);
    fmq_msg_set_offset (self, offset);
    fmq_msg_set_eof (self, eof);
    zhash_t *headers_copy = zhash_dup (headers);
    fmq_msg_set_headers (self, &headers_copy);
    zchunk_t *chunk_copy = zchunk_dup (chunk);
    fmq_msg_set_chunk (self, &chunk_copy);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the HUGZ to the socket in one step

int
fmq_msg_send_hugz (
    void *output)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_HUGZ);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the HUGZ_OK to the socket in one step

int
fmq_msg_send_hugz_ok (
    void *output)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_HUGZ_OK);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the KTHXBAI to the socket in one step

int
fmq_msg_send_kthxbai (
    void *output)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_KTHXBAI);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the SRSLY to the socket in one step

int
fmq_msg_send_srsly (
    void *output,
    const char *reason)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_SRSLY);
    fmq_msg_set_reason (self, reason);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the RTFM to the socket in one step

int
fmq_msg_send_rtfm (
    void *output,
    const char *reason)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_RTFM);
    fmq_msg_set_reason (self, reason);
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Duplicate the fmq_msg message

fmq_msg_t *
fmq_msg_dup (fmq_msg_t *self)
{
    if (!self)
        return NULL;
        
    fmq_msg_t *copy = fmq_msg_new (self->id);
    if (self->routing_id)
        copy->routing_id = zframe_dup (self->routing_id);
    switch (self->id) {
        case FMQ_MSG_OHAI:
            copy->protocol = self->protocol? strdup (self->protocol): NULL;
            copy->version = self->version;
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            copy->path = self->path? strdup (self->path): NULL;
            copy->options = self->options? zhash_dup (self->options): NULL;
            copy->cache = self->cache? zhash_dup (self->cache): NULL;
            break;

        case FMQ_MSG_ICANHAZ_OK:
            break;

        case FMQ_MSG_NOM:
            copy->credit = self->credit;
            copy->sequence = self->sequence;
            break;

        case FMQ_MSG_CHEEZBURGER:
            copy->sequence = self->sequence;
            copy->operation = self->operation;
            copy->filename = self->filename? strdup (self->filename): NULL;
            copy->offset = self->offset;
            copy->eof = self->eof;
            copy->headers = self->headers? zhash_dup (self->headers): NULL;
            copy->chunk = self->chunk? zchunk_dup (self->chunk): NULL;
            break;

        case FMQ_MSG_HUGZ:
            break;

        case FMQ_MSG_HUGZ_OK:
            break;

        case FMQ_MSG_KTHXBAI:
            break;

        case FMQ_MSG_SRSLY:
            copy->reason = self->reason? strdup (self->reason): NULL;
            break;

        case FMQ_MSG_RTFM:
            copy->reason = self->reason? strdup (self->reason): NULL;
            break;

    }
    return copy;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
fmq_msg_print (fmq_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case FMQ_MSG_OHAI:
            zsys_debug ("FMQ_MSG_OHAI:");
            zsys_debug ("    protocol=filemq");
            zsys_debug ("    version=fmq_msg_version");
            break;
            
        case FMQ_MSG_OHAI_OK:
            zsys_debug ("FMQ_MSG_OHAI_OK:");
            break;
            
        case FMQ_MSG_ICANHAZ:
            zsys_debug ("FMQ_MSG_ICANHAZ:");
            if (self->path)
                zsys_debug ("    path='%s'", self->path);
            else
                zsys_debug ("    path=");
            zsys_debug ("    options=");
            if (self->options) {
                char *item = (char *) zhash_first (self->options);
                while (item) {
                    zsys_debug ("        %s=%s", zhash_cursor (self->options), item);
                    item = (char *) zhash_next (self->options);
                }
            }
            else
                zsys_debug ("(NULL)");
            zsys_debug ("    cache=");
            if (self->cache) {
                char *item = (char *) zhash_first (self->cache);
                while (item) {
                    zsys_debug ("        %s=%s", zhash_cursor (self->cache), item);
                    item = (char *) zhash_next (self->cache);
                }
            }
            else
                zsys_debug ("(NULL)");
            break;
            
        case FMQ_MSG_ICANHAZ_OK:
            zsys_debug ("FMQ_MSG_ICANHAZ_OK:");
            break;
            
        case FMQ_MSG_NOM:
            zsys_debug ("FMQ_MSG_NOM:");
            zsys_debug ("    credit=%ld", (long) self->credit);
            zsys_debug ("    sequence=%ld", (long) self->sequence);
            break;
            
        case FMQ_MSG_CHEEZBURGER:
            zsys_debug ("FMQ_MSG_CHEEZBURGER:");
            zsys_debug ("    sequence=%ld", (long) self->sequence);
            zsys_debug ("    operation=%ld", (long) self->operation);
            if (self->filename)
                zsys_debug ("    filename='%s'", self->filename);
            else
                zsys_debug ("    filename=");
            zsys_debug ("    offset=%ld", (long) self->offset);
            zsys_debug ("    eof=%ld", (long) self->eof);
            zsys_debug ("    headers=");
            if (self->headers) {
                char *item = (char *) zhash_first (self->headers);
                while (item) {
                    zsys_debug ("        %s=%s", zhash_cursor (self->headers), item);
                    item = (char *) zhash_next (self->headers);
                }
            }
            else
                zsys_debug ("(NULL)");
            zsys_debug ("    chunk=[ ... ]");
            break;
            
        case FMQ_MSG_HUGZ:
            zsys_debug ("FMQ_MSG_HUGZ:");
            break;
            
        case FMQ_MSG_HUGZ_OK:
            zsys_debug ("FMQ_MSG_HUGZ_OK:");
            break;
            
        case FMQ_MSG_KTHXBAI:
            zsys_debug ("FMQ_MSG_KTHXBAI:");
            break;
            
        case FMQ_MSG_SRSLY:
            zsys_debug ("FMQ_MSG_SRSLY:");
            if (self->reason)
                zsys_debug ("    reason='%s'", self->reason);
            else
                zsys_debug ("    reason=");
            break;
            
        case FMQ_MSG_RTFM:
            zsys_debug ("FMQ_MSG_RTFM:");
            if (self->reason)
                zsys_debug ("    reason='%s'", self->reason);
            else
                zsys_debug ("    reason=");
            break;
            
    }
}


//  --------------------------------------------------------------------------
//  Get/set the message routing_id

zframe_t *
fmq_msg_routing_id (fmq_msg_t *self)
{
    assert (self);
    return self->routing_id;
}

void
fmq_msg_set_routing_id (fmq_msg_t *self, zframe_t *routing_id)
{
    if (self->routing_id)
        zframe_destroy (&self->routing_id);
    self->routing_id = zframe_dup (routing_id);
}


//  --------------------------------------------------------------------------
//  Get/set the fmq_msg id

int
fmq_msg_id (fmq_msg_t *self)
{
    assert (self);
    return self->id;
}

void
fmq_msg_set_id (fmq_msg_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

const char *
fmq_msg_command (fmq_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case FMQ_MSG_OHAI:
            return ("OHAI");
            break;
        case FMQ_MSG_OHAI_OK:
            return ("OHAI_OK");
            break;
        case FMQ_MSG_ICANHAZ:
            return ("ICANHAZ");
            break;
        case FMQ_MSG_ICANHAZ_OK:
            return ("ICANHAZ_OK");
            break;
        case FMQ_MSG_NOM:
            return ("NOM");
            break;
        case FMQ_MSG_CHEEZBURGER:
            return ("CHEEZBURGER");
            break;
        case FMQ_MSG_HUGZ:
            return ("HUGZ");
            break;
        case FMQ_MSG_HUGZ_OK:
            return ("HUGZ_OK");
            break;
        case FMQ_MSG_KTHXBAI:
            return ("KTHXBAI");
            break;
        case FMQ_MSG_SRSLY:
            return ("SRSLY");
            break;
        case FMQ_MSG_RTFM:
            return ("RTFM");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the path field

const char *
fmq_msg_path (fmq_msg_t *self)
{
    assert (self);
    return self->path;
}

void
fmq_msg_set_path (fmq_msg_t *self, const char *format, ...)
{
    //  Format path from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->path);
    self->path = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get the options field without transferring ownership

zhash_t *
fmq_msg_options (fmq_msg_t *self)
{
    assert (self);
    return self->options;
}

//  Get the options field and transfer ownership to caller

zhash_t *
fmq_msg_get_options (fmq_msg_t *self)
{
    zhash_t *options = self->options;
    self->options = NULL;
    return options;
}

//  Set the options field, transferring ownership from caller

void
fmq_msg_set_options (fmq_msg_t *self, zhash_t **options_p)
{
    assert (self);
    assert (options_p);
    zhash_destroy (&self->options);
    self->options = *options_p;
    *options_p = NULL;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the options dictionary

const char *
fmq_msg_options_string (fmq_msg_t *self, const char *key, const char *default_value)
{
    assert (self);
    const char *value = NULL;
    if (self->options)
        value = (const char *) (zhash_lookup (self->options, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
fmq_msg_options_number (fmq_msg_t *self, const char *key, uint64_t default_value)
{
    assert (self);
    uint64_t value = default_value;
    char *string = NULL;
    if (self->options)
        string = (char *) (zhash_lookup (self->options, key));
    if (string)
        value = atol (string);

    return value;
}

void
fmq_msg_options_insert (fmq_msg_t *self, const char *key, const char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = zsys_vprintf (format, argptr);
    va_end (argptr);

    //  Store string in hash table
    if (!self->options) {
        self->options = zhash_new ();
        zhash_autofree (self->options);
    }
    zhash_update (self->options, key, string);
    free (string);
}

size_t
fmq_msg_options_size (fmq_msg_t *self)
{
    return zhash_size (self->options);
}


//  --------------------------------------------------------------------------
//  Get the cache field without transferring ownership

zhash_t *
fmq_msg_cache (fmq_msg_t *self)
{
    assert (self);
    return self->cache;
}

//  Get the cache field and transfer ownership to caller

zhash_t *
fmq_msg_get_cache (fmq_msg_t *self)
{
    zhash_t *cache = self->cache;
    self->cache = NULL;
    return cache;
}

//  Set the cache field, transferring ownership from caller

void
fmq_msg_set_cache (fmq_msg_t *self, zhash_t **cache_p)
{
    assert (self);
    assert (cache_p);
    zhash_destroy (&self->cache);
    self->cache = *cache_p;
    *cache_p = NULL;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the cache dictionary

const char *
fmq_msg_cache_string (fmq_msg_t *self, const char *key, const char *default_value)
{
    assert (self);
    const char *value = NULL;
    if (self->cache)
        value = (const char *) (zhash_lookup (self->cache, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
fmq_msg_cache_number (fmq_msg_t *self, const char *key, uint64_t default_value)
{
    assert (self);
    uint64_t value = default_value;
    char *string = NULL;
    if (self->cache)
        string = (char *) (zhash_lookup (self->cache, key));
    if (string)
        value = atol (string);

    return value;
}

void
fmq_msg_cache_insert (fmq_msg_t *self, const char *key, const char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = zsys_vprintf (format, argptr);
    va_end (argptr);

    //  Store string in hash table
    if (!self->cache) {
        self->cache = zhash_new ();
        zhash_autofree (self->cache);
    }
    zhash_update (self->cache, key, string);
    free (string);
}

size_t
fmq_msg_cache_size (fmq_msg_t *self)
{
    return zhash_size (self->cache);
}


//  --------------------------------------------------------------------------
//  Get/set the credit field

uint64_t
fmq_msg_credit (fmq_msg_t *self)
{
    assert (self);
    return self->credit;
}

void
fmq_msg_set_credit (fmq_msg_t *self, uint64_t credit)
{
    assert (self);
    self->credit = credit;
}


//  --------------------------------------------------------------------------
//  Get/set the sequence field

uint64_t
fmq_msg_sequence (fmq_msg_t *self)
{
    assert (self);
    return self->sequence;
}

void
fmq_msg_set_sequence (fmq_msg_t *self, uint64_t sequence)
{
    assert (self);
    self->sequence = sequence;
}


//  --------------------------------------------------------------------------
//  Get/set the operation field

byte
fmq_msg_operation (fmq_msg_t *self)
{
    assert (self);
    return self->operation;
}

void
fmq_msg_set_operation (fmq_msg_t *self, byte operation)
{
    assert (self);
    self->operation = operation;
}


//  --------------------------------------------------------------------------
//  Get/set the filename field

const char *
fmq_msg_filename (fmq_msg_t *self)
{
    assert (self);
    return self->filename;
}

void
fmq_msg_set_filename (fmq_msg_t *self, const char *format, ...)
{
    //  Format filename from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->filename);
    self->filename = zsys_vprintf (format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the offset field

uint64_t
fmq_msg_offset (fmq_msg_t *self)
{
    assert (self);
    return self->offset;
}

void
fmq_msg_set_offset (fmq_msg_t *self, uint64_t offset)
{
    assert (self);
    self->offset = offset;
}


//  --------------------------------------------------------------------------
//  Get/set the eof field

byte
fmq_msg_eof (fmq_msg_t *self)
{
    assert (self);
    return self->eof;
}

void
fmq_msg_set_eof (fmq_msg_t *self, byte eof)
{
    assert (self);
    self->eof = eof;
}


//  --------------------------------------------------------------------------
//  Get the headers field without transferring ownership

zhash_t *
fmq_msg_headers (fmq_msg_t *self)
{
    assert (self);
    return self->headers;
}

//  Get the headers field and transfer ownership to caller

zhash_t *
fmq_msg_get_headers (fmq_msg_t *self)
{
    zhash_t *headers = self->headers;
    self->headers = NULL;
    return headers;
}

//  Set the headers field, transferring ownership from caller

void
fmq_msg_set_headers (fmq_msg_t *self, zhash_t **headers_p)
{
    assert (self);
    assert (headers_p);
    zhash_destroy (&self->headers);
    self->headers = *headers_p;
    *headers_p = NULL;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the headers dictionary

const char *
fmq_msg_headers_string (fmq_msg_t *self, const char *key, const char *default_value)
{
    assert (self);
    const char *value = NULL;
    if (self->headers)
        value = (const char *) (zhash_lookup (self->headers, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
fmq_msg_headers_number (fmq_msg_t *self, const char *key, uint64_t default_value)
{
    assert (self);
    uint64_t value = default_value;
    char *string = NULL;
    if (self->headers)
        string = (char *) (zhash_lookup (self->headers, key));
    if (string)
        value = atol (string);

    return value;
}

void
fmq_msg_headers_insert (fmq_msg_t *self, const char *key, const char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = zsys_vprintf (format, argptr);
    va_end (argptr);

    //  Store string in hash table
    if (!self->headers) {
        self->headers = zhash_new ();
        zhash_autofree (self->headers);
    }
    zhash_update (self->headers, key, string);
    free (string);
}

size_t
fmq_msg_headers_size (fmq_msg_t *self)
{
    return zhash_size (self->headers);
}


//  --------------------------------------------------------------------------
//  Get the chunk field without transferring ownership

zchunk_t *
fmq_msg_chunk (fmq_msg_t *self)
{
    assert (self);
    return self->chunk;
}

//  Get the chunk field and transfer ownership to caller

zchunk_t *
fmq_msg_get_chunk (fmq_msg_t *self)
{
    zchunk_t *chunk = self->chunk;
    self->chunk = NULL;
    return chunk;
}

//  Set the chunk field, transferring ownership from caller

void
fmq_msg_set_chunk (fmq_msg_t *self, zchunk_t **chunk_p)
{
    assert (self);
    assert (chunk_p);
    zchunk_destroy (&self->chunk);
    self->chunk = *chunk_p;
    *chunk_p = NULL;
}


//  --------------------------------------------------------------------------
//  Get/set the reason field

const char *
fmq_msg_reason (fmq_msg_t *self)
{
    assert (self);
    return self->reason;
}

void
fmq_msg_set_reason (fmq_msg_t *self, const char *format, ...)
{
    //  Format reason from provided arguments
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->reason);
    self->reason = zsys_vprintf (format, argptr);
    va_end (argptr);
}



//  --------------------------------------------------------------------------
//  Selftest

int
fmq_msg_test (bool verbose)
{
    printf (" * fmq_msg: ");

    //  @selftest
    //  Simple create/destroy test
    fmq_msg_t *self = fmq_msg_new (0);
    assert (self);
    fmq_msg_destroy (&self);

    //  Create pair of sockets we can send through
    zsock_t *input = zsock_new (ZMQ_ROUTER);
    assert (input);
    zsock_connect (input, "inproc://selftest-fmq_msg");

    zsock_t *output = zsock_new (ZMQ_DEALER);
    assert (output);
    zsock_bind (output, "inproc://selftest-fmq_msg");

    //  Encode/send/decode and verify each message type
    int instance;
    fmq_msg_t *copy;
    self = fmq_msg_new (FMQ_MSG_OHAI);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_OHAI_OK);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_ICANHAZ);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    fmq_msg_set_path (self, "Life is short but Now lasts for ever");
    fmq_msg_options_insert (self, "Name", "Brutus");
    fmq_msg_options_insert (self, "Age", "%d", 43);
    fmq_msg_cache_insert (self, "Name", "Brutus");
    fmq_msg_cache_insert (self, "Age", "%d", 43);
    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        assert (streq (fmq_msg_path (self), "Life is short but Now lasts for ever"));
        assert (fmq_msg_options_size (self) == 2);
        assert (streq (fmq_msg_options_string (self, "Name", "?"), "Brutus"));
        assert (fmq_msg_options_number (self, "Age", 0) == 43);
        assert (fmq_msg_cache_size (self) == 2);
        assert (streq (fmq_msg_cache_string (self, "Name", "?"), "Brutus"));
        assert (fmq_msg_cache_number (self, "Age", 0) == 43);
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_ICANHAZ_OK);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_NOM);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    fmq_msg_set_credit (self, 123);
    fmq_msg_set_sequence (self, 123);
    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        assert (fmq_msg_credit (self) == 123);
        assert (fmq_msg_sequence (self) == 123);
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_CHEEZBURGER);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    fmq_msg_set_sequence (self, 123);
    fmq_msg_set_operation (self, 123);
    fmq_msg_set_filename (self, "Life is short but Now lasts for ever");
    fmq_msg_set_offset (self, 123);
    fmq_msg_set_eof (self, 123);
    fmq_msg_headers_insert (self, "Name", "Brutus");
    fmq_msg_headers_insert (self, "Age", "%d", 43);
    zchunk_t *cheezburger_chunk = zchunk_new ("Captcha Diem", 12);
    fmq_msg_set_chunk (self, &cheezburger_chunk);
    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        assert (fmq_msg_sequence (self) == 123);
        assert (fmq_msg_operation (self) == 123);
        assert (streq (fmq_msg_filename (self), "Life is short but Now lasts for ever"));
        assert (fmq_msg_offset (self) == 123);
        assert (fmq_msg_eof (self) == 123);
        assert (fmq_msg_headers_size (self) == 2);
        assert (streq (fmq_msg_headers_string (self, "Name", "?"), "Brutus"));
        assert (fmq_msg_headers_number (self, "Age", 0) == 43);
        assert (memcmp (zchunk_data (fmq_msg_chunk (self)), "Captcha Diem", 12) == 0);
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_HUGZ);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_HUGZ_OK);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_KTHXBAI);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_SRSLY);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    fmq_msg_set_reason (self, "Life is short but Now lasts for ever");
    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
        fmq_msg_destroy (&self);
    }
    self = fmq_msg_new (FMQ_MSG_RTFM);
    
    //  Check that _dup works on empty message
    copy = fmq_msg_dup (self);
    assert (copy);
    fmq_msg_destroy (&copy);

    fmq_msg_set_reason (self, "Life is short but Now lasts for ever");
    //  Send twice from same object
    fmq_msg_send_again (self, output);
    fmq_msg_send (&self, output);

    for (instance = 0; instance < 2; instance++) {
        self = fmq_msg_recv (input);
        assert (self);
        assert (fmq_msg_routing_id (self));
        
        assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
        fmq_msg_destroy (&self);
    }

    zsock_destroy (&input);
    zsock_destroy (&output);
    //  @end

    printf ("OK\n");
    return 0;
}
