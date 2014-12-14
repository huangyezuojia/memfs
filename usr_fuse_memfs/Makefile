FUSE_SDK_PATH="/root/fuse/install"

all:
	gcc memfs.c -I $(FUSE_SDK_PATH)/include/ $(FUSE_SDK_PATH)/lib/libfuse.a  -D_FILE_OFFSET_BITS=64 -o memfs -lpthread -ldl -lrt
