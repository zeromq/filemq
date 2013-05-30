/*  =========================================================================
    fmq_msg - work with filemq messages

    Generated codec implementation for fmq_msg
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

/*
@header
    fmq_msg - work with filemq messages
@discuss
@end
*/

#include <czmq.h>
#include "../include/fmq_msg.h"

//  Structure of our class

struct _fmq_msg_t {
    zframe_t *address;          //  Address of peer if any
    int id;                     //  fmq_msg message ID
    byte *needle;               //  Read/write pointer for serialization
    byte *ceiling;              //  Valid upper limit for read pointer
    char *protocol;
    byte version;
    zlist_t *mechanisms;
    zframe_t *challenge;
    char *mechanism;
    zframe_t *response;
    char *path;
    zhash_t *options;
    size_t options_bytes;       //  Size of dictionary content
    zhash_t *cache;
    size_t cache_bytes;         //  Size of dictionary content
    uint64_t credit;
    uint64_t sequence;
    byte operation;
    char *filename;
    uint64_t offset;
    byte eof;
    zhash_t *headers;
    size_t headers_bytes;       //  Size of dictionary content
    zframe_t *chunk;
    char *reason;
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Strings are encoded with 1-byte length
#define STRING_MAX  255

//  Put a block to the frame
#define PUT_BLOCK(host,size) { \
    memcpy (self->needle, (host), size); \
    self->needle += size; \
    }

//  Get a block from the frame
#define GET_BLOCK(host,size) { \
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
    string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
    }

//  Get a string from the frame
#define GET_STRING(host) { \
    GET_NUMBER1 (string_size); \
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
        zframe_destroy (&self->address);
        free (self->protocol);
        if (self->mechanisms)
            zlist_destroy (&self->mechanisms);
        zframe_destroy (&self->challenge);
        free (self->mechanism);
        zframe_destroy (&self->response);
        free (self->path);
        zhash_destroy (&self->options);
        zhash_destroy (&self->cache);
        free (self->filename);
        zhash_destroy (&self->headers);
        zframe_destroy (&self->chunk);
        free (self->reason);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Receive and parse a fmq_msg from the socket. Returns new object or
//  NULL if error. Will block if there's no message waiting.

fmq_msg_t *
fmq_msg_recv (void *input)
{
    assert (input);
    fmq_msg_t *self = fmq_msg_new (0);
    zframe_t *frame = NULL;
    size_t string_size;
    size_t list_size;
    size_t hash_size;

    //  Read valid message frame from socket; we loop over any
    //  garbage data we might receive from badly-connected peers
    while (true) {
        //  If we're reading from a ROUTER socket, get address
        if (zsockopt_type (input) == ZMQ_ROUTER) {
            zframe_destroy (&self->address);
            self->address = zframe_recv (input);
            if (!self->address)
                goto empty;         //  Interrupted
            if (!zsocket_rcvmore (input))
                goto malformed;
        }
        //  Read and parse command in frame
        frame = zframe_recv (input);
        if (!frame)
            goto empty;             //  Interrupted

        //  Get and check protocol signature
        self->needle = zframe_data (frame);
        self->ceiling = self->needle + zframe_size (frame);
        uint16_t signature;
        GET_NUMBER2 (signature);
        if (signature == (0xAAA0 | 3))
            break;                  //  Valid signature

        //  Protocol assertion, drop message
        while (zsocket_rcvmore (input)) {
            zframe_destroy (&frame);
            frame = zframe_recv (input);
        }
        zframe_destroy (&frame);
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            free (self->protocol);
            GET_STRING (self->protocol);
            if (strneq (self->protocol, "FILEMQ"))
                goto malformed;
            GET_NUMBER1 (self->version);
            if (self->version != FMQ_MSG_VERSION)
                goto malformed;
            break;

        case FMQ_MSG_ORLY:
            GET_NUMBER1 (list_size);
            self->mechanisms = zlist_new ();
            zlist_autofree (self->mechanisms);
            while (list_size--) {
                char *string;
                GET_STRING (string);
                zlist_append (self->mechanisms, string);
                free (string);
            }
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->challenge = zframe_recv (input);
            break;

        case FMQ_MSG_YARLY:
            free (self->mechanism);
            GET_STRING (self->mechanism);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->response = zframe_recv (input);
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            free (self->path);
            GET_STRING (self->path);
            GET_NUMBER1 (hash_size);
            self->options = zhash_new ();
            zhash_autofree (self->options);
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (self->options, string, value);
                free (string);
            }
            GET_NUMBER1 (hash_size);
            self->cache = zhash_new ();
            zhash_autofree (self->cache);
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (self->cache, string, value);
                free (string);
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
            free (self->filename);
            GET_STRING (self->filename);
            GET_NUMBER8 (self->offset);
            GET_NUMBER1 (self->eof);
            GET_NUMBER1 (hash_size);
            self->headers = zhash_new ();
            zhash_autofree (self->headers);
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (self->headers, string, value);
                free (string);
            }
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            self->chunk = zframe_recv (input);
            break;

        case FMQ_MSG_HUGZ:
            break;

        case FMQ_MSG_HUGZ_OK:
            break;

        case FMQ_MSG_KTHXBAI:
            break;

        case FMQ_MSG_SRSLY:
            free (self->reason);
            GET_STRING (self->reason);
            break;

        case FMQ_MSG_RTFM:
            free (self->reason);
            GET_STRING (self->reason);
            break;

        default:
            goto malformed;
    }
    //  Successful return
    zframe_destroy (&frame);
    return self;

    //  Error returns
    malformed:
        printf ("E: malformed message '%d'\n", self->id);
    empty:
        zframe_destroy (&frame);
        fmq_msg_destroy (&self);
        return (NULL);
}

