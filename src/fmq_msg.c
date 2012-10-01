/*  =========================================================================
    fmq_msg.c

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
    byte identity [16];
    zlist_t *mechanisms;
    size_t mechanisms_bytes;
    zframe_t *challenge;
    char *mechanism;
    zframe_t *response;
    char *path;
    zhash_t *options;
    size_t options_bytes;
    int64_t credit;
    int64_t sequence;
    byte operation;
    char *filename;
    int64_t offset;
    zhash_t *headers;
    size_t headers_bytes;
    zframe_t *chunk;
    char *reason;
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Strings are encoded with 1-byte length
#define STRING_MAX  255

//  Put an octet to the frame
#define PUT_OCTET(host) { \
    *(byte *) self->needle = (host); \
    self->needle++; \
    }

//  Get an octet from the frame
#define GET_OCTET(host) { \
    if (self->needle + 1 > self->ceiling) \
        goto malformed; \
    (host) = *(byte *) self->needle; \
    self->needle++; \
    }

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

//  Put a long long integer to the frame
#define PUT_NUMBER(host) { \
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

//  Get a number from the frame
#define GET_NUMBER(host) { \
    if (self->needle + 8 > self->ceiling) \
        goto malformed; \
    (host) = ((int64_t) (self->needle [0]) << 56) \
           + ((int64_t) (self->needle [1]) << 48) \
           + ((int64_t) (self->needle [2]) << 40) \
           + ((int64_t) (self->needle [3]) << 32) \
           + ((int64_t) (self->needle [4]) << 24) \
           + ((int64_t) (self->needle [5]) << 16) \
           + ((int64_t) (self->needle [6]) << 8) \
           +  (int64_t) (self->needle [7]); \
    self->needle += 8; \
    }

//  Put a string to the frame
#define PUT_STRING(host) { \
    string_size = strlen (host); \
    PUT_OCTET (string_size); \
    memcpy (self->needle, (host), string_size); \
    self->needle += string_size; \
    }

//  Get a string from the frame
#define GET_STRING(host) { \
    GET_OCTET (string_size); \
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
        if (self->mechanisms) {
            while (zlist_size (self->mechanisms))
                free (zlist_pop (self->mechanisms));
            zlist_destroy (&self->mechanisms);
        }
        zframe_destroy (&self->challenge);
        free (self->mechanism);
        zframe_destroy (&self->response);
        free (self->path);
        zhash_destroy (&self->options);
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
fmq_msg_recv (void *socket)
{
    assert (socket);
    fmq_msg_t *self = fmq_msg_new (0);
    zframe_t *frame = NULL;

    //  If we're reading from a ROUTER socket, get address
    if (zsockopt_type (socket) == ZMQ_ROUTER) {
        self->address = zframe_recv (socket);
        if (!self->address)
            goto empty;         //  Interrupted
        if (!zsocket_rcvmore (socket))
            goto malformed;
    }
    //  Read and parse command in frame
    frame = zframe_recv (socket);
    if (!frame)
        goto empty;             //  Interrupted
    self->needle = zframe_data (frame);
    self->ceiling = self->needle + zframe_size (frame);
    size_t string_size;
    size_t list_size;
    size_t hash_size;

    //  Get message id, which is first byte in frame
    GET_OCTET (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            free (self->protocol);
            GET_STRING (self->protocol);
            if (strneq (self->protocol, "FILEMQ"))
                goto malformed;
            GET_OCTET (self->version);
            if (self->version != FMQ_MSG_VERSION)
                goto malformed;
            GET_BLOCK (self->identity, 16);
            break;

        case FMQ_MSG_ORLY:
            GET_OCTET (list_size);
            self->mechanisms = zlist_new ();
            while (list_size--) {
                char *string;
                GET_STRING (string);
                zlist_append (self->mechanisms, string);
                self->mechanisms_bytes += strlen (string);
            }
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (socket))
                goto malformed;
            self->challenge = zframe_recv (socket);
            break;

        case FMQ_MSG_YARLY:
            free (self->mechanism);
            GET_STRING (self->mechanism);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (socket))
                goto malformed;
            self->response = zframe_recv (socket);
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            free (self->path);
            GET_STRING (self->path);
            GET_OCTET (hash_size);
            self->options = zhash_new ();
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (self->options, string, strdup (value));
                zhash_freefn (self->options, string, free);
                self->options_bytes += 1 + strlen (string) + 1 + strlen (value);
                free (string);
            }
            break;

        case FMQ_MSG_ICANHAZ_OK:
            break;

        case FMQ_MSG_NOM:
            GET_NUMBER (self->credit);
            GET_NUMBER (self->sequence);
            break;

        case FMQ_MSG_CHEEZBURGER:
            GET_NUMBER (self->sequence);
            GET_OCTET (self->operation);
            free (self->filename);
            GET_STRING (self->filename);
            GET_NUMBER (self->offset);
            GET_OCTET (hash_size);
            self->headers = zhash_new ();
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (self->headers, string, strdup (value));
                zhash_freefn (self->headers, string, free);
                self->headers_bytes += 1 + strlen (string) + 1 + strlen (value);
                free (string);
            }
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (socket))
                goto malformed;
            self->chunk = zframe_recv (socket);
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


//  Serialize options key=value pair
int
s_options_write (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    char string [STRING_MAX + 1];
    snprintf (string, STRING_MAX, "%s=%s", key, (char *) item);
    size_t string_size;
    PUT_STRING (string);
    return 0;
}

//  Serialize headers key=value pair
int
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

void
fmq_msg_send (fmq_msg_t **self_p, void *socket)
{
    assert (socket);
    assert (self_p);
    assert (*self_p);

    //  Calculate size of serialized data
    fmq_msg_t *self = *self_p;
    size_t frame_size = 1;
    switch (self->id) {
        case FMQ_MSG_OHAI:
            //  protocol is a string with 1-byte length
            frame_size += 1 + strlen ("FILEMQ");
            //  version is an octet
            frame_size += 1;
            //  identity is a block of 16 bytes
            frame_size += 16;
            break;
            
        case FMQ_MSG_ORLY:
            //  mechanisms is an array of strings
            frame_size++;       //  Size is one octet
            if (self->mechanisms)
                frame_size += zlist_size (self->mechanisms) + self->mechanisms_bytes;
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
            if (self->options)
                frame_size += zhash_size (self->options) + self->options_bytes;
            break;
            
        case FMQ_MSG_ICANHAZ_OK:
            break;
            
        case FMQ_MSG_NOM:
            //  credit is an 8-byte integer
            frame_size += 8;
            //  sequence is an 8-byte integer
            frame_size += 8;
            break;
            
        case FMQ_MSG_CHEEZBURGER:
            //  sequence is an 8-byte integer
            frame_size += 8;
            //  operation is an octet
            frame_size += 1;
            //  filename is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (self->filename)
                frame_size += strlen (self->filename);
            //  offset is an 8-byte integer
            frame_size += 8;
            //  headers is an array of key=value strings
            frame_size++;       //  Size is one octet
            if (self->headers)
                frame_size += zhash_size (self->headers) + self->headers_bytes;
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
            return;
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (NULL, frame_size + 1);
    self->needle = zframe_data (frame);
    size_t string_size;
    int frame_flags = 0;
    PUT_OCTET (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            PUT_STRING ("FILEMQ");
            PUT_OCTET (FMQ_MSG_VERSION);
            PUT_BLOCK (self->identity, 16);
            break;
            
        case FMQ_MSG_ORLY:
            if (self->mechanisms != NULL) {
                PUT_OCTET (zlist_size (self->mechanisms));
                char *mechanisms = (char *) zlist_first (self->mechanisms);
                while (mechanisms) {
                    PUT_STRING (mechanisms);
                    mechanisms = (char *) zlist_next (self->mechanisms);
                }
            }
            else
                PUT_OCTET (0);      //  Empty string array
            frame_flags = ZFRAME_MORE;
            break;
            
        case FMQ_MSG_YARLY:
            if (self->mechanism) {
                PUT_STRING (self->mechanism);
            }
            else
                PUT_OCTET (0);      //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case FMQ_MSG_OHAI_OK:
            break;
            
        case FMQ_MSG_ICANHAZ:
            if (self->path) {
                PUT_STRING (self->path);
            }
            else
                PUT_OCTET (0);      //  Empty string
            if (self->options != NULL) {
                PUT_OCTET (zhash_size (self->options));
                zhash_foreach (self->options, s_options_write, self);
            }
            else
                PUT_OCTET (0);      //  Empty dictionary
            break;
            
        case FMQ_MSG_ICANHAZ_OK:
            break;
            
        case FMQ_MSG_NOM:
            PUT_NUMBER (self->credit);
            PUT_NUMBER (self->sequence);
            break;
            
        case FMQ_MSG_CHEEZBURGER:
            PUT_NUMBER (self->sequence);
            PUT_OCTET (self->operation);
            if (self->filename) {
                PUT_STRING (self->filename);
            }
            else
                PUT_OCTET (0);      //  Empty string
            PUT_NUMBER (self->offset);
            if (self->headers != NULL) {
                PUT_OCTET (zhash_size (self->headers));
                zhash_foreach (self->headers, s_headers_write, self);
            }
            else
                PUT_OCTET (0);      //  Empty dictionary
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
                PUT_OCTET (0);      //  Empty string
            break;
            
        case FMQ_MSG_RTFM:
            if (self->reason) {
                PUT_STRING (self->reason);
            }
            else
                PUT_OCTET (0);      //  Empty string
            break;
            
    }
    //  If we're sending to a ROUTER, we send the address first
    if (zsockopt_type (socket) == ZMQ_ROUTER) {
        assert (self->address);
        zframe_send (&self->address, socket, ZFRAME_MORE);
    }
    //  Now send the data frame
    zframe_send (&frame, socket, frame_flags);
    
    //  Now send any frame fields, in order
    switch (self->id) {
        case FMQ_MSG_ORLY:
        //  If challenge isn't set, send an empty frame
        if (!self->challenge)
            self->challenge = zframe_new (NULL, 0);
            zframe_send (&self->challenge, socket, 0);
            break;
        case FMQ_MSG_YARLY:
        //  If response isn't set, send an empty frame
        if (!self->response)
            self->response = zframe_new (NULL, 0);
            zframe_send (&self->response, socket, 0);
            break;
        case FMQ_MSG_CHEEZBURGER:
        //  If chunk isn't set, send an empty frame
        if (!self->chunk)
            self->chunk = zframe_new (NULL, 0);
            zframe_send (&self->chunk, socket, 0);
            break;
    }
    //  Destroy fmq_msg object
    fmq_msg_destroy (self_p);
}


//  Dump options key=value pair to stdout
int
s_options_dump (const char *key, void *item, void *argument)
{
    fmq_msg_t *self = (fmq_msg_t *) argument;
    printf ("        %s=%s\n", key, (char *) item);
    return 0;
}

//  Dump headers key=value pair to stdout
int
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
            printf ("    identity=");
            int identity_index;
            for (identity_index = 0; identity_index < 16; identity_index++) {
                if (identity_index && (identity_index % 4 == 0))
                    printf ("-");
                printf ("%02X", self->identity [identity_index]);
            }
            printf ("\n");
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
            printf ("    operation=%d\n", self->operation);
            if (self->filename)
                printf ("    filename='%s'\n", self->filename);
            else
                printf ("    filename=\n");
            printf ("    offset=%ld\n", (long) self->offset);
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
fmq_msg_address_set (fmq_msg_t *self, zframe_t *address)
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
fmq_msg_id_set (fmq_msg_t *self, int id)
{
    self->id = id;
}

//  --------------------------------------------------------------------------
//  Get/set the identity field

byte *
fmq_msg_identity (fmq_msg_t *self)
{
    assert (self);
    return self->identity;
}

void
fmq_msg_identity_set (fmq_msg_t *self, byte *identity)
{
    assert (self);
    memcpy (self->identity, identity, 16);
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
    if (!self->mechanisms)
        self->mechanisms = zlist_new ();
    zlist_append (self->mechanisms, string);
    self->mechanisms_bytes += strlen (string);
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
fmq_msg_challenge_set (fmq_msg_t *self, zframe_t *frame)
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
fmq_msg_mechanism_set (fmq_msg_t *self, char *format, ...)
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
fmq_msg_response_set (fmq_msg_t *self, zframe_t *frame)
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
fmq_msg_path_set (fmq_msg_t *self, char *format, ...)
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

int64_t
fmq_msg_options_number (fmq_msg_t *self, char *key, int64_t default_value)
{
    assert (self);
    int64_t value = default_value;
    char *string;
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
    if (!self->options)
        self->options = zhash_new ();
    if (zhash_insert (self->options, key, string) == 0) {
        zhash_freefn (self->options, key, free);
        //  Count key=value
        self->options_bytes += strlen (key) + 1 + strlen (string);
    }
}

size_t
fmq_msg_options_size (fmq_msg_t *self)
{
    return zhash_size (self->options);
}


//  --------------------------------------------------------------------------
//  Get/set the credit field

int64_t
fmq_msg_credit (fmq_msg_t *self)
{
    assert (self);
    return self->credit;
}

void
fmq_msg_credit_set (fmq_msg_t *self, int64_t credit)
{
    assert (self);
    self->credit = credit;
}


//  --------------------------------------------------------------------------
//  Get/set the sequence field

int64_t
fmq_msg_sequence (fmq_msg_t *self)
{
    assert (self);
    return self->sequence;
}

void
fmq_msg_sequence_set (fmq_msg_t *self, int64_t sequence)
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
fmq_msg_operation_set (fmq_msg_t *self, byte operation)
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
fmq_msg_filename_set (fmq_msg_t *self, char *format, ...)
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

int64_t
fmq_msg_offset (fmq_msg_t *self)
{
    assert (self);
    return self->offset;
}

void
fmq_msg_offset_set (fmq_msg_t *self, int64_t offset)
{
    assert (self);
    self->offset = offset;
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

int64_t
fmq_msg_headers_number (fmq_msg_t *self, char *key, int64_t default_value)
{
    assert (self);
    int64_t value = default_value;
    char *string;
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
    if (!self->headers)
        self->headers = zhash_new ();
    if (zhash_insert (self->headers, key, string) == 0) {
        zhash_freefn (self->headers, key, free);
        //  Count key=value
        self->headers_bytes += strlen (key) + 1 + strlen (string);
    }
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
fmq_msg_chunk_set (fmq_msg_t *self, zframe_t *frame)
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
fmq_msg_reason_set (fmq_msg_t *self, char *format, ...)
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
    byte identity_data [FMQ_MSG_IDENTITY_SIZE];
    memset (identity_data, 123, FMQ_MSG_IDENTITY_SIZE);
    fmq_msg_identity_set (self, identity_data);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_identity (self) [0] == 123);
    assert (fmq_msg_identity (self) [FMQ_MSG_IDENTITY_SIZE - 1] == 123);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_ORLY);
    fmq_msg_mechanisms_append (self, "Name: %s", "Brutus");
    fmq_msg_mechanisms_append (self, "Age: %d", 43);
    fmq_msg_challenge_set (self, zframe_new ("Captcha Diem", 12));
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_mechanisms_size (self) == 2);
    assert (streq (fmq_msg_mechanisms_first (self), "Name: Brutus"));
    assert (streq (fmq_msg_mechanisms_next (self), "Age: 43"));
    assert (zframe_streq (fmq_msg_challenge (self), "Captcha Diem"));
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_YARLY);
    fmq_msg_mechanism_set (self, "Life is short but Now lasts for ever");
    fmq_msg_response_set (self, zframe_new ("Captcha Diem", 12));
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
    fmq_msg_path_set (self, "Life is short but Now lasts for ever");
    fmq_msg_options_insert (self, "Name", "Brutus");
    fmq_msg_options_insert (self, "Age", "%d", 43);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_path (self), "Life is short but Now lasts for ever"));
    assert (fmq_msg_options_size (self) == 2);
    assert (streq (fmq_msg_options_string (self, "Name", "?"), "Brutus"));
    assert (fmq_msg_options_number (self, "Age", 0) == 43);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_ICANHAZ_OK);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_NOM);
    fmq_msg_credit_set (self, 12345678);
    fmq_msg_sequence_set (self, 12345678);
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_credit (self) == 12345678);
    assert (fmq_msg_sequence (self) == 12345678);
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_CHEEZBURGER);
    fmq_msg_sequence_set (self, 12345678);
    fmq_msg_operation_set (self, 123);
    fmq_msg_filename_set (self, "Life is short but Now lasts for ever");
    fmq_msg_offset_set (self, 12345678);
    fmq_msg_headers_insert (self, "Name", "Brutus");
    fmq_msg_headers_insert (self, "Age", "%d", 43);
    fmq_msg_chunk_set (self, zframe_new ("Captcha Diem", 12));
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (fmq_msg_sequence (self) == 12345678);
    assert (fmq_msg_operation (self) == 123);
    assert (streq (fmq_msg_filename (self), "Life is short but Now lasts for ever"));
    assert (fmq_msg_offset (self) == 12345678);
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
    fmq_msg_reason_set (self, "Life is short but Now lasts for ever");
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
    fmq_msg_destroy (&self);

    self = fmq_msg_new (FMQ_MSG_RTFM);
    fmq_msg_reason_set (self, "Life is short but Now lasts for ever");
    fmq_msg_send (&self, output);
    
    self = fmq_msg_recv (input);
    assert (self);
    assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
    fmq_msg_destroy (&self);

    zctx_destroy (&ctx);
    printf ("OK\n");
    return 0;
}
