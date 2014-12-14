/*
 * memfs is export memory to fs,
 * author:liyijun
 * mail:156666198@qq.com
 * user can visit memory by vfs's operation,such read/write.
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "memfs.h"

#define MEMFS_SINGLE_FILE "single_file"
#define MEMFS_DIR_TREE "dir_tree"

/*
 * /cache_dir/1_1/1_2/1_3/
 * 第一个目录项，第二个数字代表当前目录的层次
 */
enum {
	MEMFS_NONE,
	MEMFS_ROOT,
	MEMFS_FILE,
	MEMFS_CACHE_DIR,
};

#define MAX_UNIT_EVERY_LAY 10

const char memfs_cache_dir[] = "/"MEMFS_DIR_TREE"/0_0/0_1/0_2";
int memfs_cache_dir_len = sizeof(memfs_cache_dir) -1 ;

const char *ttt_zone[]={
"zone1",
"zone2",
"zone3"
};
char single_file_test;

char dir_tree_test;

struct memfs_object_rw_operations
{
	int (*read)(const char *,char *,size_t,off_t,struct fuse_file_info *);	
	int (*write)(const char *,const char *,size_t,off_t,struct fuse_file_info *);	
};

static int single_file_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	buf[0]=single_file_test;
	printf("buf[0]:%c\n",buf[0]);
	return 1;
}

static int single_file_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	printf("buf[0]:%c\n",buf[0]);
	single_file_test = buf[0];
	return size;
}

static struct memfs_object_rw_operations single_file_ops ={
	.read = single_file_read,
	.write = single_file_write,
};

static int dir_tree_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	buf[0]=dir_tree_test;
	printf("buf[0]:%c\n",buf[0]);
	return 1;
}

static int dir_tree_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	printf("buf[0]:%c\n",buf[0]);
	dir_tree_test = buf[0];
	return size;
}

static struct memfs_object_rw_operations dir_tree_ops ={
	.read = dir_tree_read,
	.write = dir_tree_write,
};

static int check_domain_exist(const char *domain_name)
{
	int i;

	for(i=0;i<sizeof(ttt_zone)/sizeof(char *);i++)
	{
		if(strcmp(domain_name,ttt_zone[i]) == 0)
		{
			return 0;
		}
	}
	return -1;
}

static void fill_domain_to_dir(fuse_fill_dir_t filler,int *unit_num,void *buf)
{
	int i;
	
	for(i=0; i<sizeof(ttt_zone)/sizeof(char *); i++)
	{
		if(unit_num && unit_num[0]==0 && unit_num[1]==0 && unit_num[2]==0)
		{
			filler(buf,ttt_zone[i],NULL,0);
		}
	}
}

static void fill_dirinfo_for_lay(fuse_fill_dir_t filler,int lay_num,void *buf)
{
	int i;
	char buffer[4]={0};

	for(i=0; i<MAX_UNIT_EVERY_LAY; i++)
	{
		buffer[0]='0'+i;
		buffer[1]='_';
		buffer[2]='0'+lay_num;
		
		filler(buf,buffer,NULL,0);
	}
}

static int check_file_exist(const char *path)
{
	int path_len;
	const char *domain_name;
	
	if (strcmp(path, "/" MEMFS_SINGLE_FILE) == 0)
		return 0;

	path_len = strlen(path);
	if(path_len <= memfs_cache_dir_len)
		goto end;		
	
	domain_name = path + memfs_cache_dir_len + 1;/* 1 for one '/' */	
	return check_domain_exist(domain_name);

end:
	return -1;
}

