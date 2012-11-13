/*  =========================================================================
    fmq.h - FileMQ library

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

#ifndef __FMQ_H_INCLUDED__
#define __FMQ_H_INCLUDED__

//  FMQ version macros for compile-time API detection

#define FMQ_VERSION_MAJOR 1
#define FMQ_VERSION_MINOR 0
#define FMQ_VERSION_PATCH 0

#define FMQ_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define FMQ_VERSION \
    FMQ_MAKE_VERSION(FMQ_VERSION_MAJOR, FMQ_VERSION_MINOR, FMQ_VERSION_PATCH)

//  These are reusable utility classes
//  TODO: the generators and required classes should not be part of FMQ
//  as such, but a separate project so they can be reused more widely.
#include "fmq_chunk.h"
#include "fmq_file.h"
#include "fmq_dir.h"
#include "fmq_patch.h"
#include "fmq_sasl.h"
#include "fmq_sha.h"
#include "fmq_config.h"

//  These are specific to the FileMQ implementation
#include "fmq_msg.h"
#include "fmq_client.h"
#include "fmq_server.h"

#endif
