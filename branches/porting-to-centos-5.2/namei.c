#include <linux/buffer_head.h>
#include <linux/smp_lock.h>
#include "uxfs.h"

struct inode * uxfs_new_inode(struct super_block *sb, int *error)
{
	struct ux_sb_info *sbi = uxfs_sb(sb);
	struct inode *inode = new_inode(sb);
	int i;

	if (!inode) {
		*error = -ENOMEM;
		return NULL;
	}

	if (sbi->s_nifree == 0) {
		printk("uxfs: Out of inodes\n");
		iput(inode);
		*error = -ENOSPC;
		return NULL;
	}

	for (i = 3; i < UX_MAXFILES; i++) {
		if (sbi->s_inode[i] == UX_INODE_FREE) {
			sbi->s_inode[i] = UX_INODE_INUSE;
			sbi->s_nifree--;
			sb->s_dirt = 1;
			break;
		}
	}
	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_ino = i;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
	inode->i_blocks = 0;
	memset(uxfs_i(inode)->i_data, 0, UX_DIRECT_BLOCKS);
	insert_inode_hash(inode);
	mark_inode_dirty(inode);

	*error = 0;
	return inode;
}

int uxfs_new_block(struct super_block *sb, int *error)
{
	struct ux_sb_info *sbi = uxfs_sb(sb);
	int i;

	if (sbi->s_nbfree == 0)
		goto nospace;

	for (i = 0; i < UX_MAXBLOCKS; i++) {
		if (sbi->s_block[i] == UX_BLOCK_FREE) {
			sbi->s_inode[i] = UX_BLOCK_INUSE;
			sbi->s_nbfree--;
			sb->s_dirt = 1;
			*error = 0;
			return i + UX_FIRST_DATA_BLOCK;
		}
	}

nospace:
	printk("uxfs: Out of blocks\n");
	*error = -ENOSPC;
	return 0;
}

int uxfs_add_link(struct dentry *dentry, struct inode *inode)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct ux_inode_info *ux_inode = uxfs_i(dir);
	const char * name = dentry->d_name.name;
	struct super_block * sb = dir->i_sb;
	struct ux_dirent * de = NULL;
	struct buffer_head *bh = NULL;
        __u32 blk = 0;
	int error;
	int i, j;

	for (i = 0; i < dir->i_blocks; i++) {
		bh = sb_bread(dir->i_sb, ux_inode->i_data[i]);	
		if (!bh) {
			printk("uxfs: unable to read dir block\n");
			return -EIO;
		}

		de = (struct ux_dirent *)bh->b_data;
		for (j = 0; j < UX_DIR_PER_BLK; j++, de++) {
			if (de->d_ino == 0)
				goto found_empty_dentry;	
		}
		brelse(bh);
	}

	/*
	 * We didn't find an empty slot so need to allocate 
	 * a new block if there's space in the inode.
	 */
	if (dir->i_blocks < UX_DIRECT_BLOCKS) {
		blk = uxfs_new_block(sb, &error);
		dir->i_blocks++;
		ux_inode->i_data[dir->i_blocks] = blk;
		bh = sb_bread(sb, blk);
		memset(bh->b_data, 0, UX_BSIZE);
		de = (struct ux_dirent *)bh->b_data;
		goto found_empty_dentry;
	}

found_empty_dentry:
	de->d_ino = inode->i_ino;
	strcpy(de->d_name, name);
	mark_buffer_dirty(bh);
	dir->i_size += sizeof(struct ux_dirent);
	mark_inode_dirty(dir);
	brelse(bh);
	return 0;
}

