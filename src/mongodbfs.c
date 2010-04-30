/*
 * mongodbfs.c - Userspace filesystem for MongoDB's GridFS
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

#include <fuse.h>
#include <glib.h>

#include "stdafx.h"
#include "mongodbfs.h"
#include "stats.h"
#include "queue.h"

/* Globals */
GIOChannel* stats_file = NULL;


/* 
 * Utility Routines 
 */

static struct mongodbfs_mount* get_current_mountinfo(void)
{
	/* NOTE: This function *only* works inside a FUSE callout */
	struct fuse_context* ctx = fuse_get_context();
	return (ctx ? ctx->private_data : NULL);
}

static struct mongodbfs_fdentry* fdentry_new(void)
{
	struct mongodbfs_fdentry* ret = g_new0(struct mongodbfs_fdentry, 1);
	ret->refcnt = 1;
	return ret;
}

static struct mongodbfs_fdentry* fdentry_ref(struct mongodbfs_fdentry* obj)
{
	g_atomic_int_inc(&obj->refcnt);
	return obj;
}

static void fdentry_unref(struct mongodbfs_fdentry* obj)
{
	if(g_atomic_int_dec_and_test(&obj->refcnt)) {
		g_free(obj->relative_path);
		g_free(obj);
	}
}

static struct mongodbfs_fdentry* fdentry_from_fd(uint fd)
{
	struct mongodbfs_fdentry* ret = NULL;
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();

	g_static_rw_lock_reader_lock(&mount_obj->fd_table_rwlock);
	ret = g_hash_table_lookup(mount_obj->fd_table, &fd);
	g_static_rw_lock_reader_unlock(&mount_obj->fd_table_rwlock);

	return (ret ? fdentry_ref(ret) : NULL);
}

static void insert_fdtable_entry(struct mongodbfs_mount* mount_obj, struct mongodbfs_fdentry* fde)
{
	g_hash_table_insert(mount_obj->fd_table, &fde->fd, fde);

	GSList* iter;
	if ( !(iter = g_hash_table_lookup(mount_obj->fd_table_byname, fde->relative_path)) ) {
		iter = g_slist_prepend(iter, fde);
		g_hash_table_insert(mount_obj->fd_table_byname, fde->relative_path, iter);
	} else {
		iter = g_slist_prepend(iter, fde);
		g_hash_table_replace(mount_obj->fd_table_byname, fde->relative_path, iter);
	}
}

static gpointer force_terminate_on_ioblock(gpointer dontcare)
{
	sleep(15);
	//kill(0, SIGKILL);
	return 0;
}

static void trash_fdtable_byname_item(gpointer key, gpointer val, gpointer dontcare) 
{ 
	GSList* fde_list = val;
	if (fde_list)
		g_slist_free(fde_list);
}

static void trash_fdtable_item(gpointer key, gpointer val, gpointer dontcare) 
{ 
	fdentry_unref((struct mongodbfs_fdentry*)val);
}



/*
 * FUSE callouts
 */

void* mongodbfs_init(struct fuse_conn_info *conn)
{
	struct mongodbfs_mount* mount_object = g_new0(struct mongodbfs_mount, 1);

	g_thread_init(NULL);

	stats_file = stats_open_logging();

	/* Create the file descriptor tables */
	mount_object->fd_table = g_hash_table_new(g_int_hash, g_int_equal);
	mount_object->fd_table_byname = g_hash_table_new(g_str_hash, g_str_equal);
	g_static_rw_lock_init(&mount_object->fd_table_rwlock);
	mount_object->next_fd = 4;

	mount_object->work_queue = workitem_queue_new();

	return mount_object;
}

void mongodbfs_destroy(void *mount_object_ptr)
{
	struct mongodbfs_mount* mount_object = mount_object_ptr;

	/* Kick off a watchdog thread; this is our last chance to bail; while
	 * Mac and Linux both support async IO for reads/writes, if a remote FS
	 * wanders off on an access or readdir call, there's absolutely zilch
	 * that we can do about it, except for force kill everyone involved. */
	g_thread_create(force_terminate_on_ioblock, NULL, FALSE, NULL);

	workitem_queue_free(mount_object->work_queue);

	/* XXX: We need to make sure no one is using this before we trash it */
	g_hash_table_foreach(mount_object->fd_table, trash_fdtable_item, NULL);
	g_hash_table_foreach(mount_object->fd_table_byname, trash_fdtable_byname_item, NULL);
	g_hash_table_destroy(mount_object->fd_table);
	g_hash_table_destroy(mount_object->fd_table_byname);
	mount_object->fd_table = NULL;
	mount_object->fd_table_byname = NULL;
	g_free(mount_object);

	stats_close_logging(stats_file);

	g_debug("Finished cleanup");
}

