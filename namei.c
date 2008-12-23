#include <linux/buffer_head.h>
#include "uxfs.h"

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

static struct dentry *uxfs_lookup(struct inode * dir, struct dentry *dentry,
				  struct nameidata *nd)
{
	ino_t	ino = 0;
	struct inode *inode = NULL;

	if (dentry->d_name.len > UX_NAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	ino = uxfs_find_entry(dir, (char *)dentry->d_name.name);
	if (ino) {
		inode = uxfs_iget(dir->i_sb, ino);
		if (!inode)
			return ERR_PTR(-EACCES);
	}
	d_add(dentry, inode);
	return NULL;
}

struct inode_operations ux_dir_inode_operations = {
	.lookup = uxfs_lookup,
};