//  Count size of key=value pair
static int
s_options_count (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    self->options_bytes += strlen (key) + 1 + strlen ((char *) item) + 1;
    return 0;
}

//  Serialize options key=value pair
static int
s_options_write (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    char string [STRING_MAX + 1];
    snprintf (string, STRING_MAX, "%s=%s", key, (char *) item);
    size_t string_size;
    PUT_STRING (string);
    return 0;
}

//  Count size of key=value pair
static int
s_cache_count (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    self->cache_bytes += strlen (key) + 1 + strlen ((char *) item) + 1;
    return 0;
}

//  Serialize cache key=value pair
static int
s_cache_write (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    char string [STRING_MAX + 1];
    snprintf (string, STRING_MAX, "%s=%s", key, (char *) item);
    size_t string_size;
    PUT_STRING (string);
    return 0;
}

//  Count size of key=value pair
static int
s_headers_count (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    self->headers_bytes += strlen (key) + 1 + strlen ((char *) item) + 1;
    return 0;
}

//  Serialize headers key=value pair
static int
s_headers_write (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    char string [STRING_MAX + 1];
    snprintf (string, STRING_MAX, "%s=%s", key, (char *) item);
    size_t string_size;
    PUT_STRING (string);
    return 0;
}


//  --------------------------------------------------------------------------
//  Send the fmq_msg to the socket, and destroy it
//  Returns 0 if OK, else -1

