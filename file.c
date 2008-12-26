#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "uxfs.h"

struct file_operations ux_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.aio_read	= generic_file_aio_read,
	.write		= do_sync_write,
	.aio_write	= generic_file_aio_write,
};

static int uxfs_get_block(struct inode *inode, sector_t block,
		    struct buffer_head *bh, int create)
{
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	__u32	blk;
	int	error;

	/*
	 * First check to see is the file can be extended.
	 */
	if (block >= UX_DIRECT_BLOCKS)
		return -EFBIG;

	/*
	 * If we're creating, we must allocate a new block.
	 */
	if (create) {
		blk = uxfs_new_block(inode->i_sb, &error);
		if (error) {
			printk("uxfs: ux_get_block - Out of space\n");
			return -ENOSPC;
		}
		ux_inode->i_data[block] = blk;
		inode->i_blocks++;
		mark_inode_dirty(inode);
	}

	map_bh(bh, inode->i_sb, ux_inode->i_data[block]);
	return 0;
}

static int uxfs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, uxfs_get_block, wbc);
}

static int uxfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page,uxfs_get_block);
}

int __uxfs_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	return block_write_begin(file, mapping, pos, len, flags, pagep, fsdata,
				uxfs_get_block);
}

static int uxfs_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	*pagep = NULL;
	return __uxfs_write_begin(file, mapping, pos, len, flags, pagep, fsdata);
}

static sector_t uxfs_bmap(struct address_space *mapping, sector_t block)
{
	return generic_block_bmap(mapping,block,uxfs_get_block);
}

struct address_space_operations ux_aops = {
	.readpage = uxfs_readpage,
	.writepage = uxfs_writepage,
	.sync_page = block_sync_page,
	.write_begin = uxfs_write_begin,
	.write_end = generic_write_end,
	.bmap = uxfs_bmap
};

void uxfs_truncate(struct inode * inode)
{
	struct ux_sb_info *sbi = uxfs_sb(inode->i_sb);
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	int i, blk, last_block;
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
		return;

	last_block = (inode->i_size + UX_BSIZE - 1) / UX_BSIZE;
	block_truncate_page(inode->i_mapping, inode->i_size, uxfs_get_block);
	for (i = last_block; i < inode->i_blocks; i++) {
		blk = ux_inode->i_data[i];
		if (blk) {
			sbi->s_block[blk - UX_FIRST_DATA_BLOCK] = UX_BLOCK_FREE;
			sbi->s_nbfree++;
		}
	}
	inode->i_blocks = last_block;
}

struct inode_operations ux_file_inode_operations = {
	.truncate	= uxfs_truncate,
};
