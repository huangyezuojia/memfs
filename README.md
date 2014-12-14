memfs
=====

memory filesystem

目标是做专业的内存文件系统

1)kernel_memfs 是在内核中构建内存文件系统
2)usr_fuse_memfs是在用户空间构建内存文件系统,依赖内核的fuse模块和上层的fuse用户空间sdk库
  编译的时候，需要指定Makefile中的FUSE_SDK_PATH