int
fmq_msg_send (fmq_msg_t **self_p, void *output)
{
    assert (output);
    assert (self_p);
    assert (*self_p);

    //  Calculate size of serialized data
    fmq_msg_t *self = *self_p;
    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case FMQ_MSG_OHAI:
            //  protocol is a string with 1-byte length
            frame_size += 1 + strlen ("FILEMQ");
            //  version is a 1-byte integer
            frame_size += 1;
            break;
            
        case FMQ_MSG_ORLY:
            //  mechanisms is an array of strings
            frame_size++;       //  Size is one octet
            if (self->mechanisms) {
                //  Add up size of list contents
                char *mechanisms = (char *) zlist_first (self->mechanisms);
                while (mechanisms) {
                    frame_size += 1 + strlen (mechanisms);
                    mechanisms = (char *) zlist_next (self->mechanisms);
                }
            }
            break;
            
        case FMQ_MSG_YARLY:
            //  mechanism is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->mechanism)
                frame_size += strlen (self->mechanism);
            break;
            
        case FMQ_MSG_OHAI_OK:
            break;
            
        case FMQ_MSG_ICANHAZ:
            //  path is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->path)
                frame_size += strlen (self->path);
            //  options is an array of key=value strings
            frame_size++;       //  Size is one octet
            if (self->options) {
                self->options_bytes = 0;
                //  Add up size of dictionary contents
                zhash_foreach (self->options, s_options_count, self);
            }
            frame_size += self->options_bytes;
            //  cache is an array of key=value strings
            frame_size++;       //  Size is one octet
            if (self->cache) {
                self->cache_bytes = 0;
                //  Add up size of dictionary contents
                zhash_foreach (self->cache, s_cache_count, self);
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
            frame_size++;       //  Size is one octet
            if (self->headers) {
                self->headers_bytes = 0;
                //  Add up size of dictionary contents
                zhash_foreach (self->headers, s_headers_count, self);
            }
            frame_size += self->headers_bytes;
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
            printf ("E: bad message type '%d', not sent\n", self->id);
            //  No recovery, this is a fatal application error
            assert (false);
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (NULL, frame_size);
    self->needle = zframe_data (frame);
    size_t string_size;
    int frame_flags = 0;
    PUT_NUMBER2 (0xAAA0 | 3);
    PUT_NUMBER1 (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            PUT_STRING ("FILEMQ");
            PUT_NUMBER1 (FMQ_MSG_VERSION);
            break;
            
        case FMQ_MSG_ORLY:
            if (self->mechanisms != NULL) {
                PUT_NUMBER1 (zlist_size (self->mechanisms));
                char *mechanisms = (char *) zlist_first (self->mechanisms);
                while (mechanisms) {
                    PUT_STRING (mechanisms);
                    mechanisms = (char *) zlist_next (self->mechanisms);
                }
            }
            else
                PUT_NUMBER1 (0);    //  Empty string array
            frame_flags = ZFRAME_MORE;
            break;
            
        case FMQ_MSG_YARLY:
            if (self->mechanism) {
                PUT_STRING (self->mechanism);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case FMQ_MSG_OHAI_OK:
            break;
            
        case FMQ_MSG_ICANHAZ:
            if (self->path) {
                PUT_STRING (self->path);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            if (self->options != NULL) {
                PUT_NUMBER1 (zhash_size (self->options));
                zhash_foreach (self->options, s_options_write, self);
            }
            else
                PUT_NUMBER1 (0);    //  Empty dictionary
            if (self->cache != NULL) {
                PUT_NUMBER1 (zhash_size (self->cache));
                zhash_foreach (self->cache, s_cache_write, self);
            }
            else
                PUT_NUMBER1 (0);    //  Empty dictionary
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
            if (self->headers != NULL) {
                PUT_NUMBER1 (zhash_size (self->headers));
                zhash_foreach (self->headers, s_headers_write, self);
            }
            else
                PUT_NUMBER1 (0);    //  Empty dictionary
            frame_flags = ZFRAME_MORE;
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
    //  If we're sending to a ROUTER, we send the address first
    if (zsockopt_type (output) == ZMQ_ROUTER) {
        assert (self->address);
        if (zframe_send (&self->address, output, ZFRAME_MORE)) {
            zframe_destroy (&frame);
            fmq_msg_destroy (self_p);
            return -1;
        }
    }
    //  Now send the data frame
    if (zframe_send (&frame, output, frame_flags)) {
        zframe_destroy (&frame);
        fmq_msg_destroy (self_p);
        return -1;
    }
    
    //  Now send any frame fields, in order
    switch (self->id) {
        case FMQ_MSG_ORLY:
            //  If challenge isn't set, send an empty frame
            if (!self->challenge)
                self->challenge = zframe_new (NULL, 0);
            if (zframe_send (&self->challenge, output, 0)) {
                zframe_destroy (&frame);
                fmq_msg_destroy (self_p);
                return -1;
            }
            break;
        case FMQ_MSG_YARLY:
            //  If response isn't set, send an empty frame
            if (!self->response)
                self->response = zframe_new (NULL, 0);
            if (zframe_send (&self->response, output, 0)) {
                zframe_destroy (&frame);
                fmq_msg_destroy (self_p);
                return -1;
            }
            break;
        case FMQ_MSG_CHEEZBURGER:
            //  If chunk isn't set, send an empty frame
            if (!self->chunk)
                self->chunk = zframe_new (NULL, 0);
            if (zframe_send (&self->chunk, output, 0)) {
                zframe_destroy (&frame);
                fmq_msg_destroy (self_p);
                return -1;
            }
            break;
    }
    //  Destroy fmq_msg object
    fmq_msg_destroy (self_p);
    return 0;
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
//  Send the ORLY to the socket in one step

int
fmq_msg_send_orly (
    void *output,
    zlist_t *mechanisms,
    zframe_t *challenge)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_ORLY);
    fmq_msg_set_mechanisms (self, zlist_dup (mechanisms));
    fmq_msg_set_challenge (self, zframe_dup (challenge));
    return fmq_msg_send (&self, output);
}


//  --------------------------------------------------------------------------
//  Send the YARLY to the socket in one step

int
fmq_msg_send_yarly (
    void *output,
    char *mechanism,
    zframe_t *response)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_YARLY);
    fmq_msg_set_mechanism (self, mechanism);
    fmq_msg_set_response (self, zframe_dup (response));
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
    char *path,
    zhash_t *options,
    zhash_t *cache)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_set_path (self, path);
    fmq_msg_set_options (self, zhash_dup (options));
    fmq_msg_set_cache (self, zhash_dup (cache));
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
    char *filename,
    uint64_t offset,
    byte eof,
    zhash_t *headers,
    zframe_t *chunk)
{
    fmq_msg_t *self = fmq_msg_new (FMQ_MSG_CHEEZBURGER);
    fmq_msg_set_sequence (self, sequence);
    fmq_msg_set_operation (self, operation);
    fmq_msg_set_filename (self, filename);
    fmq_msg_set_offset (self, offset);
    fmq_msg_set_eof (self, eof);
    fmq_msg_set_headers (self, zhash_dup (headers));
    fmq_msg_set_chunk (self, zframe_dup (chunk));
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
    char *reason)
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
    char *reason)
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
    if (self->address)
        copy->address = zframe_dup (self->address);
    switch (self->id) {
        case FMQ_MSG_OHAI:
            copy->protocol = strdup (self->protocol);
            copy->version = self->version;
            break;

        case FMQ_MSG_ORLY:
            copy->mechanisms = zlist_copy (self->mechanisms);
            copy->challenge = zframe_dup (self->challenge);
            break;

        case FMQ_MSG_YARLY:
            copy->mechanism = strdup (self->mechanism);
            copy->response = zframe_dup (self->response);
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            copy->path = strdup (self->path);
            copy->options = zhash_dup (self->options);
            copy->cache = zhash_dup (self->cache);
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
            copy->filename = strdup (self->filename);
            copy->offset = self->offset;
            copy->eof = self->eof;
            copy->headers = zhash_dup (self->headers);
            copy->chunk = zframe_dup (self->chunk);
            break;

        case FMQ_MSG_HUGZ:
            break;

        case FMQ_MSG_HUGZ_OK:
            break;

        case FMQ_MSG_KTHXBAI:
            break;

        case FMQ_MSG_SRSLY:
            copy->reason = strdup (self->reason);
            break;

        case FMQ_MSG_RTFM:
            copy->reason = strdup (self->reason);
            break;

    }
    return copy;
}


