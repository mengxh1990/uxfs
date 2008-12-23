#ifndef __UXFS_H__
#define __UXFS_H__

#include <linux/fs.h>
#include "ux_fs.h"

struct ux_inode_info {
	__u32	i_data[UX_DIRECT_BLOCKS];
	struct inode vfs_inode;
};

struct ux_sb_info {
	__u32	s_nifree;
	__u32	s_nbfree;
	__u32	s_inode[UX_MAXFILES];
	__u32	s_block[UX_MAXBLOCKS];
	unsigned short s_mount_state;
	struct ux_superblock * s_ms;
	struct buffer_head *s_sbh;
};

extern struct file_operations ux_dir_operations;
extern struct inode_operations ux_dir_inode_operations;

static inline struct ux_sb_info *uxfs_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct ux_inode_info *uxfs_i(struct inode *inode)
{
	return list_entry(inode, struct ux_inode_info, vfs_inode);
}

extern struct inode * uxfs_new_inode(struct super_block *sb, int *error);
extern void uxfs_set_inode(struct inode *inode);
extern struct inode * uxfs_iget(struct super_block *sb, unsigned long ino);

#endif /* __UXFS_H__ */
