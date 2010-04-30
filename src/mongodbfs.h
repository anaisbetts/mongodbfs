/*
 * mongodbfs.h - Userspace filesystem for MongoDB's GridFS
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

#ifndef _MONGODBFS_H
#define _MONGODBFS_H

#include <glib.h>

#ifndef uint
#define uint 	unsigned int
#endif

/* This object is the per-mount data we carry around with us throughout the 
 * life of the app until we release it */
struct mongodbfs_mount {
	
	/* File descriptor table */
	GHashTable* 	fd_table;
	GHashTable* 	fd_table_byname;
	gint 		next_fd_atomic;
	GStaticRWLock 	fd_table_rwlock;

	gint quitflag_atomic;
	struct WorkitemQueue* work_queue;
};

struct mongodbfs_fdentry {
	gint 		refcnt; 

	char* 		relative_path;
	uint 	 	fd;
};

void* mongodbfs_init(struct fuse_conn_info *conn);
void mongodbfs_destroy(void *mount_object_ptr);
int mongodbfs_getattr(const char *path, struct stat *stbuf);
int mongodbfs_open(const char *path, struct fuse_file_info *fi);
int mongodbfs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi);
int mongodbfs_statfs(const char *path, struct statvfs *stat);
int mongodbfs_release(const char *path, struct fuse_file_info *info);
int mongodbfs_access(const char *path, int amode);
int mongodbfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi);

#endif
