/*  =========================================================================
    fmq_sasl - work with SASL mechanisms

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
#include "../include/fmq_sasl.h"


//  --------------------------------------------------------------------------
//  Encode login and password as PLAIN response
//  
//  Formats a new authentication challenge for a PLAIN login. The protocol
//  uses a SASL-style binary data block for authentication. This method is
//  a simple way of turning a name and password into such a block. Note
//  that plain authentication data is not encrypted and does not provide
//  any degree of confidentiality. The SASL PLAIN mechanism is defined here:
//  http://www.ietf.org/internet-drafts/draft-ietf-sasl-plain-08.txt.

zframe_t *
fmq_sasl_plain_encode (const char *login, const char *password)
{
    //  PLAIN format is [null]login[null]password
    zframe_t *frame = zframe_new (NULL, strlen (login) + strlen (password) + 2);
    byte *data = zframe_data (frame);
    *data++ = 0;
    memcpy (data, login, strlen (login));
    data += strlen (login);
    *data++ = 0;
    memcpy (data, password, strlen (password));
    return frame;
}


//  --------------------------------------------------------------------------
//  Decode PLAIN response into new login and password

int
fmq_sasl_plain_decode (zframe_t *frame, char **login, char **password)
{
    int remains = zframe_size (frame);
    byte *data = zframe_data (frame);
    if (remains < 2 || *data != 0)
        return -1;
    
    data++;
    remains--;
    byte *zero = memchr (data, 0, remains);
    if (zero) {
        *login = malloc (zero - data + 1);
        strcpy (*login, (char *) data);
        data += (strlen (*login) + 1);
        remains -= (strlen (*login) + 1);
        *password = malloc (remains + 1);
        memcpy (*password, data, remains);
        (*password) [remains] = 0;
        return 0;
    }
    else
        return -1;
}


//  --------------------------------------------------------------------------
//  Self test of this class
int
fmq_sasl_test (bool verbose)
{
    printf (" * fmq_sasl: ");

    zframe_t *frame = fmq_sasl_plain_encode ("Happy", "Harry");
    char *login, *password;
    int rc = fmq_sasl_plain_decode (frame, &login, &password);
    assert (rc == 0);
    assert (streq (login, "Happy"));
    assert (streq (password, "Harry"));
    zframe_destroy (&frame);
    free (login);
    free (password);

    printf ("OK\n");
    return 0;
}
