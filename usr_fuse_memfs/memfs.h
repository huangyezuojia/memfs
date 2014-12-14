#ifndef __MEMFS_H
#define __MEMFS_H

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

enum {
	MEMFS_GET_SIZE	= _IOR('E', 0, size_t),
	MEMFS_SET_SIZE	= _IOW('E', 1, size_t),

	/*
	 * The following two ioctls don't follow usual encoding rules
	 * and transfer variable amount of data.
	 */
	MEMFS_READ	= _IO('E', 2),
	MEMFS_WRITE	= _IO('E', 3),
};

struct fioc_rw_arg {
	off_t		offset;
	void		*buf;
	size_t		size;
	size_t		prev_size;	/* out param for previous total size */
	size_t		new_size;	/* out param for new total size */
};

#endif
