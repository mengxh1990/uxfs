#include <linux/buffer_head.h>
#include "uxfs.h"

static int uxfs_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	unsigned long pos = filp->f_pos;
	struct inode *inode = filp->f_dentry->d_inode;
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	struct ux_dirent *de;
	off_t offset = 0;
	int i, j;

	if (pos >= inode->i_size) {
		offset = inode->i_size;
		goto done;
	}

	for (i = 0; i < inode->i_blocks; i++) {
		bh = sb_bread(sb, ux_inode->i_data[i]);
		if (!bh) {
			printk("uxfs: unable to read dir block\n");
			goto done;
		}

		de = (struct ux_dirent *)bh->b_data;
		for (j = 0; j < UX_DIR_PER_BLK && offset < inode->i_size; j++, de++) {
			if (offset < pos)
				continue;
			if (de->d_ino) {
				if (filldir(dirent, de->d_name,
					   strnlen(de->d_name, UX_NAMELEN),
					   offset, de->d_ino, DT_UNKNOWN)) {
					brelse(bh);
					goto done;
				}
			}
			offset += sizeof(struct ux_dirent);
		}
		brelse(bh);
	}

done:
	filp->f_pos = offset;
	return 0;
}

struct file_operations ux_dir_operations = {
	.read		= generic_read_dir,
	.readdir	= uxfs_readdir,
};