//  Dump options key=value pair to stdout
static int
s_options_dump (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    printf ("        %s=%s\n", key, (char *) item);
    return 0;
}

//  Dump cache key=value pair to stdout
static int
s_cache_dump (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    printf ("        %s=%s\n", key, (char *) item);
    return 0;
}

//  Dump headers key=value pair to stdout
static int
s_headers_dump (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    printf ("        %s=%s\n", key, (char *) item);
    return 0;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
fmq_msg_dump (fmq_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case FMQ_MSG_OHAI:
            puts ("OHAI:");
            printf ("    protocol=filemq\n");
            printf ("    version=fmq_msg_version\n");
            break;
            
        case FMQ_MSG_ORLY:
            puts ("ORLY:");
            printf ("    mechanisms={");
            if (self->mechanisms) {
                char *mechanisms = (char *) zlist_first (self->mechanisms);
                while (mechanisms) {
                    printf (" '%s'", mechanisms);
                    mechanisms = (char *) zlist_next (self->mechanisms);
                }
            }
            printf (" }\n");
            printf ("    challenge={\n");
            if (self->challenge) {
                size_t size = zframe_size (self->challenge);
                byte *data = zframe_data (self->challenge);
                printf ("        size=%td\n", zframe_size (self->challenge));
                if (size > 32)
                    size = 32;
                int challenge_index;
                for (challenge_index = 0; challenge_index < size; challenge_index++) {
                    if (challenge_index && (challenge_index % 4 == 0))
                        printf ("-");
                    printf ("%02X", data [challenge_index]);
                }
            }
            printf ("    }\n");
            break;
            
        case FMQ_MSG_YARLY:
            puts ("YARLY:");
            if (self->mechanism)
                printf ("    mechanism='%s'\n", self->mechanism);
            else
                printf ("    mechanism=\n");
            printf ("    response={\n");
            if (self->response) {
                size_t size = zframe_size (self->response);
                byte *data = zframe_data (self->response);
                printf ("        size=%td\n", zframe_size (self->response));
                if (size > 32)
                    size = 32;
                int response_index;
                for (response_index = 0; response_index < size; response_index++) {
                    if (response_index && (response_index % 4 == 0))
                        printf ("-");
                    printf ("%02X", data [response_index]);
                }
            }
            printf ("    }\n");
            break;
            
        case FMQ_MSG_OHAI_OK:
            puts ("OHAI_OK:");
            break;
            
        case FMQ_MSG_ICANHAZ:
            puts ("ICANHAZ:");
            if (self->path)
                printf ("    path='%s'\n", self->path);
            else
                printf ("    path=\n");
            printf ("    options={\n");
            if (self->options)
                zhash_foreach (self->options, s_options_dump, self);
            printf ("    }\n");
            printf ("    cache={\n");
            if (self->cache)
                zhash_foreach (self->cache, s_cache_dump, self);
            printf ("    }\n");
            break;
            
        case FMQ_MSG_ICANHAZ_OK:
            puts ("ICANHAZ_OK:");
            break;
            
        case FMQ_MSG_NOM:
            puts ("NOM:");
            printf ("    credit=%ld\n", (long) self->credit);
            printf ("    sequence=%ld\n", (long) self->sequence);
            break;
            
        case FMQ_MSG_CHEEZBURGER:
            puts ("CHEEZBURGER:");
            printf ("    sequence=%ld\n", (long) self->sequence);
            printf ("    operation=%ld\n", (long) self->operation);
            if (self->filename)
                printf ("    filename='%s'\n", self->filename);
            else
                printf ("    filename=\n");
            printf ("    offset=%ld\n", (long) self->offset);
            printf ("    eof=%ld\n", (long) self->eof);
            printf ("    headers={\n");
            if (self->headers)
                zhash_foreach (self->headers, s_headers_dump, self);
            printf ("    }\n");
            printf ("    chunk={\n");
            if (self->chunk) {
                size_t size = zframe_size (self->chunk);
                byte *data = zframe_data (self->chunk);
                printf ("        size=%td\n", zframe_size (self->chunk));
                if (size > 32)
                    size = 32;
                int chunk_index;
                for (chunk_index = 0; chunk_index < size; chunk_index++) {
                    if (chunk_index && (chunk_index % 4 == 0))
                        printf ("-");
                    printf ("%02X", data [chunk_index]);
                }
            }
            printf ("    }\n");
            break;
            
        case FMQ_MSG_HUGZ:
            puts ("HUGZ:");
            break;
            
        case FMQ_MSG_HUGZ_OK:
            puts ("HUGZ_OK:");
            break;
            
        case FMQ_MSG_KTHXBAI:
            puts ("KTHXBAI:");
            break;
            
        case FMQ_MSG_SRSLY:
            puts ("SRSLY:");
            if (self->reason)
                printf ("    reason='%s'\n", self->reason);
            else
                printf ("    reason=\n");
            break;
            
        case FMQ_MSG_RTFM:
            puts ("RTFM:");
            if (self->reason)
                printf ("    reason='%s'\n", self->reason);
            else
                printf ("    reason=\n");
            break;
            
    }
}


