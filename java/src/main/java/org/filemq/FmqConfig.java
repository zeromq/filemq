package org.filemq;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class FmqConfig
{
    
    private String name;                 //  Property name if any
    private String value;                //  Property value, if any
    @SuppressWarnings ("unused")
    private FmqConfig
        child,                 //  First child if any
        next,                  //  Next sibling if any
        parent;                //  Parent if any

    //  --------------------------------------------------------------------------
    //  Constructor
    //
    //  Optionally attach new config to parent config, as first or next child.
    public FmqConfig (final String name, FmqConfig parent)
    {
        setName (name);
        if (parent != null) {
            if (parent.child != null) {
                //  Attach as last child of parent
                FmqConfig last = parent.child;
                while (last.next != null)
                    last = last.next;
                last.next = this;
            }
            else
                //  Attach as first child of parent
                parent.child = this;
        }
        this.parent = parent;
    }

    //  --------------------------------------------------------------------------
    //  Destructor
    public void destroy ()
    {
        //  Destroy all children and siblings recursively
        if (child != null)
            child.destroy ();
        if (next != null)
            next.destroy ();
    }

    //  --------------------------------------------------------------------------
    //  Return name of config item
    public String name ()
    {
        return name;
    }
    
    //  --------------------------------------------------------------------------
    //  Set config item name, name may be NULL
    public void setName (final String name)
    {
        this.name = name;
    }

    //  --------------------------------------------------------------------------
    //  Return value of config item
    public String value ()
    {
        return value;
    }
    
    //  --------------------------------------------------------------------------
    //  Set value of config item
    public void setValue (final String value)
    {
        this.value = value;
    }

    //  --------------------------------------------------------------------------
    //  Set value of config item via printf format
    public void formatValue (final String format, Object ... args)
    {
        this.value = String.format (format, args);
    }

    //  --------------------------------------------------------------------------
    //  Find the first child of a config item, if any
    public FmqConfig child ()
    {
        return child;
    }
    
    //  --------------------------------------------------------------------------
    //  Find the next sibling of a config item, if any
    public FmqConfig next ()
    {
        return next;
    }


    //  --------------------------------------------------------------------------
    //  Find a config item along a path
    public FmqConfig locate (final String path)
    {
        //  Check length of next path segment
        int slash = path.indexOf ('/');
        int length = path.length ();
        if (slash >= 0)
            length = slash;
        
        String base = path.substring (0, length);
        //  Find matching name starting at first child of root
        FmqConfig child = this.child;
        while (child != null) {
            if (child.name.equals (base)) {
                if (slash > 0)          //  Look deeper
                    return child.locate (path.substring (slash + 1));
                else
                    return child;
            }
            child = child.next;
        }
        return null;
    }
    
    //  --------------------------------------------------------------------------
    //  Resolve a configuration path into a string value
    public String resolve (final String path, final String defaultValue)
    {
        FmqConfig item = locate (path);
        if (item != null)
            return item.value ();
        else
            return defaultValue;
    }

    public void setPath (final String path, final String value)
    {
        //  Check length of next path segment
        int slash = path.indexOf ('/');
        int length = path.length ();
        if (slash >= 0)
            length = slash;

        //  Find or create items starting at first child of root
        FmqConfig child_ = this.child;
        while (child_ != null) {
            if (child_.name.startsWith (path.substring (0, length))) {
                //  This segment exists
                if (slash >= 0)          //  Recurse to next level
                    child_.setPath (path.substring (slash + 1), value);
                else
                    child_.setValue (value);
                return;
            }
            child_ = child_.next;
        }
        //  This segment doesn't exist, create it
        child_ = new FmqConfig (path.substring (0, length), this);
        if (slash >= 0)                  //  Recurse down further
            child_.setPath (path.substring (slash + 1), value);
        else
            child_.setValue (value);
    }

    //  --------------------------------------------------------------------------
    //  Finds the latest node at the specified depth, where 0 is the root. If no
    //  such node exists, returns NULL.
    public FmqConfig depthAt (int level)
    {
        FmqConfig self = this;
        
        while (level > 0) {
            if (self.child != null) {
                self = self.child;
                while (self.next != null)
                    self = self.next;
                level--;
            }
            else
                return null;
        }
        return self;
    }

    //  --------------------------------------------------------------------------
    //  Load a config item tree from a ZPL file (http://rfc.zeromq.org/spec:4/)
    //
    //  Here is an example ZPL stream and corresponding config structure:
    //
    //  context
    //      iothreads = 1
    //      verbose = 1      #   Ask for a trace
    //  main
    //      type = zqueue    #  ZMQ_DEVICE type
    //      frontend
    //          option
    //              hwm = 1000
    //              swap = 25000000     #  25MB
    //          bind = 'inproc://addr1'
    //          bind = 'ipc://addr2'
    //      backend
    //          bind = inproc://addr3
    //
    //  root                    Down = child
    //    |                     Across = next
    //    v
    //  context-->main
    //    |         |
    //    |         v
    //    |       type=queue-->frontend-->backend
    //    |                      |          |
    //    |                      |          v
    //    |                      |        bind=inproc://addr3
    //    |                      v
    //    |                    option-->bind=inproc://addr1-->bind=ipc://addr2
    //    |                      |
    //    |                      v
    //    |                    hwm=1000-->swap=25000000
    //    v
    //  iothreads=1-->verbose=false


    /*  =========================================================================
      ZPL parser functions
      =========================================================================*/

    private static char charAt (String value, int at)
    {
        if (at >= 0 && at < value.length ())
            return value.charAt (at);
        return 0;
    }
    //  Count and verify indentation level, -1 means a syntax error
    //
    private static int collectLevel (String [] pstart, int lineno)
    {
        int pos = 0;
        String start = pstart [0];
        char readptr = charAt (start, pos);

        while (readptr == ' ')
            readptr = charAt (start, ++pos);
        
        int level = pos / 4;
        if (level * 4 != pos) {
            System.err.printf ("E: (%d) indent 4 spaces at once\n", lineno);
            level = -1;
        }
        
        pstart [0] = start.substring (pos);
        
        return level;
    }
    
    //  Collect property name
    //
    private static String collectName (String [] pstart, int lineno)
    {
        int pos = 0;
        String start = pstart [0];
        char readptr = charAt (start, pos);

        while (Character.isDigit (readptr) || Character.isLetter (readptr) || readptr == '/')
            readptr = charAt (start, ++pos);

        String name = start.substring (0, pos);
        pstart [0] = start.substring (pos);
        
        if (pos > 0
        && (name.startsWith ("/") || name.endsWith ("/"))) {
            System.err.printf ("E: (%d) '/' not valid at name start or end\n", lineno);
        }
        return name;
    }
    
    //  Checks there's no junk after value on line, returns 0 if OK else -1.
    //
    private static int verifyEoln (String start, int lineno)
    {
        int pos = 0;
        char readptr = charAt (start, pos);
        
        while (readptr > 0) {
            if (Character.isWhitespace (readptr))
                readptr = charAt (start, ++pos);
            else
            if (readptr == '#')
                break;
            else {
                System.err.printf ("E: (%d) invalid syntax '%s'\n",
                    lineno, start.substring (pos));
                return -1;
            }
        }
        return 0;
    }
    
    //  Returns value for name, or "" - if syntax error, returns NULL.
    //
    private static String collectValue (String [] pstart, int lineno)
    {
        int pos = 0;

        String value = null;
        String start = pstart [0];
        char readptr = charAt (start, pos);
        int rc = 0;

        while (Character.isWhitespace (readptr))
            readptr = charAt (start, ++pos);

        if (readptr == '=') {
            readptr = charAt (start, ++pos);
            while (Character.isWhitespace (readptr))
                readptr = charAt (start, ++pos);;

            //  If value starts with quote or apost, collect it
            if (readptr == '"' || readptr == '\'') {
                int endquote = start.indexOf (readptr, pos+1);
                if (endquote >= 0) {
                    int valueLength = endquote - pos - 1;
                    value = start.substring (pos + 1, valueLength);
                    rc = verifyEoln (start.substring (endquote + 1), lineno);
                }
                else {
                    System.err.printf ("E: (%d) missing %c\n", lineno, readptr);
                    rc = -1;
                }
            }
            else {
                //  Collect unquoted value up to comment
                int comment = start.indexOf ('#', pos);
                if (comment >= 0) {
                    while (Character.isWhitespace (start.charAt (comment -1)))
                        comment--;
                    value = start.substring (pos, comment);
                } else
                    value = start.substring (pos);
            }
        }
        else {
            value = "";
            rc = verifyEoln (start.substring (pos), lineno);
        }
        //  If we had an error, drop value and return NULL
        if (rc < 0)
            value = null;
        return value;
    }
    
    public static FmqConfig load (String configFile)
    {
        File file = new File (configFile);
        if (!file.exists ())
            return null;            //  File not found, or unreadable

        //  Prepare new fmq_config_t structure
        FmqConfig self = new FmqConfig ("root", null);

        //  Parse the file line by line
        String curLine;
        boolean valid = true;
        int lineno = 0;

        BufferedReader in = null;
        
        try {
            in = new BufferedReader (new FileReader (file));
            while ((curLine = in.readLine ()) != null) {
                //  Trim line
                int length = curLine.length ();
                while (Character.isWhitespace (charAt (curLine, length - 1)))
                    --length;
                curLine = curLine.substring (0, length);
    
                //  Collect indentation level and name, if any
                lineno++;
                String [] scanner = { curLine };
                int level = collectLevel (scanner, lineno);
                if (level == -1) {
                    valid = false;
                    break;
                }
    
                String name = collectName (scanner, lineno);
                if (name == null) {
                    valid = false;
                    break;
                }
                //  If name is not empty, collect property value
                if (!name.isEmpty ()) {
                    String value = collectValue (scanner, lineno);
                    if (value == null)
                        valid = false;
                    else {
                        //  Navigate to parent for this element
                        FmqConfig parent = self.depthAt (level);
                        if (parent != null) {
                            FmqConfig item = new FmqConfig (name, parent);
                            item.value = value;
                        }
                        else {
                            System.err.printf ("E: (%d) indentation error\n", lineno);
                            valid = false;
                        }
                    }
                }
                else
                if (verifyEoln (scanner [0], lineno) < 0)
                    valid = false;
                if (!valid)
                    break;
            }
        } catch (IOException e) {
            valid = false;
        } finally {
            try {
                in.close ();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        //  Either the whole ZPL file is valid or none of it is
        if (!valid) {
            self.destroy ();
            self = null;
        }

        return self;
    }

}
