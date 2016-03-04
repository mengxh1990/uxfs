# Introduction #

This page shows the physical disk layout of uxfs filesystem.

# Details #

## layout ##
```
 <UNIT>         <BLOCK #>   <MEANING>
 super block    0           superblock of _uxfs_ file system  
 inode tables   1 ~ 32      contain uxfs inodes        
 data blocks    33 ~ 511    real data blocks
```

**superblock**:

```
 struct ux_superblock {
         __u32   s_magic;
         __u32   s_mod;
         __u32   s_nifree;
         __u32   s_nbfree;
         __u8    s_inode[UX_MAXFILES];
         __u8    s_block[UX_MAXBLOCKS];
 };
```

**on-disk inode**:

```
 struct ux_inode {
         __u32   i_mode;
         __u32   i_nlink;
         __u32   i_atime;
         __u32   i_mtime;
         __u32   i_ctime;
         __s32   i_uid;
         __s32   i_gid;
         __u32   i_size;
         __u32   i_blocks;
         __u32   i_addr[UX_DIRECT_BLOCKS];
 };
```

**directory entry**

```
 struct ux_dirent {
         __u32   d_ino;
         char    d_name[UX_NAMELEN];
 };
```

**macros**
```
 #define UX_NAMELEN              28
 #define UX_DIRECT_BLOCKS        16
 #define UX_MAXFILES             32
 #define UX_MAXBLOCKS            479     /* 512 - 1 - 32 */
 #define UX_FIRST_DATA_BLOCK     33
 #define UX_BSIZE                1024
 #define UX_BSIZE_BITS           10
 #define UX_MAGIC                0x58494e55
 #define UX_INODE_BLOCK          4
 #define UX_ROOT_INO             2
 #define UX_DIR_PER_BLK          32      /* 1024 / 32 */
```

Notes:
  * All layout related structures are defined in ux\_fs.h
  * Block size is always UX\_BSIZE(1024)
  * The filesystem only has 512 blocks, so the fixed size of _uxfs_ filesystem is 512K
  * One inode takes the whole block just for convenience, so we can only create 32 files in _uxfs_ filesystem