/*  =========================================================================
    fmq_msg - The FileMQ Protocol

    Codec class for fmq_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: fmq_msg.xml, or
     * The code generation script that built this file: zproto_codec_c
    ************************************************************************
    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of FileMQ, a C implemenation of the protocol:    
    https://github.com/danriegsecker/filemq2.                          
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*
@header
    fmq_msg - The FileMQ Protocol
@discuss
@end
*/

#include "../include/fmq_msg.h"

//  Structure of our class

struct _fmq_msg_t {
    zframe_t *routing_id;               //  Routing_id from ROUTER, if any
    int id;                             //  fmq_msg message ID
    byte *needle;                       //  Read/write pointer for serialization
    byte *ceiling;                      //  Valid upper limit for read pointer
    /* Full path or path prefix  */
    char *path;
    /* Subscription options  */
    zhash_t *options;
    size_t options_bytes;               //  Size of hash content
    /* File SHA-1 signatures  */
    zhash_t *cache;
    size_t cache_bytes;                 //  Size of hash content
    /* Credit, in bytes  */
    uint64_t credit;
    /* Chunk sequence, 0 and up  */
    uint64_t sequence;
    /* Create=%d1 delete=%d2  */
    byte operation;
    /* Relative name of file  */
    char *filename;
    /* File offset in bytes  */
    uint64_t offset;
    /* Last chunk in file?  */
    byte eof;
    /* File properties  */
    zhash_t *headers;
    size_t headers_bytes;               //  Size of hash content
    /* Data chunk     */
    zchunk_t *chunk;
    /* Printable explanation, 255 characters  */
    char reason [256];
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
    if (self->needle + size > self->ceiling) { \
        zsys_warning ("fmq_msg: GET_OCTETS failed"); \
        goto malformed; \
    } \
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
    if (self->needle + 1 > self->ceiling) { \
        zsys_warning ("fmq_msg: GET_NUMBER1 failed"); \
        goto malformed; \
    } \
    (host) = *(byte *) self->needle; \
    self->needle++; \
}

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (self->needle + 2 > self->ceiling) { \
        zsys_warning ("fmq_msg: GET_NUMBER2 failed"); \
        goto malformed; \
    } \
    (host) = ((uint16_t) (self->needle [0]) << 8) \
           +  (uint16_t) (self->needle [1]); \
    self->needle += 2; \
}

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (self->needle + 4 > self->ceiling) { \
        zsys_warning ("fmq_msg: GET_NUMBER4 failed"); \
        goto malformed; \
    } \
    (host) = ((uint32_t) (self->needle [0]) << 24) \
           + ((uint32_t) (self->needle [1]) << 16) \
           + ((uint32_t) (self->needle [2]) << 8) \
           +  (uint32_t) (self->needle [3]); \
    self->needle += 4; \
}

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (self->needle + 8 > self->ceiling) { \
        zsys_warning ("fmq_msg: GET_NUMBER8 failed"); \
        goto malformed; \
    } \
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
    if (self->needle + string_size > (self->ceiling)) { \
        zsys_warning ("fmq_msg: GET_STRING failed"); \
        goto malformed; \
    } \
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
    if (self->needle + string_size > (self->ceiling)) { \
        zsys_warning ("fmq_msg: GET_LONGSTR failed"); \
        goto malformed; \
    } \
    free ((host)); \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), self->needle, string_size); \
    (host) [string_size] = 0; \
    self->needle += string_size; \
}


//  --------------------------------------------------------------------------
//  Create a new fmq_msg

