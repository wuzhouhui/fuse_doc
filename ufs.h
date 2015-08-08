#ifndef _UFS_H
#define _UFS_H

#include "apue.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>

/* the max and min size of disk file, in megabyte */
#define DISK_MAX_SIZE	32
#define DISK_MIN_SIZE	1
/* the size of block */
#define BLK_SIZE_SHIFT	9
#define BLK_SIZE	(1 << BLK_SIZE_SHIFT)
#define MAGIC		0x7594

/* # inode per block */
#define INUM_PER_BLK	(BLK_SIZE / sizeof(struct d_inode))

/* super block in disk */
struct d_super_block {
	/* magic number of filesystem */
	unsigned short s_magic;
	/* # block that inode map used */
	blkcnt_t s_imap_blocks;
	/* # block that zone map used */
	blkcnt_t s_zmap_blocks;
	/* # block that inode used */
	blkcnt_t s_inode_blocks;
	/* # block that zone used */
	blkcnt_t s_zone_blocks;
	/* the max file size that filesystem supports */
	off_t	s_max_size;
};

/* super block in memory */
struct m_super_block {
	/* magic number of filesystem */
	unsigned short s_magic;
	/* # block that inode map used */
	blkcnt_t s_imap_blocks;
	/* # block that zone map used */
	blkcnt_t s_zmap_blocks;
	/* # block that inode used */
	blkcnt_t s_inode_blocks;
	/* # block that zone used */
	blkcnt_t s_zone_blocks;
	/* the max file size that filesystem supports */
	off_t	s_max_size;

	/* following fields exist in memory */
	/* inode bit map */
	char	*s_imap;
	/* zone bit map */
	char	*s_zmap;
	/* 1st inode block' number in disk */
	blkcnt_t s_1st_inode_block;
	/* 1st zone block' number in disk */
	blkcnt_t s_1st_zone_block;
	/* available inode left */
	ino_t	s_inode_left;
	/* available zone bloc left */
	blkcnt_t s_block_left;
	/* file descriptor of disk file */
	int	s_fd;
	/* address of disk file in memeory */
	void	*s_addr;
};

/* inode in disk */
struct d_inode {
	/* # of links into this inode */
	nlink_t	i_nlink;
	/* file type and permission */
	mode_t	i_mode;
	/* the size of file */
	off_t	i_size;
	/* the last time this file accessed */
	time_t	i_atime;
	/* the last time this file modifed */
	time_t	i_mtime;
	/* the last time this inode changed */
	time_t	i_ctime;
	/* the owner of this file */
	uid_t	i_uid;
	/* the owner'group of this file */
	gid_t	i_gid;
	/*
	 * 0-5: direct block.
	 * 6: indirect block.
	 * 7: double indirect block.
	 */
	blkcnt_t i_zones[8];
};

#define MAX_FILE_SIZE	(8259 << 10)

#endif /* end of _UFS_H */
