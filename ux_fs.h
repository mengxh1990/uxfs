#ifndef __UX_FS_H__
#define __UX_FS_H__

#include <linux/types.h>
#define UX_NAMELEN		28
#define UX_DIRECT_BLOCKS	16
#define UX_MAXFILES		32
#define UX_MAXBLOCKS		479	/* 512 - 1 - 32 */
#define UX_FIRST_DATA_BLOCK	33
#define UX_BSIZE		1024
#define UX_BSIZE_BITS		10
#define UX_MAGIC		0x58494e55
#define UX_INODE_BLOCK		4
#define UX_ROOT_INO		2
#define UX_DIR_PER_BLK		32	/* 1024 / 32 */
/*
 * The on-disk superblock. The number of inodes and 
 * data blocks is fixed.
 */

struct ux_superblock {
	__u32	s_magic;
	__u32	s_mod;
	__u32	s_nifree;
	__u32	s_nbfree;
	__u8	s_inode[UX_MAXFILES];
	__u8	s_block[UX_MAXBLOCKS];
};

/*
 * The on-disk inode.
 */

struct ux_inode {
	__u32	i_mode;
	__u32	i_nlink;
	__u32	i_atime;
	__u32	i_mtime;
	__u32	i_ctime;
	__s32	i_uid;
	__s32	i_gid;
	__u32	i_size;
	__u32	i_blocks;
	__u32	i_addr[UX_DIRECT_BLOCKS];
};

/*
 * Allocation flags
 */

#define UX_INODE_FREE     0
#define UX_INODE_INUSE    1
#define UX_BLOCK_FREE     0
#define UX_BLOCK_INUSE    1

/*
 * Filesystem flags
 */

#define UX_FSCLEAN	0
#define UX_FSDIRTY	1

/*
 * FIxed size directory entry.
 */

struct ux_dirent {
	__u32	d_ino;
	char	d_name[UX_NAMELEN];
};

#endif /* __UX_FS_H__ */
