/*
 * gridfs_wrapper.cpp - C wrapper around the C++ GridFS API
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

#include "stdafx.h"
#include "gridfs_wrapper.h"

#include <mongo/client/dbclient.h>
#include <mongo/client/gridfs.h>
#include <mongo/client/connpool.h>

using namespace mongo;

struct MongoConnection {
	DBClientConnection db;
	auto_ptr<GridFS> fs;
};

mongo_conn_t mongodb_conn_create(const char* host, int port, const char* db)
{
	gchar* conn_str;
	struct MongoConnection* ret = new MongoConnection();
	string dontcare;

	conn_str = g_strdup_printf("%s:%d", host, port);
	if (ret->db.connect( string(conn_str), dontcare)) {
		g_free(conn_str);
		delete ret;
		return NULL;
	}

	ret->fs = auto_ptr<GridFS>(new GridFS(ret->db, string(db)));
	g_free(conn_str);
	return ret;
}

void mongodb_conn_free(mongo_conn_t conn)
{
	delete conn;
}