//  --------------------------------------------------------------------------
//  Get/set the message address

zframe_t *
fmq_msg_address (fmq_msg_t *self)
{
    assert (self);
    return self->address;
}

void
fmq_msg_set_address (fmq_msg_t *self, zframe_t *address)
{
    if (self->address)
        zframe_destroy (&self->address);
    self->address = zframe_dup (address);
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

char *
fmq_msg_command (fmq_msg_t *self)
{
    assert (self);
    switch (self->id) {
        case FMQ_MSG_OHAI:
            return ("OHAI");
            break;
        case FMQ_MSG_ORLY:
            return ("ORLY");
            break;
        case FMQ_MSG_YARLY:
            return ("YARLY");
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
//  Get/set the mechanisms field

zlist_t *
fmq_msg_mechanisms (fmq_msg_t *self)
{
    assert (self);
    return self->mechanisms;
}

//  Greedy function, takes ownership of mechanisms; if you don't want that
//  then use zlist_dup() to pass a copy of mechanisms

void
fmq_msg_set_mechanisms (fmq_msg_t *self, zlist_t *mechanisms)
{
    assert (self);
    zlist_destroy (&self->mechanisms);
    self->mechanisms = mechanisms;
}

//  --------------------------------------------------------------------------
//  Iterate through the mechanisms field, and append a mechanisms value

char *
fmq_msg_mechanisms_first (fmq_msg_t *self)
{
    assert (self);
    if (self->mechanisms)
        return (char *) (zlist_first (self->mechanisms));
    else
        return NULL;
}

char *
fmq_msg_mechanisms_next (fmq_msg_t *self)
{
    assert (self);
    if (self->mechanisms)
        return (char *) (zlist_next (self->mechanisms));
    else
        return NULL;
}

void
fmq_msg_mechanisms_append (fmq_msg_t *self, char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = (char *) malloc (STRING_MAX + 1);
    assert (string);
    vsnprintf (string, STRING_MAX, format, argptr);
    va_end (argptr);
    
    //  Attach string to list
    if (!self->mechanisms) {
        self->mechanisms = zlist_new ();
        zlist_autofree (self->mechanisms);
    }
    zlist_append (self->mechanisms, string);
    free (string);
}

size_t
fmq_msg_mechanisms_size (fmq_msg_t *self)
{
    return zlist_size (self->mechanisms);
}


//  --------------------------------------------------------------------------
//  Get/set the challenge field

zframe_t *
fmq_msg_challenge (fmq_msg_t *self)
{
    assert (self);
    return self->challenge;
}

//  Takes ownership of supplied frame
void
fmq_msg_set_challenge (fmq_msg_t *self, zframe_t *frame)
{
    assert (self);
    if (self->challenge)
        zframe_destroy (&self->challenge);
    self->challenge = frame;
}

//  --------------------------------------------------------------------------
//  Get/set the mechanism field

char *
fmq_msg_mechanism (fmq_msg_t *self)
{
    assert (self);
    return self->mechanism;
}

void
fmq_msg_set_mechanism (fmq_msg_t *self, char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->mechanism);
    self->mechanism = (char *) malloc (STRING_MAX + 1);
    assert (self->mechanism);
    vsnprintf (self->mechanism, STRING_MAX, format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the response field

zframe_t *
fmq_msg_response (fmq_msg_t *self)
{
    assert (self);
    return self->response;
}

//  Takes ownership of supplied frame
void
fmq_msg_set_response (fmq_msg_t *self, zframe_t *frame)
{
    assert (self);
    if (self->response)
        zframe_destroy (&self->response);
    self->response = frame;
}

//  --------------------------------------------------------------------------
//  Get/set the path field

char *
fmq_msg_path (fmq_msg_t *self)
{
    assert (self);
    return self->path;
}

void
fmq_msg_set_path (fmq_msg_t *self, char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->path);
    self->path = (char *) malloc (STRING_MAX + 1);
    assert (self->path);
    vsnprintf (self->path, STRING_MAX, format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the options field

zhash_t *
fmq_msg_options (fmq_msg_t *self)
{
    assert (self);
    return self->options;
}

//  Greedy function, takes ownership of options; if you don't want that
//  then use zhash_dup() to pass a copy of options

void
fmq_msg_set_options (fmq_msg_t *self, zhash_t *options)
{
    assert (self);
    zhash_destroy (&self->options);
    self->options = options;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the options dictionary

char *
fmq_msg_options_string (fmq_msg_t *self, char *key, char *default_value)
{
    assert (self);
    char *value = NULL;
    if (self->options)
        value = (char *) (zhash_lookup (self->options, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
fmq_msg_options_number (fmq_msg_t *self, char *key, uint64_t default_value)
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
fmq_msg_options_insert (fmq_msg_t *self, char *key, char *format, ...)
{
    //  Format string into buffer
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = (char *) malloc (STRING_MAX + 1);
    assert (string);
    vsnprintf (string, STRING_MAX, format, argptr);
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
//  Get/set the cache field

zhash_t *
fmq_msg_cache (fmq_msg_t *self)
{
    assert (self);
    return self->cache;
}

//  Greedy function, takes ownership of cache; if you don't want that
//  then use zhash_dup() to pass a copy of cache

void
fmq_msg_set_cache (fmq_msg_t *self, zhash_t *cache)
{
    assert (self);
    zhash_destroy (&self->cache);
    self->cache = cache;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the cache dictionary

char *
fmq_msg_cache_string (fmq_msg_t *self, char *key, char *default_value)
{
    assert (self);
    char *value = NULL;
    if (self->cache)
        value = (char *) (zhash_lookup (self->cache, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
fmq_msg_cache_number (fmq_msg_t *self, char *key, uint64_t default_value)
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
fmq_msg_cache_insert (fmq_msg_t *self, char *key, char *format, ...)
{
    //  Format string into buffer
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = (char *) malloc (STRING_MAX + 1);
    assert (string);
    vsnprintf (string, STRING_MAX, format, argptr);
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

char *
fmq_msg_filename (fmq_msg_t *self)
{
    assert (self);
    return self->filename;
}

void
fmq_msg_set_filename (fmq_msg_t *self, char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->filename);
    self->filename = (char *) malloc (STRING_MAX + 1);
    assert (self->filename);
    vsnprintf (self->filename, STRING_MAX, format, argptr);
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
//  Get/set the headers field

zhash_t *
fmq_msg_headers (fmq_msg_t *self)
{
    assert (self);
    return self->headers;
}

//  Greedy function, takes ownership of headers; if you don't want that
//  then use zhash_dup() to pass a copy of headers

void
fmq_msg_set_headers (fmq_msg_t *self, zhash_t *headers)
{
    assert (self);
    zhash_destroy (&self->headers);
    self->headers = headers;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the headers dictionary

char *
fmq_msg_headers_string (fmq_msg_t *self, char *key, char *default_value)
{
    assert (self);
    char *value = NULL;
    if (self->headers)
        value = (char *) (zhash_lookup (self->headers, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
fmq_msg_headers_number (fmq_msg_t *self, char *key, uint64_t default_value)
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
fmq_msg_headers_insert (fmq_msg_t *self, char *key, char *format, ...)
{
    //  Format string into buffer
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    char *string = (char *) malloc (STRING_MAX + 1);
    assert (string);
    vsnprintf (string, STRING_MAX, format, argptr);
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
//  Get/set the chunk field

zframe_t *
fmq_msg_chunk (fmq_msg_t *self)
{
    assert (self);
    return self->chunk;
}

//  Takes ownership of supplied frame
void
fmq_msg_set_chunk (fmq_msg_t *self, zframe_t *frame)
{
    assert (self);
    if (self->chunk)
        zframe_destroy (&self->chunk);
    self->chunk = frame;
}

//  --------------------------------------------------------------------------
//  Get/set the reason field

char *
fmq_msg_reason (fmq_msg_t *self)
{
    assert (self);
    return self->reason;
}

void
fmq_msg_set_reason (fmq_msg_t *self, char *format, ...)
{
    //  Format into newly allocated string
    assert (self);
    va_list argptr;
    va_start (argptr, format);
    free (self->reason);
    self->reason = (char *) malloc (STRING_MAX + 1);
    assert (self->reason);
    vsnprintf (self->reason, STRING_MAX, format, argptr);
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
    zctx_t *ctx = zctx_new ();
    assert (ctx);

    void *output = zsocket_new (ctx, ZMQ_DEALER);
    assert (output);
    zsocket_bind (output, "inproc://selftest");
    void *input = zsocket_new (ctx, ZMQ_ROUTER);
    assert (input);
    zsocket_connect (input, "inproc://selftest");
    
    //  Encode/send/decode and verify each message type

    self = fmq_msg_new (FMQ_MSG_OHAI);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_ORLY);
    fmq_msg_mechanisms_append (self, "Name: %s", "Brutus");
    fmq_msg_mechanisms_append (self, "Age: %d", 43);
    fmq_msg_set_challenge (self, zframe_new ("Captcha Diem", 12));
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_mechanisms_size (self) == 2);
    assert (streq (fmq_msg_mechanisms_first (self), "Name: Brutus"));
    assert (streq (fmq_msg_mechanisms_next (self), "Age: 43"));
    assert (zframe_streq (fmq_msg_challenge (self), "Captcha Diem"));
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_YARLY);
    fmq_msg_set_mechanism (self, "Life is short but Now lasts for ever");
    fmq_msg_set_response (self, zframe_new ("Captcha Diem", 12));
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_mechanism (self), "Life is short but Now lasts for ever"));
    assert (zframe_streq (fmq_msg_response (self), "Captcha Diem"));
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_OHAI_OK);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_ICANHAZ);
    fmq_msg_set_path (self, "Life is short but Now lasts for ever");
    fmq_msg_options_insert (self, "Name", "Brutus");
    fmq_msg_options_insert (self, "Age", "%d", 43);
    fmq_msg_cache_insert (self, "Name", "Brutus");
    fmq_msg_cache_insert (self, "Age", "%d", 43);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_path (self), "Life is short but Now lasts for ever"));
    assert (fmq_msg_options_size (self) == 2);
    assert (streq (fmq_msg_options_string (self, "Name", "?"), "Brutus"));
    assert (fmq_msg_options_number (self, "Age", 0) == 43);
    assert (fmq_msg_cache_size (self) == 2);
    assert (streq (fmq_msg_cache_string (self, "Name", "?"), "Brutus"));
    assert (fmq_msg_cache_number (self, "Age", 0) == 43);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_ICANHAZ_OK);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_set_credit (self, 123);
    fmq_msg_set_sequence (self, 123);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_credit (self) == 123);
    assert (fmq_msg_sequence (self) == 123);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_CHEEZBURGER);
    fmq_msg_set_sequence (self, 123);
    fmq_msg_set_operation (self, 123);
    fmq_msg_set_filename (self, "Life is short but Now lasts for ever");
    fmq_msg_set_offset (self, 123);
    fmq_msg_set_eof (self, 123);
    fmq_msg_headers_insert (self, "Name", "Brutus");
    fmq_msg_headers_insert (self, "Age", "%d", 43);
    fmq_msg_set_chunk (self, zframe_new ("Captcha Diem", 12));
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_sequence (self) == 123);
    assert (fmq_msg_operation (self) == 123);
    assert (streq (fmq_msg_filename (self), "Life is short but Now lasts for ever"));
    assert (fmq_msg_offset (self) == 123);
    assert (fmq_msg_eof (self) == 123);
    assert (fmq_msg_headers_size (self) == 2);
    assert (streq (fmq_msg_headers_string (self, "Name", "?"), "Brutus"));
    assert (fmq_msg_headers_number (self, "Age", 0) == 43);
    assert (zframe_streq (fmq_msg_chunk (self), "Captcha Diem"));
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_HUGZ);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_HUGZ_OK);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_KTHXBAI);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_SRSLY);
    fmq_msg_set_reason (self, "Life is short but Now lasts for ever");
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_RTFM);
    fmq_msg_set_reason (self, "Life is short but Now lasts for ever");
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
    fmq_msg_destroy (&self);

    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
