/*
 * gridfs_wrapper.h - C wrapper around the C++ GridFS API
 *
 * Copyright 2008 Paul Betts <paul.betts@gmail.com>
 *
 *
 * License:
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef _GRIDFS_WRAPPER_H
#define _GRIDFS_WRAPPER_H

#include "stdafx.h"

typedef void* mongo_conn_t;

mongo_conn_t mongodb_conn_create(const char* host, int port);
void mongodb_conn_free(mongo_conn_t conn);

#endif 
