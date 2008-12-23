BUILD_SRC = /lib/modules/`uname -r`/build
obj-m += uxfs.o
uxfs-objs := inode.o dir.o namei.o

.PHONY: all modules
all: uxmkfs modules
uxmkfs: mkfs.c ux_fs.h
	$(CC) $^ -o $@
write_test: write_test.c
	$(CC) $^ -o $@
modules:
	make -C $(BUILD_SRC) SUBDIRS=`pwd` modules
clean:
	$(RM) *.o *.ko uxmkfs
