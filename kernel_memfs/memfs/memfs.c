/*
 * memfs filesystem for Linux.
 *
 * Copyright (C) 2014 liyijun
 *
 * Every memory region export one file inode.
 * Every file should be hava base pointer and length,
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#define MEMFS_DEFAULT_MODE	0755
#define MEMFS_MAGIC 0x9c9c9c9c

static const struct super_operations memfs_ops;
static const struct inode_operations memfs_dir_inode_operations;


const struct address_space_operations memfs_aops = {
	.readpage	= simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
};

const struct file_operations memfs_file_operations = {
	.read		= do_sync_read,
	.aio_read	= generic_file_aio_read,
	.write		= do_sync_write,
	.aio_write	= generic_file_aio_write,
	.llseek		= generic_file_llseek,
	.fsync		= noop_fsync	
};

const struct inode_operations memfs_file_inode_operations = {
};

static struct backing_dev_info memfs_backing_dev_info = {
	.name		= "memfs",
	.ra_pages	= 0,	/* No readahead */
	.capabilities	= BDI_CAP_NO_ACCT_AND_WRITEBACK |
			  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
			  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP | BDI_CAP_EXEC_MAP,
};

struct inode *memfs_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
		inode->i_mapping->a_ops = &memfs_aops;
		inode->i_mapping->backing_dev_info = &memfs_backing_dev_info;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &memfs_file_inode_operations;
			inode->i_fop = &memfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &memfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			break;
		}
	}
	return inode;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
/* SMP-safe */
static int
memfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode * inode = memfs_get_inode(dir->i_sb, dir, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	}
	return error;
}

static int memfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
	int retval = memfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir);
	return retval;
}

static int memfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, struct nameidata *nd)
{
	return memfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int memfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = memfs_get_inode(dir->i_sb, dir, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		} else
			iput(inode);
	}
	return error;
}

static const struct inode_operations memfs_dir_inode_operations = {
	.create		= memfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= simple_unlink,
	.symlink	= memfs_symlink,
	.mkdir		= memfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod		= memfs_mknod,
	.rename		= simple_rename,
};

static const struct super_operations memfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	.show_options	= generic_show_options,
};

struct memfs_mount_opts {
	umode_t mode;
};

enum {
	Opt_mode,
	Opt_err
};

static const match_table_t tokens = {
	{Opt_mode, "mode=%o"},
	{Opt_err, NULL}
};

struct memfs_fs_info {
	struct memfs_mount_opts mount_opts;
};

static int memfs_parse_options(char *data, struct memfs_mount_opts *opts)
{
	substring_t args[MAX_OPT_ARGS];
	int option;
	int token;
	char *p;

	opts->mode = MEMFS_DEFAULT_MODE;

	while ((p = strsep(&data, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_mode:
			if (match_octal(&args[0], &option))
				return -EINVAL;
			opts->mode = option & S_IALLUGO;
			break;
		/*
		 * We might like to report bad mount options here;
		 * but traditionally memfs has ignored all mount options,
		 * and as it is used as a !CONFIG_SHMEM simple substitute
		 * for tmpfs, better continue to ignore other mount options.
		 */
		}
	}

	return 0;
}

int memfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct memfs_fs_info *fsi;
	struct inode *inode;
	int err;

	save_mount_options(sb, data);

	fsi = kzalloc(sizeof(struct memfs_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = memfs_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_CACHE_SIZE;
	sb->s_blocksize_bits	= PAGE_CACHE_SHIFT;
	sb->s_magic		= MEMFS_MAGIC;
	sb->s_op		= &memfs_ops;
	sb->s_time_gran		= 1;

	inode = memfs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}

struct dentry *memfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, memfs_fill_super);
}

static void memfs_kill_sb(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}

static struct file_system_type memfs_fs_type = {
	.name		= "memfs",
	.mount		= memfs_mount,
	.kill_sb	= memfs_kill_sb,
};

static int __init init_memfs_fs(void)
{
	return register_filesystem(&memfs_fs_type);
}
static void __exit exit_memfs_fs(void)
{
	unregister_filesystem(&memfs_fs_type);
}
module_init(init_memfs_fs)
module_exit(exit_memfs_fs)

