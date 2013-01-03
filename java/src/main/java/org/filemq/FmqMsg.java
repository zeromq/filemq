/*  =========================================================================
    FmqMsg.java
    
    Generated codec class for FmqMsg
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

package org.filemq;

import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.ArrayList;
import java.util.HashMap;
import java.nio.ByteBuffer;

import org.zeromq.ZFrame;
import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;

//  Opaque class structure
public class FmqMsg 
{
    public static final int FMQ_MSG_VERSION                 = 1;
    public static final int FMQ_MSG_FILE_CREATE             = 1;
    public static final int FMQ_MSG_FILE_DELETE             = 2;

    public static final int OHAI                  = 1;
    public static final int ORLY                  = 2;
    public static final int YARLY                 = 3;
    public static final int OHAI_OK               = 4;
    public static final int ICANHAZ               = 5;
    public static final int ICANHAZ_OK            = 6;
    public static final int NOM                   = 7;
    public static final int CHEEZBURGER           = 8;
    public static final int HUGZ                  = 9;
    public static final int HUGZ_OK               = 10;
    public static final int KTHXBAI               = 11;
    public static final int SRSLY                 = 128;
    public static final int RTFM                  = 129;

    //  Structure of our class
    private ZFrame address;             //  Address of peer if any
    private int id;                     //  FmqMsg message ID
    private ByteBuffer needle;          //  Read/write pointer for serialization
    private String protocol;
    private int version;
    private List <String> mechanisms;
    private ZFrame challenge;
    private String mechanism;
    private ZFrame response;
    private String path;
    private Map <String, String> options;
    private int optionsBytes;
    private Map <String, String> cache;
    private int cacheBytes;
    private long credit;
    private long sequence;
    private int operation;
    private String filename;
    private long offset;
    private int eof;
    private Map <String, String> headers;
    private int headersBytes;
    private ZFrame chunk;
    private String reason;


    //  --------------------------------------------------------------------------
    //  Create a new FmqMsg

    public FmqMsg (int id)
    {
        this.id = id;
    }


    //  --------------------------------------------------------------------------
    //  Destroy the fmq_msg

    public void destroy ()
    {
        //  Free class properties
        if (address != null)
            address.destroy ();
        address = null;
    }


    //  --------------------------------------------------------------------------
    //  Network data encoding macros


    //  Put a 1-byte number to the frame
    private final void putNumber1 (int value) 
    {
        needle.put ((byte) value);
    }

    //  Get a 1-byte number to the frame
    //  then make it unsigned
    private int getNumber1 () 
    { 
        int value = needle.get (); 
        if (value < 0)
            value = (0xff) & value;
        return value;
    }

    //  Put a 2-byte number to the frame
    private final void putNumber2 (int value) 
    {
        needle.putShort ((short) value);
    }

    //  Get a 2-byte number to the frame
    private int getNumber2 () 
    { 
        int value = needle.getShort (); 
        if (value < 0)
            value = (0xffff) & value;
        return value;
    }

    //  Put a 4-byte number to the frame
    private final void putNumber4 (long value) 
    {
        needle.putInt ((int) value);
    }

    //  Get a 4-byte number to the frame
    //  then make it unsigned
    private long getNumber4 () 
    { 
        long value = needle.getInt (); 
        if (value < 0)
            value = (0xffffffff) & value;
        return value;
    }

    //  Put a 8-byte number to the frame
    public void putNumber8 (long value) 
    {
        needle.putLong (value);
    }

    //  Get a 8-byte number to the frame
    public long getNumber8 () 
    {
        return needle.getLong ();
    }


    //  Put a block to the frame
    private void putBlock (byte [] value, int size) 
    {
        needle.put (value, 0, size);
    }

    private byte [] getBlock (int size) 
    {
        byte [] value = new byte [size]; 
        needle.get (value);

        return value;
    }

    //  Put a string to the frame
    public void putString (String value) 
    {
        needle.put ((byte) value.length ());
        needle.put (value.getBytes());
    }

    //  Get a string from the frame
    public String getString () 
    {
        int size = getNumber1 ();
        byte [] value = new byte [size];
        needle.get (value);

        return new String (value);
    }

    //  --------------------------------------------------------------------------
    //  Receive and parse a FmqMsg from the socket. Returns new object or
    //  null if error. Will block if there's no message waiting.

    public static FmqMsg recv (Socket input)
    {
        assert (input != null);
        FmqMsg self = new FmqMsg (0);
        ZFrame frame = null;

        try {
            //  Read valid message frame from socket; we loop over any
            //  garbage data we might receive from badly-connected peers
            while (true) {
                //  If we're reading from a ROUTER socket, get address
                if (input.getType () == ZMQ.ROUTER) {
                    self.address = ZFrame.recvFrame (input);
                    if (self.address == null)
                        return null;         //  Interrupted
                    if (!input.hasReceiveMore ())
                        throw new IllegalArgumentException ();
                }
                //  Read and parse command in frame
                frame = ZFrame.recvFrame (input);
                if (frame == null)
                    return null;             //  Interrupted

                //  Get and check protocol signature
                self.needle = ByteBuffer.wrap (frame.getData ()); 
                int signature = self.getNumber2 ();
                if (signature == (0xAAA0 | 3))
                    break;                  //  Valid signature

                //  Protocol assertion, drop message
                while (input.hasReceiveMore ()) {
                    frame.destroy ();
                    frame = ZFrame.recvFrame (input);
                }
                frame.destroy ();
            }

            //  Get message id, which is first byte in frame
            self.id = self.getNumber1 ();
            int listSize;
            int hashSize;

            switch (self.id) {
            case OHAI:
                self.protocol = self.getString ();
                if (!self.protocol.equals( "FILEMQ"))
                    throw new IllegalArgumentException ();
                self.version = self.getNumber1 ();
                if (self.version != FMQ_MSG_VERSION)
                    throw new IllegalArgumentException ();
                break;

            case ORLY:
                listSize = self.getNumber1 ();
                self.mechanisms = new ArrayList<String> ();
                while (listSize-- > 0) {
                    String string = self.getString ();
                    self.mechanisms.add (string);
                }
                //  Get next frame, leave current untouched
                if (!input.hasReceiveMore ())
                    throw new IllegalArgumentException ();
                self.challenge = ZFrame.recvFrame (input);
                break;

            case YARLY:
                self.mechanism = self.getString ();
                //  Get next frame, leave current untouched
                if (!input.hasReceiveMore ())
                    throw new IllegalArgumentException ();
                self.response = ZFrame.recvFrame (input);
                break;

            case OHAI_OK:
                break;

            case ICANHAZ:
                self.path = self.getString ();
                hashSize = self.getNumber1 ();
                self.options = new HashMap <String, String> ();
                while (hashSize-- > 0) {
                    String string = self.getString ();
                    String [] kv = string.split("=");
                    self.options.put(kv[0], kv[1]);
                }

                hashSize = self.getNumber1 ();
                self.cache = new HashMap <String, String> ();
                while (hashSize-- > 0) {
                    String string = self.getString ();
                    String [] kv = string.split("=");
                    self.cache.put(kv[0], kv[1]);
                }

                break;

            case ICANHAZ_OK:
                break;

            case NOM:
                self.credit = self.getNumber8 ();
                self.sequence = self.getNumber8 ();
                break;

            case CHEEZBURGER:
                self.sequence = self.getNumber8 ();
                self.operation = self.getNumber1 ();
                self.filename = self.getString ();
                self.offset = self.getNumber8 ();
                self.eof = self.getNumber1 ();
                hashSize = self.getNumber1 ();
                self.headers = new HashMap <String, String> ();
                while (hashSize-- > 0) {
                    String string = self.getString ();
                    String [] kv = string.split("=");
                    self.headers.put(kv[0], kv[1]);
                }

                //  Get next frame, leave current untouched
                if (!input.hasReceiveMore ())
                    throw new IllegalArgumentException ();
                self.chunk = ZFrame.recvFrame (input);
                break;

            case HUGZ:
                break;

            case HUGZ_OK:
                break;

            case KTHXBAI:
                break;

            case SRSLY:
                self.reason = self.getString ();
                break;

            case RTFM:
                self.reason = self.getString ();
                break;

            default:
                throw new IllegalArgumentException ();
            }

            return self;

        } catch (Exception e) {
            //  Error returns
            System.out.printf ("E: malformed message '%d'\n", self.id);
            self.destroy ();
            return null;
        } finally {
            if (frame != null)
                frame.destroy ();
        }
    }


    //  Count size of key=value pair
    private static void 
    optionsCount (final Map.Entry <String, String> entry, FmqMsg self)
    {
        self.optionsBytes += entry.getKey ().length () + 1 + entry.getValue ().length () + 1;
    }

    //  Serialize options key=value pair
    private static void
    optionsWrite (final Map.Entry <String, String> entry, FmqMsg self)
    {
        String string = entry.getKey () + "=" + entry.getValue ();
        self.putString (string);
    }

    //  Count size of key=value pair
    private static void 
    cacheCount (final Map.Entry <String, String> entry, FmqMsg self)
    {
        self.cacheBytes += entry.getKey ().length () + 1 + entry.getValue ().length () + 1;
    }

    //  Serialize cache key=value pair
    private static void
    cacheWrite (final Map.Entry <String, String> entry, FmqMsg self)
    {
        String string = entry.getKey () + "=" + entry.getValue ();
        self.putString (string);
    }

    //  Count size of key=value pair
    private static void 
    headersCount (final Map.Entry <String, String> entry, FmqMsg self)
    {
        self.headersBytes += entry.getKey ().length () + 1 + entry.getValue ().length () + 1;
    }

    //  Serialize headers key=value pair
    private static void
    headersWrite (final Map.Entry <String, String> entry, FmqMsg self)
    {
        String string = entry.getKey () + "=" + entry.getValue ();
        self.putString (string);
    }


    //  --------------------------------------------------------------------------
    //  Send the FmqMsg to the socket, and destroy it

    public boolean send (Socket socket)
    {
        assert (socket != null);

        //  Calculate size of serialized data
        int frameSize = 2 + 1;          //  Signature and message ID
        switch (id) {
        case OHAI:
            //  protocol is a string with 1-byte length
            frameSize += 1 + "FILEMQ".length ();
            //  version is a 1-byte integer
            frameSize += 1;
            break;
            
        case ORLY:
            //  mechanisms is an array of strings
            frameSize++;       //  Size is one octet
            if (mechanisms != null) {
                for (String value : mechanisms) 
                    frameSize += 1 + value.length ();
            }
            break;
            
        case YARLY:
            //  mechanism is a string with 1-byte length
            frameSize++;       //  Size is one octet
            if (mechanism != null)
                frameSize += mechanism.length ();
            break;
            
        case OHAI_OK:
            break;
            
        case ICANHAZ:
            //  path is a string with 1-byte length
            frameSize++;       //  Size is one octet
            if (path != null)
                frameSize += path.length ();
            //  options is an array of key=value strings
            frameSize++;       //  Size is one octet
            if (options != null) {
                optionsBytes = 0;
                for (Map.Entry <String, String> entry: options.entrySet ()) {
                    optionsCount (entry, this);
                }
                frameSize += optionsBytes;
            }
            //  cache is an array of key=value strings
            frameSize++;       //  Size is one octet
            if (cache != null) {
                cacheBytes = 0;
                for (Map.Entry <String, String> entry: cache.entrySet ()) {
                    cacheCount (entry, this);
                }
                frameSize += cacheBytes;
            }
            break;
            
        case ICANHAZ_OK:
            break;
            
        case NOM:
            //  credit is a 8-byte integer
            frameSize += 8;
            //  sequence is a 8-byte integer
            frameSize += 8;
            break;
            
        case CHEEZBURGER:
            //  sequence is a 8-byte integer
            frameSize += 8;
            //  operation is a 1-byte integer
            frameSize += 1;
            //  filename is a string with 1-byte length
            frameSize++;       //  Size is one octet
            if (filename != null)
                frameSize += filename.length ();
            //  offset is a 8-byte integer
            frameSize += 8;
            //  eof is a 1-byte integer
            frameSize += 1;
            //  headers is an array of key=value strings
            frameSize++;       //  Size is one octet
            if (headers != null) {
                headersBytes = 0;
                for (Map.Entry <String, String> entry: headers.entrySet ()) {
                    headersCount (entry, this);
                }
                frameSize += headersBytes;
            }
            break;
            
        case HUGZ:
            break;
            
        case HUGZ_OK:
            break;
            
        case KTHXBAI:
            break;
            
        case SRSLY:
            //  reason is a string with 1-byte length
            frameSize++;       //  Size is one octet
            if (reason != null)
                frameSize += reason.length ();
            break;
            
        case RTFM:
            //  reason is a string with 1-byte length
            frameSize++;       //  Size is one octet
            if (reason != null)
                frameSize += reason.length ();
            break;
            
        default:
            System.out.printf ("E: bad message type '%d', not sent\n", id);
            assert (false);
        }
        //  Now serialize message into the frame
        ZFrame frame = new ZFrame (new byte [frameSize]);
        needle = ByteBuffer.wrap (frame.getData ()); 
        int frameFlags = 0;
        putNumber2 (0xAAA0 | 3);
        putNumber1 ((byte) id);

        switch (id) {
        case OHAI:
            putString ("FILEMQ");
            putNumber1 (FMQ_MSG_VERSION);
            break;
            
        case ORLY:
            if (mechanisms != null) {
                putNumber1 ((byte) mechanisms.size ());
                for (String value : mechanisms) {
                    putString (value);
                }
            }
            else
                putNumber1 ((byte) 0);      //  Empty string array
            frameFlags = ZMQ.SNDMORE;
            break;
            
        case YARLY:
            if (mechanism != null)
                putString (mechanism);
            else
                putNumber1 ((byte) 0);      //  Empty string
            frameFlags = ZMQ.SNDMORE;
            break;
            
        case OHAI_OK:
            break;
            
        case ICANHAZ:
            if (path != null)
                putString (path);
            else
                putNumber1 ((byte) 0);      //  Empty string
            if (options != null) {
                putNumber1 ((byte) options.size ());
                for (Map.Entry <String, String> entry: options.entrySet ()) {
                    optionsWrite (entry, this);
                }
            }
            else
                putNumber1 ((byte) 0);      //  Empty dictionary
            if (cache != null) {
                putNumber1 ((byte) cache.size ());
                for (Map.Entry <String, String> entry: cache.entrySet ()) {
                    cacheWrite (entry, this);
                }
            }
            else
                putNumber1 ((byte) 0);      //  Empty dictionary
            break;
            
        case ICANHAZ_OK:
            break;
            
        case NOM:
            putNumber8 (credit);
            putNumber8 (sequence);
            break;
            
        case CHEEZBURGER:
            putNumber8 (sequence);
            putNumber1 (operation);
            if (filename != null)
                putString (filename);
            else
                putNumber1 ((byte) 0);      //  Empty string
            putNumber8 (offset);
            putNumber1 (eof);
            if (headers != null) {
                putNumber1 ((byte) headers.size ());
                for (Map.Entry <String, String> entry: headers.entrySet ()) {
                    headersWrite (entry, this);
                }
            }
            else
                putNumber1 ((byte) 0);      //  Empty dictionary
            frameFlags = ZMQ.SNDMORE;
            break;
            
        case HUGZ:
            break;
            
        case HUGZ_OK:
            break;
            
        case KTHXBAI:
            break;
            
        case SRSLY:
            if (reason != null)
                putString (reason);
            else
                putNumber1 ((byte) 0);      //  Empty string
            break;
            
        case RTFM:
            if (reason != null)
                putString (reason);
            else
                putNumber1 ((byte) 0);      //  Empty string
            break;
            
        }
        //  If we're sending to a ROUTER, we send the address first
        if (socket.getType () == ZMQ.ROUTER) {
            assert (address != null);
            if (!address.sendAndDestroy (socket, ZMQ.SNDMORE)) {
                destroy ();
                return false;
            }
        }
        //  Now send the data frame
        if (!frame.sendAndDestroy (socket, frameFlags)) {
            frame.destroy ();
            destroy ();
            return false;
        }
        
        //  Now send any frame fields, in order
        switch (id) {
        case ORLY:
            //  If challenge isn't set, send an empty frame
            if (challenge == null)
                challenge = new ZFrame ("".getBytes ());
            if (!challenge.sendAndDestroy (socket, 0)) {
                frame.destroy ();
                destroy ();
                return false;
            }
            break;
        case YARLY:
            //  If response isn't set, send an empty frame
            if (response == null)
                response = new ZFrame ("".getBytes ());
            if (!response.sendAndDestroy (socket, 0)) {
                frame.destroy ();
                destroy ();
                return false;
            }
            break;
        case CHEEZBURGER:
            //  If chunk isn't set, send an empty frame
            if (chunk == null)
                chunk = new ZFrame ("".getBytes ());
            if (!chunk.sendAndDestroy (socket, 0)) {
                frame.destroy ();
                destroy ();
                return false;
            }
            break;
        }
        //  Destroy FmqMsg object
        destroy ();
        return true;
    }


//  --------------------------------------------------------------------------
//  Send the OHAI to the socket in one step

    public static void sendOhai (
        Socket output,
        String protocol,
        int version) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.OHAI);
        self.setProtocol (protocol);
        self.setVersion (version);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the ORLY to the socket in one step

    public static void sendOrly (
        Socket output,
        Collection <String> mechanisms,
        ZFrame challenge) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.ORLY);
        self.setMechanisms (new ArrayList <String> (mechanisms));
        self.setChallenge (challenge.duplicate ());
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the YARLY to the socket in one step

    public static void sendYarly (
        Socket output,
        String mechanism,
        ZFrame response) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.YARLY);
        self.setMechanism (mechanism);
        self.setResponse (response.duplicate ());
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the OHAI_OK to the socket in one step

    public static void sendOhai_Ok (
        Socket output) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.OHAI_OK);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the ICANHAZ to the socket in one step

    public static void sendIcanhaz (
        Socket output,
        String path,
        Map <String, String> options,
        Map <String, String> cache) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.ICANHAZ);
        self.setPath (path);
        self.setOptions (new HashMap <String, String> (options));
        self.setCache (new HashMap <String, String> (cache));
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the ICANHAZ_OK to the socket in one step

    public static void sendIcanhaz_Ok (
        Socket output) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.ICANHAZ_OK);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the NOM to the socket in one step

    public static void sendNom (
        Socket output,
        long credit,
        long sequence) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.NOM);
        self.setCredit (credit);
        self.setSequence (sequence);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the CHEEZBURGER to the socket in one step

    public static void sendCheezburger (
        Socket output,
        long sequence,
        int operation,
        String filename,
        long offset,
        int eof,
        Map <String, String> headers,
        ZFrame chunk) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.CHEEZBURGER);
        self.setSequence (sequence);
        self.setOperation (operation);
        self.setFilename (filename);
        self.setOffset (offset);
        self.setEof (eof);
        self.setHeaders (new HashMap <String, String> (headers));
        self.setChunk (chunk.duplicate ());
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the HUGZ to the socket in one step

    public static void sendHugz (
        Socket output) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.HUGZ);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the HUGZ_OK to the socket in one step

    public static void sendHugz_Ok (
        Socket output) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.HUGZ_OK);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the KTHXBAI to the socket in one step

    public static void sendKthxbai (
        Socket output) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.KTHXBAI);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the SRSLY to the socket in one step

    public static void sendSrsly (
        Socket output,
        String reason) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.SRSLY);
        self.setReason (reason);
        self.send (output); 
    }

//  --------------------------------------------------------------------------
//  Send the RTFM to the socket in one step

    public static void sendRtfm (
        Socket output,
        String reason) 
    {
        FmqMsg self = new FmqMsg (FmqMsg.RTFM);
        self.setReason (reason);
        self.send (output); 
    }


    //  --------------------------------------------------------------------------
    //  Duplicate the FmqMsg message

    public FmqMsg dup (FmqMsg self)
    {
        if (self == null)
            return null;

        FmqMsg copy = new FmqMsg (self.id);
        if (self.address != null)
            copy.address = self.address.duplicate ();
        switch (self.id) {
        case OHAI:
            copy.protocol = self.protocol;
            copy.version = self.version;
        break;
        case ORLY:
            copy.mechanisms = new ArrayList <String> (self.mechanisms);
            copy.challenge = self.challenge.duplicate ();
        break;
        case YARLY:
            copy.mechanism = self.mechanism;
            copy.response = self.response.duplicate ();
        break;
        case OHAI_OK:
        break;
        case ICANHAZ:
            copy.path = self.path;
            copy.options = new HashMap <String, String> (self.options);
            copy.cache = new HashMap <String, String> (self.cache);
        break;
        case ICANHAZ_OK:
        break;
        case NOM:
            copy.credit = self.credit;
            copy.sequence = self.sequence;
        break;
        case CHEEZBURGER:
            copy.sequence = self.sequence;
            copy.operation = self.operation;
            copy.filename = self.filename;
            copy.offset = self.offset;
            copy.eof = self.eof;
            copy.headers = new HashMap <String, String> (self.headers);
            copy.chunk = self.chunk.duplicate ();
        break;
        case HUGZ:
        break;
        case HUGZ_OK:
        break;
        case KTHXBAI:
        break;
        case SRSLY:
            copy.reason = self.reason;
        break;
        case RTFM:
            copy.reason = self.reason;
        break;
        }
        return copy;
    }

    //  Dump options key=value pair to stdout
    public static void optionsDump (Map.Entry <String, String> entry, FmqMsg self)
    {
        System.out.printf ("        %s=%s\n", entry.getKey (), entry.getValue ());
    }

    //  Dump cache key=value pair to stdout
    public static void cacheDump (Map.Entry <String, String> entry, FmqMsg self)
    {
        System.out.printf ("        %s=%s\n", entry.getKey (), entry.getValue ());
    }

    //  Dump headers key=value pair to stdout
    public static void headersDump (Map.Entry <String, String> entry, FmqMsg self)
    {
        System.out.printf ("        %s=%s\n", entry.getKey (), entry.getValue ());
    }


    //  --------------------------------------------------------------------------
    //  Print contents of message to stdout

    public void dump ()
    {
        switch (id) {
        case OHAI:
            System.out.println ("OHAI:");
            System.out.printf ("    protocol=filemq\n");
            System.out.printf ("    version=fmq_msg_version\n");
            break;
            
        case ORLY:
            System.out.println ("ORLY:");
            System.out.printf ("    mechanisms={");
            if (mechanisms != null) {
                for (String value : mechanisms) {
                    System.out.printf (" '%s'", value);
                }
            }
            System.out.printf (" }\n");
            System.out.printf ("    challenge={\n");
            if (challenge != null) {
                int size = challenge.size ();
                byte [] data = challenge.getData ();
                System.out.printf ("        size=%d\n", challenge.size ());
                if (size > 32)
                    size = 32;
                int challengeIndex;
                for (challengeIndex = 0; challengeIndex < size; challengeIndex++) {
                    if (challengeIndex != 0 && (challengeIndex % 4 == 0))
                        System.out.printf ("-");
                    System.out.printf ("%02X", data [challengeIndex]);
                }
            }
            System.out.printf ("    }\n");
            break;
            
        case YARLY:
            System.out.println ("YARLY:");
            if (mechanism != null)
                System.out.printf ("    mechanism='%s'\n", mechanism);
            else
                System.out.printf ("    mechanism=\n");
            System.out.printf ("    response={\n");
            if (response != null) {
                int size = response.size ();
                byte [] data = response.getData ();
                System.out.printf ("        size=%d\n", response.size ());
                if (size > 32)
                    size = 32;
                int responseIndex;
                for (responseIndex = 0; responseIndex < size; responseIndex++) {
                    if (responseIndex != 0 && (responseIndex % 4 == 0))
                        System.out.printf ("-");
                    System.out.printf ("%02X", data [responseIndex]);
                }
            }
            System.out.printf ("    }\n");
            break;
            
        case OHAI_OK:
            System.out.println ("OHAI_OK:");
            break;
            
        case ICANHAZ:
            System.out.println ("ICANHAZ:");
            if (path != null)
                System.out.printf ("    path='%s'\n", path);
            else
                System.out.printf ("    path=\n");
            System.out.printf ("    options={\n");
            if (options != null) {
                for (Map.Entry <String, String> entry : options.entrySet ())
                    optionsDump (entry, this);
            }
            System.out.printf ("    }\n");
            System.out.printf ("    cache={\n");
            if (cache != null) {
                for (Map.Entry <String, String> entry : cache.entrySet ())
                    cacheDump (entry, this);
            }
            System.out.printf ("    }\n");
            break;
            
        case ICANHAZ_OK:
            System.out.println ("ICANHAZ_OK:");
            break;
            
        case NOM:
            System.out.println ("NOM:");
            System.out.printf ("    credit=%d\n", (long)credit);
            System.out.printf ("    sequence=%d\n", (long)sequence);
            break;
            
        case CHEEZBURGER:
            System.out.println ("CHEEZBURGER:");
            System.out.printf ("    sequence=%d\n", (long)sequence);
            System.out.printf ("    operation=%d\n", (long)operation);
            if (filename != null)
                System.out.printf ("    filename='%s'\n", filename);
            else
                System.out.printf ("    filename=\n");
            System.out.printf ("    offset=%d\n", (long)offset);
            System.out.printf ("    eof=%d\n", (long)eof);
            System.out.printf ("    headers={\n");
            if (headers != null) {
                for (Map.Entry <String, String> entry : headers.entrySet ())
                    headersDump (entry, this);
            }
            System.out.printf ("    }\n");
            System.out.printf ("    chunk={\n");
            if (chunk != null) {
                int size = chunk.size ();
                byte [] data = chunk.getData ();
                System.out.printf ("        size=%d\n", chunk.size ());
                if (size > 32)
                    size = 32;
                int chunkIndex;
                for (chunkIndex = 0; chunkIndex < size; chunkIndex++) {
                    if (chunkIndex != 0 && (chunkIndex % 4 == 0))
                        System.out.printf ("-");
                    System.out.printf ("%02X", data [chunkIndex]);
                }
            }
            System.out.printf ("    }\n");
            break;
            
        case HUGZ:
            System.out.println ("HUGZ:");
            break;
            
        case HUGZ_OK:
            System.out.println ("HUGZ_OK:");
            break;
            
        case KTHXBAI:
            System.out.println ("KTHXBAI:");
            break;
            
        case SRSLY:
            System.out.println ("SRSLY:");
            if (reason != null)
                System.out.printf ("    reason='%s'\n", reason);
            else
                System.out.printf ("    reason=\n");
            break;
            
        case RTFM:
            System.out.println ("RTFM:");
            if (reason != null)
                System.out.printf ("    reason='%s'\n", reason);
            else
                System.out.printf ("    reason=\n");
            break;
            
        }
    }


    //  --------------------------------------------------------------------------
    //  Get/set the message address

    public ZFrame address ()
    {
        return address;
    }

    public void setAddress (ZFrame address)
    {
        if (this.address != null)
            this.address.destroy ();
        this.address = address.duplicate ();
    }


    //  --------------------------------------------------------------------------
    //  Get/set the fmq_msg id

    public int id ()
    {
        return id;
    }

    public void setId (int id)
    {
        this.id = id;
    }

    //  --------------------------------------------------------------------------
    //  Iterate through the mechanisms field, and append a mechanisms value

    public List <String> mechanisms ()
    {
        return mechanisms;
    }

    public void appendMechanisms (String format, Object ... args)
    {
        //  Format into newly allocated string
        
        String string = String.format (format, args);
        //  Attach string to list
        if (mechanisms == null)
            mechanisms = new ArrayList <String> ();
        mechanisms.add (string);
    }

    public void setMechanisms (Collection <String> value)
    {
        mechanisms = new ArrayList (value); 
    }


    //  --------------------------------------------------------------------------
    //  Get/set the challenge field

    public ZFrame challenge ()
    {
        return challenge;
    }

    //  Takes ownership of supplied frame
    public void setChallenge (ZFrame frame)
    {
        if (challenge != null)
            challenge.destroy ();
        challenge = frame;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the mechanism field

    public String mechanism ()
    {
        return mechanism;
    }

    public void setMechanism (String format, Object ... args)
    {
        //  Format into newly allocated string
        mechanism = String.format (format, args);
    }


    //  --------------------------------------------------------------------------
    //  Get/set the response field

    public ZFrame response ()
    {
        return response;
    }

    //  Takes ownership of supplied frame
    public void setResponse (ZFrame frame)
    {
        if (response != null)
            response.destroy ();
        response = frame;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the path field

    public String path ()
    {
        return path;
    }

    public void setPath (String format, Object ... args)
    {
        //  Format into newly allocated string
        path = String.format (format, args);
    }


    //  --------------------------------------------------------------------------
    //  Get/set a value in the options dictionary

    public Map <String, String> options ()
    {
        return options;
    }

    public String optionsString (String key, String defaultValue)
    {
        String value = null;
        if (options != null)
            value = options.get (key);
        if (value == null)
            value = defaultValue;

        return value;
    }

    public long optionsNumber (String key, long defaultValue)
    {
        long value = defaultValue;
        String string = null;
        if (options != null)
            string = options.get (key);
        if (string != null)
            value = Long.valueOf (string);

        return value;
    }

    public void insertOptions (String key, String format, Object ... args)
    {
        //  Format string into buffer
        String string = String.format (format, args);

        //  Store string in hash table
        if (options == null)
            options = new HashMap <String, String> ();
        options.put (key, string);
        optionsBytes += key.length () + 1 + string.length ();
    }

    public void setOptions (Map <String, String> value)
    {
        if (value != null)
            options = new HashMap <String, String> (value); 
        else
            options = value;
    }


    //  --------------------------------------------------------------------------
    //  Get/set a value in the cache dictionary

    public Map <String, String> cache ()
    {
        return cache;
    }

    public String cacheString (String key, String defaultValue)
    {
        String value = null;
        if (cache != null)
            value = cache.get (key);
        if (value == null)
            value = defaultValue;

        return value;
    }

    public long cacheNumber (String key, long defaultValue)
    {
        long value = defaultValue;
        String string = null;
        if (cache != null)
            string = cache.get (key);
        if (string != null)
            value = Long.valueOf (string);

        return value;
    }

    public void insertCache (String key, String format, Object ... args)
    {
        //  Format string into buffer
        String string = String.format (format, args);

        //  Store string in hash table
        if (cache == null)
            cache = new HashMap <String, String> ();
        cache.put (key, string);
        cacheBytes += key.length () + 1 + string.length ();
    }

    public void setCache (Map <String, String> value)
    {
        if (value != null)
            cache = new HashMap <String, String> (value); 
        else
            cache = value;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the credit field

    public long credit ()
    {
        return credit;
    }

    public void setCredit (long credit)
    {
        this.credit = credit;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the sequence field

    public long sequence ()
    {
        return sequence;
    }

    public void setSequence (long sequence)
    {
        this.sequence = sequence;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the operation field

    public int operation ()
    {
        return operation;
    }

    public void setOperation (int operation)
    {
        this.operation = operation;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the filename field

    public String filename ()
    {
        return filename;
    }

    public void setFilename (String format, Object ... args)
    {
        //  Format into newly allocated string
        filename = String.format (format, args);
    }


    //  --------------------------------------------------------------------------
    //  Get/set the offset field

    public long offset ()
    {
        return offset;
    }

    public void setOffset (long offset)
    {
        this.offset = offset;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the eof field

    public int eof ()
    {
        return eof;
    }

    public void setEof (int eof)
    {
        this.eof = eof;
    }


    //  --------------------------------------------------------------------------
    //  Get/set a value in the headers dictionary

    public Map <String, String> headers ()
    {
        return headers;
    }

    public String headersString (String key, String defaultValue)
    {
        String value = null;
        if (headers != null)
            value = headers.get (key);
        if (value == null)
            value = defaultValue;

        return value;
    }

    public long headersNumber (String key, long defaultValue)
    {
        long value = defaultValue;
        String string = null;
        if (headers != null)
            string = headers.get (key);
        if (string != null)
            value = Long.valueOf (string);

        return value;
    }

    public void insertHeaders (String key, String format, Object ... args)
    {
        //  Format string into buffer
        String string = String.format (format, args);

        //  Store string in hash table
        if (headers == null)
            headers = new HashMap <String, String> ();
        headers.put (key, string);
        headersBytes += key.length () + 1 + string.length ();
    }

    public void setHeaders (Map <String, String> value)
    {
        if (value != null)
            headers = new HashMap <String, String> (value); 
        else
            headers = value;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the chunk field

    public ZFrame chunk ()
    {
        return chunk;
    }

    //  Takes ownership of supplied frame
    public void setChunk (ZFrame frame)
    {
        if (chunk != null)
            chunk.destroy ();
        chunk = frame;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the reason field

    public String reason ()
    {
        return reason;
    }

    public void setReason (String format, Object ... args)
    {
        //  Format into newly allocated string
        reason = String.format (format, args);
    }


}