static int memfs_file_dir_type(const char *path,int *layer_num,int *unit_num)
{
	int len;
	
	len = strlen(path);

	if (strcmp(path, "/") == 0)
		return MEMFS_ROOT;

	if( strcmp(path,"/" MEMFS_DIR_TREE) == 0)
	{
		if(layer_num)
			*layer_num = 0;
		return MEMFS_CACHE_DIR;
	}
	
	if(len > 2 && (path[len-1]== '0'||path[len-1]=='1'||path[len-1]=='2') && path[len-2]=='_')
	{
		char num_buf[2];

		if(layer_num)
		{
			num_buf[0]=path[len-1];
			num_buf[1]=0;
			*layer_num = atoi(num_buf)+1;
		}
		
		if(unit_num && len >= memfs_cache_dir_len)
		{
			int i;
			num_buf[0]=path[len-3-8];
			num_buf[1]=0;
			unit_num[0] = atoi(num_buf);
			
			num_buf[0]=path[len-3-4];
			num_buf[1]=0;
			unit_num[1] = atoi(num_buf);

			num_buf[0]=path[len-3];
			num_buf[1]=0;
			unit_num[2] = atoi(num_buf);
		}
		return MEMFS_CACHE_DIR;
	}

	if(check_file_exist(path)== 0)
		return MEMFS_FILE;
	
	return MEMFS_NONE;
}

static int memfs_file_type(const char *path)
{
	return memfs_file_dir_type(path,NULL,NULL);
}

static int memfs_getattr(const char *path, struct stat *stbuf)
{
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = time(NULL);

	switch (memfs_file_type(path)) {
	case MEMFS_ROOT:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		break;
	case MEMFS_CACHE_DIR:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		break;	
	case MEMFS_FILE:
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = 4;
		break;
	case MEMFS_NONE:
		return -ENOENT;
	}

	return 0;
}

static int memfs_open(const char *path, struct fuse_file_info *fi)
{
	(void) fi;

	if (memfs_file_type(path) != MEMFS_NONE)
		return 0;
	return -ENOENT;
}

int get_memfs_object_ops(const char *path,struct memfs_object_rw_operations **file_ops)
{
	if(strcmp(path,"/""single_file") == 0){
		*file_ops = &single_file_ops;
	}else if(check_file_exist(path) ==0 ){
		*file_ops = &dir_tree_ops;
	}
	return 0;
}

static int memfs_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	struct memfs_object_rw_operations *file_ops = NULL;

	if (memfs_file_type(path) != MEMFS_FILE)
		return -EINVAL;
	
	get_memfs_object_ops(path,&file_ops);
	if(file_ops)
		return file_ops->read(path,buf,size,offset,fi);

	return 0;
}

static int memfs_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	struct memfs_object_rw_operations *file_ops = NULL;

	if (memfs_file_type(path) != MEMFS_FILE)
		return -EINVAL;

	get_memfs_object_ops(path,&file_ops);
	if(file_ops)
		return file_ops->write(path,buf,size,offset,fi);

	return size;
}

static int memfs_truncate(const char *path,off_t size)
{
	return 0;
}

static int memfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	(void) offset;
	int lay_num;
	int unit_num[3];

	if (memfs_file_type(path) == MEMFS_ROOT)
	{
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		filler(buf,MEMFS_SINGLE_FILE,NULL,0);
		filler(buf,MEMFS_DIR_TREE,NULL,0);

		return 0;
	}else if(memfs_file_dir_type(path,&lay_num,unit_num) == MEMFS_CACHE_DIR){

		if(lay_num >= 0 && lay_num <=2 )
		{
			fill_dirinfo_for_lay(filler,lay_num,buf);
		}else if(lay_num == 3){
			fill_domain_to_dir(filler,unit_num,buf);	
		}
		return 0;
	}
	return -ENOENT;
}

static int memfs_ioctl(const char *path, int cmd, void *arg,
		      struct fuse_file_info *fi, unsigned int flags, void *data)
{
	(void) arg;
	(void) fi;
	(void) flags;

	if (memfs_file_type(path) != MEMFS_FILE)
		return -EINVAL;

	if (flags & FUSE_IOCTL_COMPAT)
		return -ENOSYS;

	switch (cmd) {
	case MEMFS_GET_SIZE:
		*(size_t *)data = 4;
		return 0;

	case MEMFS_SET_SIZE:
		return 0;
	}

	return -EINVAL;
}

static struct fuse_operations memfs_oper = {
	.getattr	= memfs_getattr,
	.readdir	= memfs_readdir,
	.truncate	= memfs_truncate,
	.open		= memfs_open,
	.read		= memfs_read,
	.write		= memfs_write,
	.ioctl		= memfs_ioctl,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &memfs_oper, NULL);
}