static int uxfs_diradd(struct dentry *dentry, struct inode *inode)
{
	int err = uxfs_add_link(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}

int uxfs_find_entry(struct inode *dir, char *name)
{
	struct ux_inode_info *ux_inode = uxfs_i(dir);
	struct buffer_head *bh;
	struct ux_dirent *de;
	off_t offset = 0;
	__u32 ino = 0;
	int i, j;

	for (i = 0, offset = 0; i < dir->i_blocks; i++) {
		bh = sb_bread(dir->i_sb, ux_inode->i_data[i]);	
		if (!bh) {
			printk("uxfs: unable to read dir block\n");
			goto out;
		}

		de = (struct ux_dirent *)bh->b_data;
		for (j = 0; j < UX_DIR_PER_BLK && offset < dir->i_size;
				j++, de++) {
			if (strncmp(name, de->d_name, strlen(name)) == 0) {
				ino = de->d_ino;
			    	brelse(bh);
				goto out;
			}
		}
		brelse(bh);
	}
out:
	return ino;
}

int uxfs_delete_entry(struct inode *dir, char *name)
{
	struct ux_inode_info *ux_inode = uxfs_i(dir);
	struct buffer_head *bh;
	struct ux_dirent *de;
	off_t offset = 0;
	int i, j;

	for (i = 0, offset = 0; i < dir->i_blocks; i++) {
		bh = sb_bread(dir->i_sb, ux_inode->i_data[i]);	
		if (!bh) {
			printk("uxfs: unable to read dir block\n");
			goto out;
		}

		de = (struct ux_dirent *)bh->b_data;
		for (j = 0; j < UX_DIR_PER_BLK && offset < dir->i_size;
				j++, de++) {
			if (strncmp(name, de->d_name, strlen(name)) == 0) {
				de->d_ino = 0;
				memset(de->d_name, 0, UX_NAMELEN);
				mark_inode_dirty(dir);
				mark_buffer_dirty(bh);
			    	brelse(bh);
				return 0;
			}
		}
		brelse(bh);
	}
out:
	return -ENOENT;
}

static struct dentry *uxfs_lookup(struct inode * dir, struct dentry *dentry,
				  struct nameidata *nd)
{
	ino_t	ino = 0;
	struct inode *inode = NULL;

	if (dentry->d_name.len > UX_NAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	ino = uxfs_find_entry(dir, (char *)dentry->d_name.name);
	if (ino) {
		inode = iget(dir->i_sb, ino);
		if (!inode)
			return ERR_PTR(-EACCES);
	}
	d_add(dentry, inode);
	return NULL;
}

static int uxfs_create(struct inode * dir, struct dentry *dentry, int mode,
		struct nameidata *nd)
{
	struct inode *inode;
	ino_t	inum = 0;
	int error;

	/*
	 * See if the entry exists. If not, create a new 
	 * disk inode, and incore inode. The add the new 
	 * entry to the directory.
	 */

	inum = uxfs_find_entry(dir, (char *)dentry->d_name.name);
	if (inum)
		return -EEXIST;

	inode = uxfs_new_inode(dir->i_sb, &error);
	if (inode) {
		inode->i_mode = mode;
		uxfs_set_inode(inode);
		mark_inode_dirty(inode);
		error = uxfs_diradd(dentry, inode);
	}
	return error;
}

static int uxfs_unlink(struct inode * dir, struct dentry *dentry)
{
	int err = -ENOENT;
	struct inode * inode = dentry->d_inode;

	err = uxfs_delete_entry(dir, (char *)dentry->d_name.name);
	if (err)
		goto end_unlink;

	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);
end_unlink:
	return err;
}

static int uxfs_make_empty(struct inode *inode, struct inode *dir)
{
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	struct buffer_head *bh;
	struct ux_dirent *de;
	int blk, err;

	blk = uxfs_new_block(inode->i_sb, &err);
	if (err)
		return err;
	bh = sb_bread(inode->i_sb, blk);
	de = (struct ux_dirent *)bh->b_data;
	de->d_ino = inode->i_ino;
	strcpy(de->d_name, ".");
	de++;
	de->d_ino = dir->i_ino;
	strcpy(de->d_name, "..");
	mark_buffer_dirty(bh);
	brelse(bh);

	inode->i_blocks = 1;
	inode->i_size = sizeof(struct ux_dirent) * 2;
	ux_inode->i_data[0] = blk;
	mark_inode_dirty(inode);
	return 0;
}

static int uxfs_mkdir(struct inode * dir, struct dentry *dentry, int mode)
{
	struct inode * inode;
	int err = -EMLINK;

	inode_inc_link_count(dir);

	inode = uxfs_new_inode(dir->i_sb, &err);
	if (!inode)
		goto out_dir;

	inode->i_mode = S_IFDIR | mode;
	if (dir->i_mode & S_ISGID)
		inode->i_mode |= S_ISGID;
	uxfs_set_inode(inode);

	inode_inc_link_count(inode);

	err = uxfs_make_empty(inode, dir);
	if (err)
		goto out_fail;

	err = uxfs_add_link(dentry, inode);
	if (err)
		goto out_fail;

	d_instantiate(dentry, inode);
out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	inode_dec_link_count(inode);
	iput(inode);
out_dir:
	inode_dec_link_count(dir);
	goto out;

	return 0;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
int uxfs_empty_dir(struct inode * inode)
{
	struct ux_inode_info *ux_inode = uxfs_i(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	struct ux_dirent *de;
	off_t offset = 0;
	int i, j;

	for (i = 0; i < inode->i_blocks; i++) {
		bh = sb_bread(sb, ux_inode->i_data[i]);
		if (!bh) {
			printk("uxfs: unable to read dir block\n");
			return 0;
		}

		de = (struct ux_dirent *)bh->b_data;
		for (j = 0; j < UX_DIR_PER_BLK && offset < inode->i_size; j++, de++) {
			if (de->d_ino) {
				/* check for . and .. */
				if (de->d_name[0] != '.')
					goto not_empty;
				if (!de->d_name[1]) {
					if (de->d_ino != inode->i_ino)
						goto not_empty;
				} else if (de->d_name[1] != '.') {
					goto not_empty;
				} else if (de->d_name[2]) {
					goto not_empty;
				}
			}
			offset += sizeof(struct ux_dirent);
		}
		brelse(bh);
	}

	return 1;
not_empty:
	brelse(bh);
	return 0;
}

static int uxfs_rmdir(struct inode * dir, struct dentry *dentry)
{
	struct inode * inode = dentry->d_inode;
	int err = -ENOTEMPTY;

	if (uxfs_empty_dir(inode)) {
		err = uxfs_unlink(dir, dentry);
		if (!err) {
			inode_dec_link_count(dir);
			inode_dec_link_count(inode);
		}
	}
	return err;
}

struct inode_operations ux_dir_inode_operations = {
	.lookup = uxfs_lookup,
	.create = uxfs_create,
	.unlink	= uxfs_unlink,
	.mkdir	= uxfs_mkdir,
	.rmdir	= uxfs_rmdir,
};