static int is_quitting(struct mongodbfs_mount* mount_obj)
{
	return (g_atomic_int_get(&mount_obj->quitflag_atomic) != 0);
}

/* File ops callouts */

int mongodbfs_getattr(const char *path, struct stat *stbuf)
{
	// XXX: Implement me
	return -EIO;
}

int mongodbfs_open(const char *path, struct fuse_file_info *fi)
{
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();
	struct mongodbfs_fdentry* fde = NULL;

	if(path == NULL || strlen(path) == 0) {
		return -ENOENT;
	}

	/* On shutdown, fail new requests */
	if(is_quitting(mount_obj))
		return -EIO;

	/* Open succeeded - time to create a fdentry */
	fde = fdentry_new();
	fde->relative_path = g_strdup(path);

	g_static_rw_lock_writer_lock(&mount_obj->fd_table_rwlock);
	fi->fh = fde->fd = mount_obj->next_fd;
	mount_obj->next_fd++; 				// XXX: Atomic, you dumb fuck!
	insert_fdtable_entry(mount_obj, fde);
	g_static_rw_lock_writer_unlock(&mount_obj->fd_table_rwlock);

	/* FUSE handles this differently */
	stats_write_record(stats_file, "open", 0, 0, path);
	return 0;
}

#if 0
static int read_from_fd(int fd, off_t* cur_offset, char* buf, size_t size, off_t offset)
{
	int ret = 0;

	if(*cur_offset != offset) {
		int tmp = lseek(fd, offset, SEEK_SET);
		if (tmp < 0) {
			ret = tmp;
			goto out;
		}
	}

	ret = read(fd, buf, size);
	if (ret >= 0)
		*cur_offset = offset + ret;
out:
	return ret;
}
#endif

int mongodbfs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int ret = 0;
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();
	struct mongodbfs_fdentry* fde = fdentry_from_fd(fi->fh);
	if(!fde)
		return -ENOENT;

	/* On shutdown, fail new requests */
	if(is_quitting(mount_obj))
		return -EIO;

	// XXX: Implement Me!
	return -EIO;

	fdentry_unref(fde); 	// XXX: Is this right?
	return (ret < 0 ? -errno : ret);
}

int mongodbfs_statfs(const char *path, struct statvfs *stat)
{
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();

	/* On shutdown, fail new requests */
	if(is_quitting(mount_obj))
		return -EIO;

	// XXX: Implement me!
	return -EIO;
}

int mongodbfs_release(const char *path, struct fuse_file_info *info)
{
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();
	struct mongodbfs_fdentry* fde = NULL;

	/* On shutdown, fail new requests */
	if(is_quitting(mount_obj))
		return -EIO;

	/* Remove the entry from the fd table */
	g_static_rw_lock_writer_lock(&mount_obj->fd_table_rwlock);
	fde = g_hash_table_lookup(mount_obj->fd_table, &info->fh);
	if (fde) {
		g_hash_table_remove(mount_obj->fd_table, &info->fh);
		GSList* iter = g_hash_table_lookup(mount_obj->fd_table_byname, fde->relative_path);
		if (iter) {
			iter = g_slist_remove(iter, fde);
		}

		/* Update the table to point to the correct SList */

		if (!iter) {
			/* If the SList is empty, remove the entire relative path */
			g_hash_table_remove(mount_obj->fd_table_byname, fde->relative_path);
		} else {
			g_hash_table_replace(mount_obj->fd_table_byname, fde->relative_path, iter);
		}
	}

	g_static_rw_lock_writer_unlock(&mount_obj->fd_table_rwlock);

	if(!fde)
		return -ENOENT;

	// XXX: Is this correct?
	fdentry_unref(fde);

	return 0;
}

int mongodbfs_access(const char *path, int amode)
{
	int ret = 0; 
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();

	if(path == NULL || strlen(path) == 0)
		return -ENOENT;

	/* On shutdown, fail new requests */
	if(is_quitting(mount_obj))
		return -EIO;
	
	// XXX: Implement me
	return -EIO;

	return (ret < 0 ? errno : ret);
}

int mongodbfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi)
{
	struct mongodbfs_mount* mount_obj = get_current_mountinfo();

	if(path == NULL || strlen(path) == 0)
		return -ENOENT;

	/* On shutdown, fail new requests */
	if(is_quitting(mount_obj))
		return -EIO;

	// XXX: Implement me
	return -EIO;

	stats_write_record(stats_file, "readdir", offset, 0, path);

	return 0;
}
