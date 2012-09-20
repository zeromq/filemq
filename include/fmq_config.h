/*  =========================================================================
    fmq_config - work with configuration files

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation -- http://www.imatix.com
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of FILEMQ, see http://filemq.org.

    This is free software; you can redistribute it and/or modify it under the
    terms of the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your option)
    any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABIL-
    ITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
    Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#ifndef __FMQ_CONFIG_H_INCLUDED__
#define __FMQ_CONFIG_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _fmq_config_t fmq_config_t;

//  Function that executes config
typedef int (fmq_config_fct) (fmq_config_t *self, void *arg, int level);

//  Create new config item
fmq_config_t *
    fmq_config_new (const char *name, fmq_config_t *parent);

//  Destroy a config item and all its children
void
    fmq_config_destroy (fmq_config_t **self_p);

//  Return name of config item
char *
    fmq_config_name (fmq_config_t *self);

//  Set config item name, name may be NULL
void
    fmq_config_name_set (fmq_config_t *self, const char *name);

//  Return value of config item
char *
    fmq_config_value (fmq_config_t *self);
    
//  Set value of config item
void
    fmq_config_value_set (fmq_config_t *self, const char *value);
    
//  Set value of config item via printf format
char *
    fmq_config_value_format (fmq_config_t *self, const char *format, ...);

//  Find the first child of a config item, if any
fmq_config_t *
    fmq_config_child (fmq_config_t *self);

//  Find the next sibling of a config item, if any
fmq_config_t *
    fmq_config_next (fmq_config_t *self);

//  Find a config item along a path
fmq_config_t *
    fmq_config_locate (fmq_config_t *self, const char *path);

//  Resolve a configuration path into a string value
char *
    fmq_config_resolve (fmq_config_t *self,
                        const char *path, const char *default_value);

//  Locate the last config item at a specified depth
fmq_config_t *
    fmq_config_at_depth (fmq_config_t *self, int level);

//  Execute a callback for each config item in the tree
int
    fmq_config_execute (fmq_config_t *self, fmq_config_fct handler, void *arg);

//  Load a config item tree from a ZPL text file
fmq_config_t *
    fmq_config_load (const char *filename);

//  Dump the config file to stdout for tracing
void
    fmq_config_dump (fmq_config_t *self);

//  Self test of this class
int
    fmq_config_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif

