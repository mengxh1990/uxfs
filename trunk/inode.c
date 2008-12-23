#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/statfs.h>
#include "uxfs.h"

static int uxfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	struct ux_sb_info *sbi = uxfs_sb(sb);
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = UX_MAXBLOCKS;
	buf->f_bfree = sbi->s_nbfree;
	buf->f_bavail = sbi->s_nbfree;
	buf->f_files = UX_MAXFILES;
	buf->f_ffree = sbi->s_nifree;
	buf->f_namelen = UX_NAMELEN;
	return 0;
}

struct ux_inode *
uxfs_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)
{
	int block;
	struct ux_inode *p;

	*bh = NULL;
	if (!ino || ino > UX_MAXFILES) {
		printk("Bad inode number on dev %s: %ld is out of range\n",
		       sb->s_id, (long)ino);
		return NULL;
	}
	block = UX_INODE_BLOCK + ino; 
	*bh = sb_bread(sb, block);
	if (!*bh) {
		printk("Unable to read inode block\n");
		return NULL;
	}
	p = (void *)(*bh)->b_data;
	return p;
}

void uxfs_set_inode(struct inode *inode)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &ux_file_inode_operations; 
		inode->i_fop = &ux_file_operations;	
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &ux_dir_inode_operations; 
		inode->i_fop = &ux_dir_operations;
	}
}

struct inode * uxfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	struct buffer_head	*bh;
	struct ux_inode		*raw_inode;
	struct ux_inode_info	*ux_inode;
	int			i;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;
	
	ux_inode = uxfs_i(inode);
	raw_inode = uxfs_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode) {
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	inode->i_mode = raw_inode->i_mode;
	inode->i_nlink = raw_inode->i_nlink;
	inode->i_atime.tv_sec = raw_inode->i_atime;
	inode->i_mtime.tv_sec = raw_inode->i_mtime;
	inode->i_ctime.tv_sec = raw_inode->i_ctime;
	inode->i_mtime.tv_nsec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_ctime.tv_nsec = 0;
	inode->i_uid = (uid_t)raw_inode->i_uid;
	inode->i_gid = (gid_t)raw_inode->i_gid;
	inode->i_size = raw_inode->i_size;
	inode->i_blocks = raw_inode->i_blocks;
	for (i = 0; i < UX_DIRECT_BLOCKS; i++)
		ux_inode->i_data[i] = raw_inode->i_addr[i];
	uxfs_set_inode(inode);
	brelse(bh);
	unlock_new_inode(inode);
	return inode;
}

static struct kmem_cache * uxfs_inode_cachep;

static struct inode *uxfs_alloc_inode(struct super_block *sb)
{
	struct ux_inode_info *ei;
	ei = (struct ux_inode_info *)kmem_cache_alloc(uxfs_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void uxfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(uxfs_inode_cachep, uxfs_i(inode));
}

static void init_once(void * foo)
{
	struct ux_inode_info *ei = (struct ux_inode_info *) foo;

	inode_init_once(&ei->vfs_inode);
}

static int init_inodecache(void)
{
	uxfs_inode_cachep = kmem_cache_create("uxfs_inode_cache",
					     sizeof(struct ux_inode_info),
					     0, (SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD),
					     init_once);
	if (uxfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	kmem_cache_destroy(uxfs_inode_cachep);
}

static void uxfs_put_super(struct super_block *sb)
{
	struct ux_sb_info *sbi = uxfs_sb(sb);
	struct ux_superblock *usb = sbi->s_ms;
	int	i;

	if (!(sb->s_flags & MS_RDONLY))
		mark_buffer_dirty(sbi->s_sbh);
	usb->s_nifree = sbi->s_nifree;
	usb->s_nbfree = sbi->s_nbfree;
	usb->s_mod = sbi->s_mount_state;
	for (i = 0; i < UX_MAXFILES; i++)
		usb->s_inode[i] = sbi->s_inode[i];
	for (i = 0; i < UX_MAXBLOCKS; i++)
		usb->s_block[i] = sbi->s_block[i];
	brelse(sbi->s_sbh);
	sb->s_fs_info = NULL;
	kfree(sbi);
	return;
}

static int uxfs_write_inode(struct inode * inode, int wait)
{
	struct buffer_head	*bh;	
	struct ux_inode		*raw_inode;
	struct ux_inode_info	*ux_inode = uxfs_i(inode);
	int			i;

	raw_inode = uxfs_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode)
		return 0;
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = inode->i_uid;
	raw_inode->i_gid = inode->i_gid;
	raw_inode->i_nlink = inode->i_nlink;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_mtime = inode->i_mtime.tv_sec;
	raw_inode->i_atime = inode->i_atime.tv_sec;
	raw_inode->i_ctime = inode->i_ctime.tv_sec;
	raw_inode->i_blocks = inode->i_blocks;
	for (i = 0; i < UX_DIRECT_BLOCKS; i++)
		raw_inode->i_addr[i] = ux_inode->i_data[i];
	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

static void uxfs_delete_inode(struct inode *inode)
{
	struct ux_sb_info *sbi = uxfs_sb(inode->i_sb);

	struct buffer_head *bh = NULL;
	struct ux_inode *raw_inode;

	uxfs_truncate(inode);
	sbi->s_inode[inode->i_ino] = UX_INODE_FREE;
	sbi->s_nifree++;

	raw_inode = uxfs_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (raw_inode) {
		raw_inode->i_nlink = 0;
		raw_inode->i_mode = 0;
	}
	if (bh) {
		mark_buffer_dirty(bh);
		brelse(bh);
	}

	clear_inode(inode);
}

struct super_operations uxfs_sops = {
	.alloc_inode	= uxfs_alloc_inode,
	.destroy_inode	= uxfs_destroy_inode,
	.write_inode	= uxfs_write_inode,
	.delete_inode	= uxfs_delete_inode,
	.statfs		= uxfs_statfs,
	.put_super	= uxfs_put_super,
};

static int uxfs_fill_super(struct super_block *s, void *data, int silent)
{
	struct ux_superblock	*usb;
	struct buffer_head	*bh;
	struct inode		*root;
	struct ux_sb_info	*sbi;
	int			i;

	sbi = kmalloc(sizeof(struct ux_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	s->s_fs_info = sbi;
	memset(sbi, 0, sizeof(struct ux_sb_info));

	sb_set_blocksize(s, UX_BSIZE);
	s->s_maxbytes = UX_BSIZE * UX_DIRECT_BLOCKS;

	bh = sb_bread(s, 0);
	if(!bh) {
		printk("uxfs: unable to read superblock\n");
		goto outnobh;
	}

	usb = (struct ux_superblock *)bh->b_data;
	if (usb->s_magic != UX_MAGIC) {
		if (!silent)
			printk("VFS: Unable to find uxfs filesystem on dev "
			       "%s.\n", s->s_id);
		goto out;
	}
	if (usb->s_mod == UX_FSDIRTY) {
		printk("uxfs: Filesystem is not clean. Write and "
		       "run fsck!\n");
		goto out;
	}
	sbi->s_ms = usb;
	sbi->s_sbh = bh;
	sbi->s_nifree = usb->s_nifree;
	sbi->s_nbfree = usb->s_nbfree;
	sbi->s_mount_state = usb->s_mod;
	for (i = 0; i < UX_MAXFILES; i++)
		sbi->s_inode[i] = usb->s_inode[i];
	for (i = 0; i < UX_MAXBLOCKS; i++)
		sbi->s_block[i] = usb->s_block[i];

	s->s_magic = UX_MAGIC;
	s->s_fs_info = sbi;
	s->s_op = &uxfs_sops;

	root = uxfs_iget(s, UX_ROOT_INO);
	if (!root)
		goto out;

	s->s_root = d_alloc_root(root);
	if (!s->s_root) {
		iput(root);
		goto out;
	}
	
	return 0;

out:
	s->s_fs_info = NULL;
	brelse(bh);
outnobh:
	kfree(sbi);
	return -EINVAL;
}

static int uxfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, uxfs_fill_super,
			mnt);
}

static struct file_system_type uxfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "uxfs",
	.get_sb		= uxfs_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_uxfs_fs(void)
{
	int err = init_inodecache();
	if (err)
		goto out1;
	err = register_filesystem(&uxfs_fs_type);
	if (err)
		goto out;
	return 0;
out:
	destroy_inodecache();
out1:
	return err;

}

static void __exit exit_uxfs_fs(void)
{
	unregister_filesystem(&uxfs_fs_type);
	destroy_inodecache();
}

module_init(init_uxfs_fs)
module_exit(exit_uxfs_fs)
MODULE_DESCRIPTION("A primitive filesystem for Linux");
MODULE_LICENSE("GPL");
