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
        version       octet 
        identity      octets [16]

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

    ICANHAZ_OK - Server confirms the subscription

    NOM - Client sends credit to the server
        credit        number 
        sequence      number 

    CHEEZBURGER - The server sends a file chunk
        sequence      number 
        operation     octet 
        filename      string 
        offset        number 
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
    public static final int IDENTITY_SIZE         = 16;

    //  Structure of our class
    private ZFrame address;             //  Address of peer if any
    private int id;                     //  FmqMsg message ID
    private ByteBuffer needle;          //  Read/write pointer for serialization
    private String protocol;
    private int version;
    private byte [] identity = new byte [16];
    private List <String> mechanisms;
    private int mechanismsBytes;
    private ZFrame challenge;
    private String mechanism;
    private ZFrame response;
    private String path;
    private Map <String, String> options;
    private int optionsBytes;
    private long credit;
    private long sequence;
    private int operation;
    private String filename;
    private long offset;
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

    public void
    destroy ()
    {
        //  Free class properties
        if (address != null)
            address.destroy ();
        address = null;
    }


    //  --------------------------------------------------------------------------
    //  Network data encoding macros


    //  Put an octet to the frame
    private final void putOctet (int value) 
    {
        needle.put ((byte) value);
    }

    //  Get an octet from the frame
    private int getOctet () 
    { 
        int value = needle.get (); 
        if (value < 0)
            value = (0xff) & value;
        return value;
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

    //  Put a long long integer to the frame
    public void putNumber (long value) 
    {
        needle.putLong (value);
    }

    //  Get a number from the frame
    public long getNumber () 
    {
        return needle.getLong ();
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
        int size = getOctet ();
        byte [] value = new byte [size];
        needle.get (value);

        return new String (value);
    }

    //  --------------------------------------------------------------------------
    //  Receive and parse a FmqMsg from the socket. Returns new object or
    //  null if error. Will block if there's no message waiting.

    public static FmqMsg
    recv (Socket socket)
    {
        assert (socket != null);
        FmqMsg self = new FmqMsg (0);
        ZFrame frame = null;

        try {
            //  If we're reading from a ROUTER socket, get address
            if (socket.getType () == ZMQ.ROUTER) {
                self.address = ZFrame.recvFrame (socket);
                if (self.address == null)
                    return null;         //  Interrupted
                if (!socket.hasReceiveMore ())
                    throw new IllegalArgumentException ();
            }
            //  Read and parse command in frame
            frame = ZFrame.recvFrame (socket);
            if (frame == null)
                return null;             //  Interrupted
            self.needle = ByteBuffer.wrap (frame.getData ()); 

            //  Get message id, which is first byte in frame
            self.id = self.getOctet ();
            int listSize;
            int hashSize;

            switch (self.id) {
            case OHAI:
                self.protocol = self.getString ();
                if (!self.protocol.equals( "FILEMQ"))
                    throw new IllegalArgumentException ();
                self.version = self.getOctet ();
                if (self.version != FMQ_MSG_VERSION)
                    throw new IllegalArgumentException ();
                self.identity = self.getBlock (16);
                break;

            case ORLY:
                listSize = self.getOctet ();
                self.mechanisms = new ArrayList<String> ();
                while (listSize-- > 0) {
                    String string = self.getString ();
                    self.mechanisms.add (string);
                    self.mechanismsBytes += 1 + string.length();
                }
                //  Get next frame, leave current untouched
                if (!socket.hasReceiveMore ())
                    throw new IllegalArgumentException ();
                self.challenge = ZFrame.recvFrame (socket);
                break;

            case YARLY:
                self.mechanism = self.getString ();
                //  Get next frame, leave current untouched
                if (!socket.hasReceiveMore ())
                    throw new IllegalArgumentException ();
                self.response = ZFrame.recvFrame (socket);
                break;

            case OHAI_OK:
                break;

            case ICANHAZ:
                self.path = self.getString ();
                hashSize = self.getOctet ();
                self.options = new HashMap <String, String> ();
                while (hashSize-- > 0) {
                    String string = self.getString ();
                    String [] kv = string.split("=");
                    self.options.put(kv[0], kv[1]);
                    self.optionsBytes += 1 + string.length();
                }

                break;

            case ICANHAZ_OK:
                break;

            case NOM:
                self.credit = self.getNumber ();
                self.sequence = self.getNumber ();
                break;

            case CHEEZBURGER:
                self.sequence = self.getNumber ();
                self.operation = self.getOctet ();
                self.filename = self.getString ();
                self.offset = self.getNumber ();
                hashSize = self.getOctet ();
                self.headers = new HashMap <String, String> ();
                while (hashSize-- > 0) {
                    String string = self.getString ();
                    String [] kv = string.split("=");
                    self.headers.put(kv[0], kv[1]);
                    self.headersBytes += 1 + string.length();
                }

                //  Get next frame, leave current untouched
                if (!socket.hasReceiveMore ())
                    throw new IllegalArgumentException ();
                self.chunk = ZFrame.recvFrame (socket);
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

            frame.destroy ();
            frame = null;
            return self;

        } catch (Exception e) {
            //  Error returns
            System.out.printf ("E: malformed message '%d'\n", self.id);
            return null;
        } finally {
            if (frame != null)
                frame.destroy ();
        }
    }


    //  Serialize options key=value pair
    public static void
    optionsWrite (final Map.Entry <String, String> entry, FmqMsg self)
    {
        String string = entry.getKey () + "=" + entry.getValue ();
        self.putString (string);
    }

    //  Serialize headers key=value pair
    public static void
    headersWrite (final Map.Entry <String, String> entry, FmqMsg self)
    {
        String string = entry.getKey () + "=" + entry.getValue ();
        self.putString (string);
    }


    //  --------------------------------------------------------------------------
    //  Send the FmqMsg to the socket, and destroy it

    public void
    send (Socket socket)
    {
        assert (socket != null);

        //  Calculate size of serialized data
        int frameSize = 1;
        switch (id) {
        case OHAI:
            //  protocol is a string with 1-byte length
            frameSize += 1 + "FILEMQ".length ();
            //  version is an octet
            frameSize += 1;
            //  identity is a block of 16 bytes
            frameSize += 16;
            break;
            
        case ORLY:
            //  mechanisms is an array of strings
            frameSize++;       //  Size is one octet
            if (mechanisms != null)
                frameSize += mechanisms.size () + mechanismsBytes;
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
            if (options != null)
                frameSize += options.size () + optionsBytes;
            break;
            
        case ICANHAZ_OK:
            break;
            
        case NOM:
            //  credit is an 8-byte integer
            frameSize += 8;
            //  sequence is an 8-byte integer
            frameSize += 8;
            break;
            
        case CHEEZBURGER:
            //  sequence is an 8-byte integer
            frameSize += 8;
            //  operation is an octet
            frameSize += 1;
            //  filename is a string with 1-byte length
            frameSize++;       //  Size is one octet
            if (filename != null)
                frameSize += filename.length ();
            //  offset is an 8-byte integer
            frameSize += 8;
            //  headers is an array of key=value strings
            frameSize++;       //  Size is one octet
            if (headers != null)
                frameSize += headers.size () + headersBytes;
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
            return;
        }
        //  Now serialize message into the frame
        ZFrame frame = new ZFrame (new byte [frameSize + 1]);
        needle = ByteBuffer.wrap (frame.getData ()); 
        int frameFlags = 0;
        putOctet ((byte) id);

        switch (id) {
        case OHAI:
            putString ("FILEMQ");
            putOctet ((byte) FMQ_MSG_VERSION);
            putBlock (identity, 16);
            break;
            
        case ORLY:
            if (mechanisms != null) {
                putOctet ((byte) mechanisms.size ());
                for (String value : mechanisms) {
                    putString (value);
                }
            }
            else
                putOctet ((byte) 0);      //  Empty string array
            frameFlags = ZMQ.SNDMORE;
            break;
            
        case YARLY:
            if (mechanism != null) {
                putString (mechanism);
            }
            else
                putOctet ((byte) 0);      //  Empty string
            frameFlags = ZMQ.SNDMORE;
            break;
            
        case OHAI_OK:
            break;
            
        case ICANHAZ:
            if (path != null) {
                putString (path);
            }
            else
                putOctet ((byte) 0);      //  Empty string
            if (options != null) {
                putOctet ((byte) options.size ());
                for (Map.Entry <String, String> entry: options.entrySet ()) {
                    optionsWrite (entry, this);
                }
            }
            else
                putOctet ((byte) 0);      //  Empty dictionary
            break;
            
        case ICANHAZ_OK:
            break;
            
        case NOM:
            putNumber (credit);
            putNumber (sequence);
            break;
            
        case CHEEZBURGER:
            putNumber (sequence);
            putOctet (operation);
            if (filename != null) {
                putString (filename);
            }
            else
                putOctet ((byte) 0);      //  Empty string
            putNumber (offset);
            if (headers != null) {
                putOctet ((byte) headers.size ());
                for (Map.Entry <String, String> entry: headers.entrySet ()) {
                    headersWrite (entry, this);
                }
            }
            else
                putOctet ((byte) 0);      //  Empty dictionary
            frameFlags = ZMQ.SNDMORE;
            break;
            
        case HUGZ:
            break;
            
        case HUGZ_OK:
            break;
            
        case KTHXBAI:
            break;
            
        case SRSLY:
            if (reason != null) {
                putString (reason);
            }
            else
                putOctet ((byte) 0);      //  Empty string
            break;
            
        case RTFM:
            if (reason != null) {
                putString (reason);
            }
            else
                putOctet ((byte) 0);      //  Empty string
            break;
            
        }
        //  If we're sending to a ROUTER, we send the address first
        if (socket.getType () == ZMQ.ROUTER) {
            assert (address != null);
            address.sendAndKeep (socket, ZMQ.SNDMORE);
        }
        //  Now send the data frame
        frame.sendAndKeep (socket, frameFlags);
        
        //  Now send any frame fields, in order
        switch (id) {
        case ORLY:
            //  If challenge isn't set, send an empty frame
            if (challenge == null)
                challenge = new ZFrame ((byte []) null);
            challenge.sendAndKeep (socket, 0);
            break;
        case YARLY:
            //  If response isn't set, send an empty frame
            if (response == null)
                response = new ZFrame ((byte []) null);
            response.sendAndKeep (socket, 0);
            break;
        case CHEEZBURGER:
            //  If chunk isn't set, send an empty frame
            if (chunk == null)
                chunk = new ZFrame ((byte []) null);
            chunk.sendAndKeep (socket, 0);
            break;
        }
        //  Destroy FmqMsg object
        destroy ();
    }


    //  Dump options key=value pair to stdout
    public static void
    optionsDump (Map.Entry <String, String> entry, FmqMsg self)
    {
        System.out.printf ("        %s=%s\n", entry.getKey (), entry.getValue ());
    }

    //  Dump headers key=value pair to stdout
    public static void
    headersDump (Map.Entry <String, String> entry, FmqMsg self)
    {
        System.out.printf ("        %s=%s\n", entry.getKey (), entry.getValue ());
    }


    //  --------------------------------------------------------------------------
    //  Print contents of message to stdout

    public void
    dump ()
    {
        switch (id) {
        case OHAI:
            System.out.println ("OHAI:");
            System.out.printf ("    protocol=filemq\n");
            System.out.printf ("    version=fmq_msg_version\n");
            System.out.printf ("    identity=");
            int identityIndex;
            for (identityIndex = 0; identityIndex < 16; identityIndex++) {
                if (identityIndex != 0 && (identityIndex % 4 == 0))
                    System.out.printf ("-");
                System.out.printf ("%02X", identity [identityIndex]);
            }
            System.out.printf ("\n");
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
                System.out.printf ("        size=%td\n", challenge.size ());
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
                System.out.printf ("        size=%td\n", response.size ());
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
            break;
            
        case ICANHAZ_OK:
            System.out.println ("ICANHAZ_OK:");
            break;
            
        case NOM:
            System.out.println ("NOM:");
            System.out.printf ("    credit=%ld\n", (long) credit);
            System.out.printf ("    sequence=%ld\n", (long) sequence);
            break;
            
        case CHEEZBURGER:
            System.out.println ("CHEEZBURGER:");
            System.out.printf ("    sequence=%ld\n", (long) sequence);
            System.out.printf ("    operation=%d\n", operation);
            if (filename != null)
                System.out.printf ("    filename='%s'\n", filename);
            else
                System.out.printf ("    filename=\n");
            System.out.printf ("    offset=%ld\n", (long) offset);
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
                System.out.printf ("        size=%td\n", chunk.size ());
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

    public ZFrame
    address ()
    {
        return address;
    }

    public void
    setAddress (ZFrame address)
    {
        if (this.address != null)
            this.address.destroy ();
        this.address = address;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the fmq_msg id

    public int
    id ()
    {
        return id;
    }

    public void
    setId (int id)
    {
        this.id = id;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the identity field

    public byte []
    identity ()
    {
        return identity;
    }

    public void
    setIdentity (byte [] identity)
    {
        System.arraycopy (identity, 0, this.identity, 0, 16);
    }


    //  --------------------------------------------------------------------------
    //  Iterate through the mechanisms field, and append a mechanisms value

    public List <String>
    mechanisms ()
    {
        return mechanisms;
    }

    public void
    appendMechanisms (String format, Object ... args)
    {
        //  Format into newly allocated string
        
        String string = String.format (format, args);
        //  Attach string to list
        if (mechanisms == null)
            mechanisms = new ArrayList <String> ();
        mechanisms.add (string);
        mechanismsBytes += string.length ();
    }


    //  --------------------------------------------------------------------------
    //  Get/set the challenge field

    public ZFrame 
    challenge ()
    {
        return challenge;
    }

    //  Takes ownership of supplied frame
    public void
    setChallenge (ZFrame frame)
    {
        if (challenge != null)
            challenge.destroy ();
        challenge = frame;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the mechanism field

    public String
    mechanism ()
    {
        return mechanism;
    }

    public void
    setMechanism (String format, Object ... args)
    {
        //  Format into newly allocated string
        mechanism = String.format (format, args);
    }


    //  --------------------------------------------------------------------------
    //  Get/set the response field

    public ZFrame 
    response ()
    {
        return response;
    }

    //  Takes ownership of supplied frame
    public void
    setResponse (ZFrame frame)
    {
        if (response != null)
            response.destroy ();
        response = frame;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the path field

    public String
    path ()
    {
        return path;
    }

    public void
    setPath (String format, Object ... args)
    {
        //  Format into newly allocated string
        path = String.format (format, args);
    }


    //  --------------------------------------------------------------------------
    //  Get/set a value in the options dictionary

    public Map <String, String>
    options ()
    {
        return options;
    }

    public String
    optionsString (String key, String defaultValue)
    {
        String value = null;
        if (options != null)
            value = options.get (key);
        if (value == null)
            value = defaultValue;

        return value;
    }

    public long
    optionsNumber (String key, long defaultValue)
    {
        long value = defaultValue;
        String string = null;
        if (options != null)
            string = options.get (key);
        if (string != null)
            value = Long.valueOf (string);

        return value;
    }

    public void
    insertOptions (String key, String format, Object ... args)
    {
        //  Format string into buffer
        String string = String.format (format, args);

        //  Store string in hash table
        if (options == null)
            options = new HashMap <String, String> ();
        options.put (key, string);
        optionsBytes += key.length () + 1 + string.length ();
    }


    //  --------------------------------------------------------------------------
    //  Get/set the credit field

    public long
    credit ()
    {
        return credit;
    }

    public void
    setCredit (long credit)
    {
        this.credit = credit;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the sequence field

    public long
    sequence ()
    {
        return sequence;
    }

    public void
    setSequence (long sequence)
    {
        this.sequence = sequence;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the operation field

    public int
    operation ()
    {
        return operation;
    }

    public void
    setOperation (int operation)
    {
        this.operation = operation;
    }


    //  --------------------------------------------------------------------------
    //  Get/set the filename field

    public String
    filename ()
    {
        return filename;
    }

    public void
    setFilename (String format, Object ... args)
    {
        //  Format into newly allocated string
        filename = String.format (format, args);
    }


    //  --------------------------------------------------------------------------
    //  Get/set the offset field

    public long
    offset ()
    {
        return offset;
    }

    public void
    setOffset (long offset)
    {
        this.offset = offset;
    }


    //  --------------------------------------------------------------------------
    //  Get/set a value in the headers dictionary

    public Map <String, String>
    headers ()
    {
        return headers;
    }

    public String
    headersString (String key, String defaultValue)
    {
        String value = null;
        if (headers != null)
            value = headers.get (key);
        if (value == null)
            value = defaultValue;

        return value;
    }

    public long
    headersNumber (String key, long defaultValue)
    {
        long value = defaultValue;
        String string = null;
        if (headers != null)
            string = headers.get (key);
        if (string != null)
            value = Long.valueOf (string);

        return value;
    }

    public void
    insertHeaders (String key, String format, Object ... args)
    {
        //  Format string into buffer
        String string = String.format (format, args);

        //  Store string in hash table
        if (headers == null)
            headers = new HashMap <String, String> ();
        headers.put (key, string);
        headersBytes += key.length () + 1 + string.length ();
    }


    //  --------------------------------------------------------------------------
    //  Get/set the chunk field

    public ZFrame 
    chunk ()
    {
        return chunk;
    }

    //  Takes ownership of supplied frame
    public void
    setChunk (ZFrame frame)
    {
        if (chunk != null)
            chunk.destroy ();
        chunk = frame;
    }

    //  --------------------------------------------------------------------------
    //  Get/set the reason field

    public String
    reason ()
    {
        return reason;
    }

    public void
    setReason (String format, Object ... args)
    {
        //  Format into newly allocated string
        reason = String.format (format, args);
    }


}

