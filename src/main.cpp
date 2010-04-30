/*
 * main.cpp - Userspace filesystem for MongoDB's GridFS
 *
 * Copyright 2010 Paul Betts <paul.betts@gmail.com>
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

#include <fuse.h>
#include <glib.h>

extern "C" {
#include "stdafx.h"
#include "mongodbfs.h"
}

static struct fuse_operations mongodbfs_oper;

int main(int argc, char *argv[])
{
	// NB: C++ is so full of fail
	mongodbfs_oper.getattr		= mongodbfs_getattr;
	/*mongodbfs_oper.readlink 	= mongodbfs_readlink, */
	mongodbfs_oper.open 		= mongodbfs_open;
	mongodbfs_oper.read		= mongodbfs_read;
	mongodbfs_oper.statfs 		= mongodbfs_statfs;
	/* TODO: do we need flush? */
	mongodbfs_oper.release 		= mongodbfs_release;
	mongodbfs_oper.init 		= mongodbfs_init;
	mongodbfs_oper.destroy 		= mongodbfs_destroy;
	mongodbfs_oper.access 		= mongodbfs_access;

	/* TODO: implement these later
	mongodbfs_oper.getxattr 	= mongodbfs_getxattr,
	mongodbfs_oper.listxattr 	= mongodbfs_listxattr,
	mongodbfs_oper.opendir 		= mongodbfs_opendir, */
	mongodbfs_oper.readdir		= mongodbfs_readdir;
	/*mongodbfs_oper.releasedir 	= mongodbfs_releasedir,
	mongodbfs_oper.fsyncdir 	= mongodbfs_fsyncdir, */

	return fuse_main(argc, argv, &mongodbfs_oper, NULL);
}
