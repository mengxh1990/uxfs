#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "uxfs.h"

#define MIN(a, b) ((a > b)? b: a)

ssize_t uxfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	int	blk;
	off_t	offset, count, total_count = 0;
	struct buffer_head *bh;
	int error;

again:
	if (*ppos >= inode->i_size) {
		if (inode->i_blocks < UX_DIRECT_BLOCKS) {
			blk = uxfs_new_block(inode->i_sb, &error);
			if (error)
				goto out;
			ux_inode->i_data[inode->i_blocks] = blk;
			inode->i_blocks++;
			mark_inode_dirty(inode);
		} else {
			printk("uxfs: file size too big!\n");
			error = -EFAULT;
			goto out;
		}
	}

	blk = *ppos / UX_BSIZE;
	offset = *ppos % UX_BSIZE;
	bh = sb_bread(inode->i_sb, ux_inode->i_data[blk]);
	if (!bh) {
		error = -EIO;
		goto out;
	}
	count = MIN(UX_BSIZE - offset, len);
	if (copy_from_user(bh->b_data + offset, buf, count)) {
		error = -EFAULT;
		goto out;
	}
	mark_buffer_dirty(bh);
	brelse(bh);
	*ppos += count;
	total_count += count;
	if (*ppos > inode->i_size)
		inode->i_size = *ppos;
	if (count < len) {
		buf += count;
		len -= count;
		goto again;
	}
out:
	if (total_count)
		return total_count;
	return error;
}

ssize_t uxfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	int	blk;
	off_t	offset, count;
	struct buffer_head *bh;

	if (*ppos >= inode->i_size)
		return 0;

	blk = *ppos / UX_BSIZE;
	offset = *ppos % UX_BSIZE;
	bh = sb_bread(inode->i_sb, ux_inode->i_data[blk]);
	if (!bh)
		return -EIO;
	count = MIN(inode->i_size - *ppos, len);
	count = MIN(UX_BSIZE - offset, count);
	if (copy_to_user(buf, bh->b_data + offset, count))
		return -EFAULT;
	mark_buffer_dirty(bh);
	brelse(bh);
	*ppos += count;
	return count;
}

struct file_operations ux_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= uxfs_read,
	.write		= uxfs_write,
};

void uxfs_truncate(struct inode * inode)
{
	struct ux_sb_info *sbi = uxfs_sb(inode->i_sb);
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	int i, blk;
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
		return;

	inode->i_size = 0;
	for (i = 0; i < inode->i_blocks; i++) {
		blk = ux_inode->i_data[i];
		sbi->s_block[blk - UX_FIRST_DATA_BLOCK] = UX_BLOCK_FREE;
	}
	sbi->s_nbfree += inode->i_blocks;
}

struct inode_operations ux_file_inode_operations = {
	.truncate	= uxfs_truncate,
};