fmq_msg_t *
fmq_msg_new (void)
{
    fmq_msg_t *self = (fmq_msg_t *) zmalloc (sizeof (fmq_msg_t));
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
        free (self->path);
        zhash_destroy (&self->options);
        zhash_destroy (&self->cache);
        free (self->filename);
        zhash_destroy (&self->headers);
        zchunk_destroy (&self->chunk);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Receive a fmq_msg from the socket. Returns 0 if OK, -1 if
//  there was an error. Blocks if there is no message waiting.

int
fmq_msg_recv (fmq_msg_t *self, zsock_t *input)
{
    assert (input);
    
    if (zsock_type (input) == ZMQ_ROUTER) {
        zframe_destroy (&self->routing_id);
        self->routing_id = zframe_recv (input);
        if (!self->routing_id || !zsock_rcvmore (input)) {
            zsys_warning ("fmq_msg: no routing ID");
            return -1;          //  Interrupted or malformed
        }
    }
    zmq_msg_t frame;
    zmq_msg_init (&frame);
    int size = zmq_msg_recv (&frame, zsock_resolve (input), 0);
    if (size == -1) {
        zsys_warning ("fmq_msg: interrupted");
        goto malformed;         //  Interrupted
    }
    //  Get and check protocol signature
    self->needle = (byte *) zmq_msg_data (&frame);
    self->ceiling = self->needle + zmq_msg_size (&frame);
    
    uint16_t signature;
    GET_NUMBER2 (signature);
    if (signature != (0xAAA0 | 3)) {
        zsys_warning ("fmq_msg: invalid signature");
        //  TODO: discard invalid messages and loop, and return
        //  -1 only on interrupt
        goto malformed;         //  Interrupted
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (self->id);

    switch (self->id) {
        case FMQ_MSG_OHAI:
            {
                char protocol [256];
                GET_STRING (protocol);
                if (strneq (protocol, "FILEMQ")) {
                    zsys_warning ("fmq_msg: protocol is invalid");
                    goto malformed;
                }
            }
            {
                uint16_t version;
                GET_NUMBER2 (version);
                if (version != FMQ_MSG_VERSION) {
                    zsys_warning ("fmq_msg: version is invalid");
                    goto malformed;
                }
            }
            break;

        case FMQ_MSG_OHAI_OK:
            break;

        case FMQ_MSG_ICANHAZ:
            GET_LONGSTR (self->path);
            {
                size_t hash_size;
                GET_NUMBER4 (hash_size);
                self->options = zhash_new ();
                zhash_autofree (self->options);
                while (hash_size--) {
                    char key [256];
                    char *value = NULL;
                    GET_STRING (key);
                    GET_LONGSTR (value);
                    zhash_insert (self->options, key, value);
                    free (value);
                }
            }
            {
                size_t hash_size;
                GET_NUMBER4 (hash_size);
                self->cache = zhash_new ();
                zhash_autofree (self->cache);
                while (hash_size--) {
                    char key [256];
                    char *value = NULL;
                    GET_STRING (key);
                    GET_LONGSTR (value);
                    zhash_insert (self->cache, key, value);
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
            GET_LONGSTR (self->filename);
            GET_NUMBER8 (self->offset);
            GET_NUMBER1 (self->eof);
            {
                size_t hash_size;
                GET_NUMBER4 (hash_size);
                self->headers = zhash_new ();
                zhash_autofree (self->headers);
                while (hash_size--) {
                    char key [256];
                    char *value = NULL;
                    GET_STRING (key);
                    GET_LONGSTR (value);
                    zhash_insert (self->headers, key, value);
                    free (value);
                }
            }
            {
                size_t chunk_size;
                GET_NUMBER4 (chunk_size);
                if (self->needle + chunk_size > (self->ceiling)) {
                    zsys_warning ("fmq_msg: chunk is missing data");
                    goto malformed;
                }
                zchunk_destroy (&self->chunk);
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
            zsys_warning ("fmq_msg: bad message ID");
            goto malformed;
    }
    //  Successful return
    zmq_msg_close (&frame);
    return 0;

    //  Error returns
    malformed:
        zsys_warning ("fmq_msg: fmq_msg malformed message, fail");
        zmq_msg_close (&frame);
        return -1;              //  Invalid message
}


//  --------------------------------------------------------------------------
//  Send the fmq_msg to the socket. Does not destroy it. Returns 0 if
//  OK, else -1.

int
fmq_msg_send (fmq_msg_t *self, zsock_t *output)
{
    assert (self);
    assert (output);

    if (zsock_type (output) == ZMQ_ROUTER)
        zframe_send (&self->routing_id, output, ZFRAME_MORE + ZFRAME_REUSE);

    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (self->id) {
        case FMQ_MSG_OHAI:
            frame_size += 1 + strlen ("FILEMQ");
            frame_size += 2;            //  version
            break;
        case FMQ_MSG_ICANHAZ:
            frame_size += 4;
            if (self->path)
                frame_size += strlen (self->path);
            frame_size += 4;            //  Size is 4 octets
            if (self->options) {
                self->options_bytes = 0;
                char *item = (char *) zhash_first (self->options);
                while (item) {
                    self->options_bytes += 1 + strlen (zhash_cursor (self->options));
                    self->options_bytes += 4 + strlen (item);
                    item = (char *) zhash_next (self->options);
                }
            }
            frame_size += self->options_bytes;
            frame_size += 4;            //  Size is 4 octets
            if (self->cache) {
                self->cache_bytes = 0;
                char *item = (char *) zhash_first (self->cache);
                while (item) {
                    self->cache_bytes += 1 + strlen (zhash_cursor (self->cache));
                    self->cache_bytes += 4 + strlen (item);
                    item = (char *) zhash_next (self->cache);
                }
            }
            frame_size += self->cache_bytes;
            break;
        case FMQ_MSG_NOM:
            frame_size += 8;            //  credit
            frame_size += 8;            //  sequence
            break;
        case FMQ_MSG_CHEEZBURGER:
            frame_size += 8;            //  sequence
            frame_size += 1;            //  operation
            frame_size += 4;
            if (self->filename)
                frame_size += strlen (self->filename);
            frame_size += 8;            //  offset
            frame_size += 1;            //  eof
            frame_size += 4;            //  Size is 4 octets
            if (self->headers) {
                self->headers_bytes = 0;
                char *item = (char *) zhash_first (self->headers);
                while (item) {
                    self->headers_bytes += 1 + strlen (zhash_cursor (self->headers));
                    self->headers_bytes += 4 + strlen (item);
                    item = (char *) zhash_next (self->headers);
                }
            }
            frame_size += self->headers_bytes;
            frame_size += 4;            //  Size is 4 octets
            if (self->chunk)
                frame_size += zchunk_size (self->chunk);
            break;
        case FMQ_MSG_SRSLY:
            frame_size += 1 + strlen (self->reason);
            break;
        case FMQ_MSG_RTFM:
            frame_size += 1 + strlen (self->reason);
            break;
    }
    //  Now serialize message into the frame
    zmq_msg_t frame;
    zmq_msg_init_size (&frame, frame_size);
    self->needle = (byte *) zmq_msg_data (&frame);
    PUT_NUMBER2 (0xAAA0 | 3);
    PUT_NUMBER1 (self->id);
    size_t nbr_frames = 1;              //  Total number of frames to send
    
    switch (self->id) {
        case FMQ_MSG_OHAI:
            PUT_STRING ("FILEMQ");
            PUT_NUMBER2 (FMQ_MSG_VERSION);
            break;

        case FMQ_MSG_ICANHAZ:
            if (self->path) {
                PUT_LONGSTR (self->path);
            }
            else
                PUT_NUMBER4 (0);    //  Empty string
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
                PUT_NUMBER4 (0);    //  Empty hash
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
                PUT_NUMBER4 (0);    //  Empty hash
            break;

        case FMQ_MSG_NOM:
            PUT_NUMBER8 (self->credit);
            PUT_NUMBER8 (self->sequence);
            break;

        case FMQ_MSG_CHEEZBURGER:
            PUT_NUMBER8 (self->sequence);
            PUT_NUMBER1 (self->operation);
            if (self->filename) {
                PUT_LONGSTR (self->filename);
            }
            else
                PUT_NUMBER4 (0);    //  Empty string
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
                PUT_NUMBER4 (0);    //  Empty hash
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

        case FMQ_MSG_SRSLY:
            PUT_STRING (self->reason);
            break;

        case FMQ_MSG_RTFM:
            PUT_STRING (self->reason);
            break;

    }
    //  Now send the data frame
    zmq_msg_send (&frame, zsock_resolve (output), --nbr_frames? ZMQ_SNDMORE: 0);
    
    return 0;
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
fmq_msg_set_path (fmq_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    free (self->path);
    self->path = strdup (value);
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
fmq_msg_set_filename (fmq_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    free (self->filename);
    self->filename = strdup (value);
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
fmq_msg_set_reason (fmq_msg_t *self, const char *value)
{
    assert (self);
    assert (value);
    if (value == self->reason)
        return;
    strncpy (self->reason, value, 255);
    self->reason [255] = 0;
}



//  --------------------------------------------------------------------------
//  Selftest

int
fmq_msg_test (bool verbose)
{
    printf (" * fmq_msg: ");

    //  Silence an "unused" warning by "using" the verbose variable
    if (verbose) {;}

    //  @selftest
    //  Simple create/destroy test
    fmq_msg_t *self = fmq_msg_new ();
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
    self = fmq_msg_new ();
    fmq_msg_set_id (self, FMQ_MSG_OHAI);

    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
    }
    fmq_msg_set_id (self, FMQ_MSG_OHAI_OK);

    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
    }
    fmq_msg_set_id (self, FMQ_MSG_ICANHAZ);

    fmq_msg_set_path (self, "Life is short but Now lasts for ever");
    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
        assert (streq (fmq_msg_path (self), "Life is short but Now lasts for ever"));
    }
    fmq_msg_set_id (self, FMQ_MSG_ICANHAZ_OK);

    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
    }
    fmq_msg_set_id (self, FMQ_MSG_NOM);

    fmq_msg_set_credit (self, 123);
    fmq_msg_set_sequence (self, 123);
    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
        assert (fmq_msg_credit (self) == 123);
        assert (fmq_msg_sequence (self) == 123);
    }
    fmq_msg_set_id (self, FMQ_MSG_CHEEZBURGER);

    fmq_msg_set_sequence (self, 123);
    fmq_msg_set_operation (self, 123);
    fmq_msg_set_filename (self, "Life is short but Now lasts for ever");
    fmq_msg_set_offset (self, 123);
    fmq_msg_set_eof (self, 123);
    zchunk_t *cheezburger_chunk = zchunk_new ("Captcha Diem", 12);
    fmq_msg_set_chunk (self, &cheezburger_chunk);
    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
        assert (fmq_msg_sequence (self) == 123);
        assert (fmq_msg_operation (self) == 123);
        assert (streq (fmq_msg_filename (self), "Life is short but Now lasts for ever"));
        assert (fmq_msg_offset (self) == 123);
        assert (fmq_msg_eof (self) == 123);
        assert (memcmp (zchunk_data (fmq_msg_chunk (self)), "Captcha Diem", 12) == 0);
    }
    fmq_msg_set_id (self, FMQ_MSG_HUGZ);

    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
    }
    fmq_msg_set_id (self, FMQ_MSG_HUGZ_OK);

    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
    }
    fmq_msg_set_id (self, FMQ_MSG_KTHXBAI);

    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
    }
    fmq_msg_set_id (self, FMQ_MSG_SRSLY);

    fmq_msg_set_reason (self, "Life is short but Now lasts for ever");
    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
        assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
    }
    fmq_msg_set_id (self, FMQ_MSG_RTFM);

    fmq_msg_set_reason (self, "Life is short but Now lasts for ever");
    //  Send twice
    fmq_msg_send (self, output);
    fmq_msg_send (self, output);

    for (instance = 0; instance < 2; instance++) {
        fmq_msg_recv (self, input);
        assert (fmq_msg_routing_id (self));
        assert (streq (fmq_msg_reason (self), "Life is short but Now lasts for ever"));
    }

    fmq_msg_destroy (&self);
    zsock_destroy (&input);
    zsock_destroy (&output);
    //  @end

    printf ("OK\n");
    return 0;
}
