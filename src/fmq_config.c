/*  =========================================================================
    fmq_config - work with configuration files

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
#include "../include/fmq_config.h"

//  Structure of our class

struct _fmq_config_t {
    char *name;                 //  Property name if any
    char *value;                //  Property value, if any
    struct _fmq_config_t
        *child,                 //  First child if any
        *next,                  //  Next sibling if any
        *parent;                //  Parent if any
};


//  --------------------------------------------------------------------------
//  Constructor
//
//  Optionally attach new config to parent config, as first or next child.

fmq_config_t *
fmq_config_new (const char *name, fmq_config_t *parent)
{
    fmq_config_t
        *self;

    self = (fmq_config_t *) zmalloc (sizeof (fmq_config_t));
    fmq_config_name_set (self, name);
    if (parent) {
        if (parent->child) {
            //  Attach as last child of parent
            fmq_config_t *last = parent->child;
            while (last->next)
                last = last->next;
            last->next = self;
        }
        else
            //  Attach as first child of parent
            parent->child = self;
    }
    self->parent = parent;
    return self;
}


//  --------------------------------------------------------------------------
//  Destructor

void
fmq_config_destroy (fmq_config_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fmq_config_t *self = *self_p;

        //  Destroy all children and siblings recursively
        if (self->child)
            fmq_config_destroy (&self->child);
        if (self->next)
            fmq_config_destroy (&self->next);

        free (self->name);
        free (self->value);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Return name of config item

char *
fmq_config_name (fmq_config_t *self)
{
    assert (self);
    return self->name;
}


//  --------------------------------------------------------------------------
//  Set config item name, name may be NULL

void
fmq_config_name_set (fmq_config_t *self, const char *name)
{
    assert (self);
    free (self->name);
    self->name = strdup (name);
}


//  --------------------------------------------------------------------------
//  Return value of config item

char *
fmq_config_value (fmq_config_t *self)
{
    assert (self);
    return self->value;
}


//  --------------------------------------------------------------------------
//  Set value of config item

void
fmq_config_value_set (fmq_config_t *self, const char *value)
{
    assert (self);
    free (self->value);
    self->value = strdup (value);
}


//  --------------------------------------------------------------------------
//  Set value of config item via printf format

char *
fmq_config_value_format (fmq_config_t *self, const char *format, ...)
{
    char *value = malloc (255 + 1);
    va_list args;
    assert (self);
    va_start (args, format);
    vsnprintf (value, 255, format, args);
    va_end (args);
    
    free (self->value);
    self->value = value;
    return 0;
}


//  --------------------------------------------------------------------------
//  Find the first child of a config item, if any

fmq_config_t *
fmq_config_child (fmq_config_t *self)
{
    assert (self);
    return self->child;
}


//  --------------------------------------------------------------------------
//  Find the next sibling of a config item, if any

fmq_config_t *
fmq_config_next (fmq_config_t *self)
{
    assert (self);
    return self->next;
}


//  --------------------------------------------------------------------------
//  Find a config item along a path

fmq_config_t *
fmq_config_locate (fmq_config_t *self, const char *path)
{
    //  Calculate significant length of name
    char *slash = strchr (path, '/');
    int length = strlen (path);
    if (slash)
        length = slash - path;

    //  Find matching name starting at first child of root
    fmq_config_t *child = self->child;
    while (child) {
        if (strlen (child->name) == length
        &&  memcmp (child->name, path, length) == 0) {
            if (slash)          //  Look deeper
                return fmq_config_locate (child, slash + 1);
            else
                return child;
        }
        child = child->next;
    }
    return NULL;
}


//  --------------------------------------------------------------------------
//  Resolve a configuration path into a string value

char *
fmq_config_resolve (fmq_config_t *self, const char *path, const char *default_value)
{
    fmq_config_t *item = fmq_config_locate (self, path);
    if (item)
        return fmq_config_value (item);
    else
        return (char *) default_value;
}


//  --------------------------------------------------------------------------
//  Finds the latest node at the specified depth, where 0 is the root. If no
//  such node exists, returns NULL.

fmq_config_t *
fmq_config_at_depth (fmq_config_t *self, int level)
{
    while (level > 0) {
        if (self->child) {
            self = self->child;
            while (self->next)
                self = self->next;
            level--;
        }
        else
            return NULL;
    }
    return self;
}


//  --------------------------------------------------------------------------
//  Execute a callback for each config item in the tree

static int
s_config_execute (fmq_config_t *self, fmq_config_fct handler, void *arg, int level)
{
    assert (self);
    int rc = handler (self, arg, level);

    //  Process all children in one go, as a list
    fmq_config_t *child = self->child;
    while (child && !rc) {
        rc = s_config_execute (child, handler, arg, level + 1);
        if (rc == -1)
            break;              //  -1 from callback means end execution
        child = child->next;
    }
    return rc;
}

int
fmq_config_execute (fmq_config_t *self, fmq_config_fct handler, void *arg)
{
    //  Execute top level config at level zero
    assert (self);
    return s_config_execute (self, handler, arg, 0);
}


//  --------------------------------------------------------------------------
//  Dump the config file to stdout for tracing
void
fmq_config_dump (fmq_config_t *self)
{
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

//  Count and verify indentation level, -1 means a syntax error
//
static int
s_collect_level (char **start, int lineno)
{
    char *readptr = *start;
    while (*readptr == ' ')
        readptr++;
    int level = (readptr - *start) / 4;
    if (level * 4 != readptr - *start) {
        fprintf (stderr, "E: (%d) indent 4 spaces at once\n", lineno);
        level = -1;
    }
    *start = readptr;
    return level;
}

//  Collect property name
//
static char *
s_collect_name (char **start, int lineno)
{
    char *readptr = *start;
    while (isalnum ((byte) **start) || (byte) **start == '/')
        (*start)++;

    size_t length = *start - readptr;
    char *name = (char *) zmalloc (length + 1);
    memcpy (name, readptr, length);
    name [length] = 0;
    
    if (length > 0
    && (name [0] == '/' || name [length - 1] == '/')) {
        fprintf (stderr, "E: (%d) '/' not valid at name start or end\n", lineno);
        free (name);
    }
    return name;
}


//  Checks there's no junk after value on line, returns 0 if OK else -1.
//
static int
s_verify_eoln (char *readptr, int lineno)
{
    while (*readptr) {
        if (isspace ((byte) *readptr))
            readptr++;
        else
        if (*readptr == '#')
            break;
        else {
            fprintf (stderr, "E: (%d) invalid syntax '%s'\n",
                lineno, readptr);
            return -1;
            break;
        }
    }
    return 0;
}


//  Returns value for name, or "" - if syntax error, returns NULL.
//
static char *
s_collect_value (char **start, int lineno)
{
    char *value = NULL;
    char *readptr = *start;
    int rc = 0;

    while (isspace ((byte) *readptr))
        readptr++;

    if (*readptr == '=') {
        readptr++;
        while (isspace ((byte) *readptr))
            readptr++;

        //  If value starts with quote or apost, collect it
        if (*readptr == '"' || *readptr == '\'') {
            char *endquote = strchr (readptr + 1, *readptr);
            if (endquote) {
                size_t value_length = endquote - readptr - 1;
                value = (char *) zmalloc (value_length + 1);
                memcpy (value, readptr + 1, value_length);
                value [value_length] = 0;
                rc = s_verify_eoln (endquote + 1, lineno);
            }
            else {
                fprintf (stderr, "E: (%d) missing %c\n", lineno, *readptr);
                rc = -1;
            }
        }
        else {
            //  Collect unquoted value up to comment
            char *comment = strchr (readptr, '#');
            if (comment) {
                while (isspace ((byte) comment [-1]))
                    comment--;
                *comment = 0;
            }
            value = strdup (readptr);
        }
    }
    else {
        value = strdup ("");
        rc = s_verify_eoln (readptr, lineno);
    }
    //  If we had an error, drop value and return NULL
    if (rc)
        free (value);
    return value;
}


fmq_config_t *
fmq_config_load (const char *filename)
{
    FILE *file = fopen (filename, "r");
    if (!file)
        return NULL;        //  File not found, or unreadable

    //  Prepare new fmq_config_t structure
    fmq_config_t *self = fmq_config_new ("root", NULL);
    
    //  Parse the file line by line
    char cur_line [1024];
    bool valid = true;
    int lineno = 0;
    
    while (fgets (cur_line, 1024, file)) {
        //  Trim line
        int length = strlen (cur_line);
        while (isspace ((byte) cur_line [length - 1]))
            cur_line [--length] = 0;

        //  Collect indentation level and name, if any
        lineno++;
        char *scanner = cur_line;
        int level = s_collect_level (&scanner, lineno);
        if (level == -1) {
            valid = false;
            break;
        }
        char *name = s_collect_name (&scanner, lineno);
        if (name == NULL) {
            valid = false;
            break;
        }
        //  If name is not empty, collect property value
        if (*name) {
            char *value = s_collect_value (&scanner, lineno);
            if (value == NULL)
                valid = false;
            else {
                //  Navigate to parent for this element
                fmq_config_t *parent = fmq_config_at_depth (self, level);
                if (parent) {
                    fmq_config_t *item = fmq_config_new (name, parent);
                    item->value = value;
                }
                else {
                    fprintf (stderr, "E: (%d) indentation error\n", lineno);
                    free (value);
                    valid = false;
                }
            }
        }
        else
        if (s_verify_eoln (scanner, lineno))
            valid = false;
        free (name);
        if (!valid)
            break;
    }
    //  Either the whole ZPL file is valid or none of it is
    if (!valid)
        fmq_config_destroy (&self);
    
    return self;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_config_test (bool verbose)
{
    printf (" * fmq_config: ");

    //  @selftest
    //  We create a config of this structure:
    //
    //  root
    //      type = zqueue
    //      frontend
    //          option
    //              swap = 25000000     #  25MB
    //              subscribe = #2
    //              hwm = 1000
    //          bind = tcp://*:5555
    //      backend
    //          bind = tcp://*:5556
    //
    fmq_config_t
        *root,
        *type,
        *frontend,
        *option,
        *hwm,
        *swap,
        *subscribe,
        *bind,
        *backend;

    //  Left is first child, next is next sibling
    root     = fmq_config_new ("root", NULL);
    type     = fmq_config_new ("type", root);
    fmq_config_value_set (type, "zqueue");
    frontend = fmq_config_new ("frontend", root);
    option   = fmq_config_new ("option", frontend);
    swap     = fmq_config_new ("swap", option);
    fmq_config_value_set (swap, "25000000");
    subscribe = fmq_config_new ("subscribe", option);
    fmq_config_value_format (subscribe, "#%d", 2);
    hwm      = fmq_config_new ("hwm", option);
    fmq_config_value_set (hwm, "1000");
    bind     = fmq_config_new ("bind", frontend);
    fmq_config_value_set (bind, "tcp://*:5555");
    backend  = fmq_config_new ("backend", root);
    bind     = fmq_config_new ("bind", backend);
    fmq_config_value_set (bind, "tcp://*:5556");

    assert (atoi (fmq_config_resolve (root, "frontend/option/hwm", "0")) == 1000);
    assert (streq (fmq_config_resolve (root, "backend/bind", ""), "tcp://*:5556"));

    fmq_config_destroy (&root);
    assert (root == NULL);

    //  Test loading from a ZPL file
    fmq_config_t *config = fmq_config_load ("anonymous.cfg");
    assert (config);

    //  Destructor should be safe to call twice
    fmq_config_destroy (&config);
    fmq_config_destroy (&config);
    assert (config == NULL);
    //  @end

    printf ("OK\n");
    return 0;
}
