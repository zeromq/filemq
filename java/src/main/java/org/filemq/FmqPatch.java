/*  =========================================================================
    fmq_patch - work with directory patches
    A patch is a change to the directory (create/delete/resize/retime).

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

package org.filemq;

import java.io.File;

public class FmqPatch
{
    private File path;                  //  Directory path
    private String virtual;             //  Virtual file name
    private FmqFile file;               //  File we refer to
    private OP op;                      //  Operation
    private String digest;              //  File SHA-1 digest
    
    public static enum OP {
        patch_create (1),
        patch_delete (2);
        
        @SuppressWarnings ("unused")
        private int op;
        
        private OP (int op) {
            this.op = op;
        }
    }

    //  --------------------------------------------------------------------------
    //  Constructor
    //  Create new patch, create virtual path from alias
    public FmqPatch (File path, FmqFile file, OP op, String alias)
    {
        this.path = path;
        this.file = file.dup ();
        this.op = op;
        
        //  Calculate virtual filename for patch (remove path, prefix alias)
        String filename = file.name (path.getAbsolutePath ());
        assert (!filename.startsWith ("/"));
        if (alias.endsWith ("/"))
            virtual = String.format ("%s%s", alias, filename);
        else
            virtual = String.format ("%s/%s", alias, filename);
    }
    
    //  --------------------------------------------------------------------------
    //  Destroy a patch
    public void destroy ()
    {
        file.destroy ();
    }

    //  --------------------------------------------------------------------------
    //  Create copy of a patch
    public FmqPatch dup ()
    {
        FmqPatch copy = new FmqPatch (path, file, op, "");
        copy.virtual = virtual;
        //  Don't recalculate hash when we duplicate patch
        copy.digest = digest;
        return copy;

    }

    //  --------------------------------------------------------------------------
    //  Return patch file item
    public FmqFile file ()
    {
        return file;
    }

    //  --------------------------------------------------------------------------
    //  Return operation
    public OP op ()
    {
        return op;
    }

    //  --------------------------------------------------------------------------
    //  Return patch virtual file name
    public String virtual ()
    {
        return virtual;
    }
    
    //  --------------------------------------------------------------------------
    //  Calculate hash digest for file (create only)
    public void setDigest ()
    {
        if (op == OP.patch_create
                && digest == null)
        digest = file.hash ();
    }

    //  --------------------------------------------------------------------------
    //  Return hash digest for patch file (create only)
    public String digest ()
    {
        return digest;
    }

}
