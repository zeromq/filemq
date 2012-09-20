/*  =========================================================================
    fmq_sasl - work with SASL mechanisms

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

#ifndef __FMQ_SASL_H_INCLUDED__
#define __FMQ_SASL_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Encode login and password as PLAIN response
zframe_t *
    fmq_sasl_plain_encode (const char *login, const char *password);

//  Decode PLAIN response into new login and password
int
    fmq_sasl_plain_decode (zframe_t *frame, char **login, char **password);

//  Self test of this class
int
    fmq_sasl_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
