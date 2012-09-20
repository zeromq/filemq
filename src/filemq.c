/*  =========================================================================
    filemq - FileMQ command-line service

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation <www.imatix.com>
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
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#include "czmq.h"
#include "../include/fmq.h"

#define PRODUCT         "FileMQ service/0.1a0"
#define COPYRIGHT       "Copyright (c) 2012 iMatix Corporation"
#define NOWARRANTY \
"This is free software; see the source for copying conditions.  There is NO\n" \
"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n" 

int main (int argc, char *argv [])
{
    puts (PRODUCT);
    puts (COPYRIGHT);
    puts (NOWARRANTY);
    
    //  Run selftest using 'anonymous.cfg' configuration
    fmq_server_t *self = fmq_server_new ();
    assert (self);
    puts ("Accepting FILEMQ connections on port 6000");
    fmq_server_bind (self, "tcp://*:6000");
    fmq_server_configure (self, "anonymous.cfg");

    while (!zctx_interrupted)
        sleep (1);

    fmq_server_destroy (&self);
    return 0;
}
