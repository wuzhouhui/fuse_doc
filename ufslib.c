#include "ufs.h"

extern struct m_super_block sb;

/* determine whether the inode inum is valid */
static inline int is_ivalid(ino_t inum)
{
	return(inum >= 1 && inum < (sb.s_inode_blocks << (BLK_SIZE_SHIFT + 3)));
}

static inline int is_zvalid(blkcnt_t znum)
{
	return(znum >= 1 && znum <= sb.s_zone_blocks);
}

static inline int is_bvalid(blkcnt_t bnum)
{
	return(bnum >= sb.s_1st_inode_block &&
			bnum < (sb.s_1st_zone_block + sb.s_zone_blocks));
}

static inline int is_dvalid(blkcnt_t dnum)
{
	return(dnum >= 0  && dnum < (6 + ZNUM_PER_BLK + ZNUM_PER_BLK *
				ZNUM_PER_BLK));
}

static blkcnt_t creat_zone(ino_t inum, block dnum)
{
	blkcnt_t ret = 0;
	log_msg("creat_zone called, inum = %u, dnum = %u", inum, dnum);
	ret = _datanum2zonenum(inum, dnum, 1);
	log_msg("creat_zone return %u", ret);
	return(ret);
}
/*
 * read super block from disk, return the disk file's file descriptor
 */
int read_sb(const char *disk_name)
{
	int	fd;

	if (disk_name == NULL || disk_name[0] == 0)
		return(-1);
	if ((fd = open(disk_name, O_RDWR)) < 0)
		err_sys("read_sb: open %s error.", disk_name);
	if (read(fd, &sb, sizeof(struct d_super_block)) !=
			sizeof(struct d_super_block))
		err_sys("read_sb: read %s error.", disk_name);
	if (sb.s_magic != MAGIC) {
		err_msg("filesystem magic %x isn't right.", sb.s_magic);
		return(-1);
	}
	return(fd);
}

ino_t new_inode(void)
{
	char	*p = sb.s_imap;
	int	n = sb.s_imap_blocks << BLK_SIZE_SHIFT;
	int	i, j;
	ino_t	ret = 0;

	log_msg("new_inode called");

	for (i = 0; i < n; i++) {
		if (p[i] == 0xff)
			continue;
		for (j = 0; j < 8; j++) {
			if (p[i] & (1 << j))
				continue;

			/* find a free inode */
			p[i] |= 1 << j;
			ret = (i << 3) + j;
			goto out;
		}
	}
out:
	log_msg("new_inode return %d", (int)ret);
	return(ret);
}

int free_inode(ino_t inum)
{
	ino_t	n;
	int	ret;

	log_msg("free_inode called, inum = %u", inum);
	if (!is_ivalid(inum)) {
		log_msg("free_inode: inum out of range, inum = %u", inum);
		ret = -1;
		goto out;
	}

	n = inum >> 3;
	if ((sb.s_imap[n] & (1 << (inum & 7))) == 0) {
		log_msg("free_inode: inode %d already freed", (int)inum);
		ret = -1;
	} else {
		sb.s_imap[n] &= ~(1 << (inum & 7));
		ret = 0;
	}

out:
	log_msg("free_inode return %d", ret);
	return(ret);
}

int rd_inode(ino_t inum, struct d_inode *inode)
{
	int	ret;
	blkcnt_t blknum;
	char	buf[BLK_SIZE];

	log_msg("rd_inode called, inode num = %u", inum);
	if (inode == NULL) {
		ret = -EINVAL;
		log_msg("rd_inode: inode is NULL");
		goto out;
	}
	if (!is_ivalid(inum)) {
		ret = -EINVAL;
		log_msg("rd_inode: inode number %u out of range", inum);
		goto out;
	}
	if ((blknum = inum2blknum(inum)) == 0) {
		ret = -EINVAL;
		log_msg("rd_inode: block number of inode %u is zero", inum);
		goto out;
	}
	if ((ret = rd_blk(blknum, buf, sizeof(buf))) < 0) {
		log_msg("rd_inode: rd_blk error for block %u", blknum);
		goto out;
	}
	/* inode started from 1, so substract by 1 */
	*inode = ((struct d_inode *)buf)[(inum - 1) % INUM_PER_BLK];
	ret = 0;
out:
	log_msg("rd_inode return %d", ret);
	return(ret);
}

int wr_inode(const struct m_inode *inode)
{
	int	ret;
	blkcnt_t blknum;
	char	buf[BLK_SIZE];

	log_msg("wr_inode called");
	if (inode == NULL) {
		ret = -1;
		log_msg("wr_inode: inode is NULL");
		goto out;
	}
	if (!is_ivalid(inode->i_ino)) {
		ret = -1;
		log_msg("wr_inode: inode number %u out of range",
				inode->i_ino);
		goto out;
	}
	if ((blknum = inum2blknum(inode->i_ino)) == 0) {
		ret = -1;
		log_msg("wr_inode: block number of inode %u is zero",
				inode->i_ino);
		goto out;
	}
	if (rd_blk(blknum, buf, sizeof(buf)) < 0) {
		ret = -1;
		log_msg("wr_inode: rd_blk error for block %u", blknum);
		goto out;
	}
	/* inode started from 1, so substract by 1 */
	((struct d_inode *)buf)[(inode->i_ino - 1) % INUM_PER_BLK] =
		*(struct d_inode *)inode;
	if (wr_blk(blknum, buf, sizeof(buf)) < 0) {
		log_msg("wr_inode: wr_blk error for block %u", blknum);
		ret = -1;
		goto out;
	}
	ret = 0;
out:
	log_msg("rd_inode return %d", ret);
	return(ret);
}

blkcnt_t new_zone(void)
{
	char	*p = sb.s_zmap;
	int	n = sb.s_zmap_blocks << BLK_SIZE_SHIFT;
	int	i, j;
	ino_t	ret = 0;

	log_msg("new_zone called");

	for (i = 0; i < n; i++) {
		if (p[i] == 0xff)
			continue;
		for (j = 0; j < 8; j++) {
			if (p[i] & (1 << j))
				continue;

			/* find a free zone bit */

			/* test if there is available zone in disk */
			if (((i << 3) + j - 1) >= sb.s_zone_blocks) {
				ret = 0;
				goto out;
			}
			p[i] |= 1 << j;
			ret = (i << 3) + j;
			goto out;
		}
	}
out:
	log_msg("new_zone return %d", (int)ret);
	return(ret);
}

int free_zone(blkcnt_t znum)
{
	ino_t	n;
	int	ret;

	log_msg("free_zone called, znum = %u", znum);
	if (!is_zvalid(znum)) {
		log_msg("free_zone: znum out of range, znum = %u", znum);
		ret = -1;
		goto out;
	}

	n = znum >> 3;
	if ((sb.s_zmap[n] & (1 << (znum & 7))) == 0) {
		log_msg("free_zone: zone %d already freed", (int)znum);
		ret = -1;
	} else {
		sb.s_zmap[n] &= ~(1 << (znum & 7));
		ret = 0;
	}

out:
	log_msg("free_znode return %d", ret);
	return(ret);
}

int rd_zone(blkcnt_t zone_num, void *buf, size_t size)
{
	int	ret;
	blkcnt_t bnum;

	log_msg("rd_zone called, zone_num = %u", zone_num);
	if (!is_zvalid(zone_num)) {
		log_msg("rd_zone: znum out of range, znum = %u", zone_num);
		ret = -EINVAL;
		goto out;
	}
	if (buf == NULL) {
		log_msg("rd_zone: buf is NULL");
		ret = -EINVAL;
		goto out;
	}
	if (size != BLK_SIZE) {
		log_msg("rd_zone: size = %z, do not equals to %d",
				size, BLK_SIZE);
		ret = -EINVAL;
		goto out;
	}
	if ((bnum = zonenum2blknum(zone_num)) == 0) {
		log_msg("rd_zone: zone %u's block number is zero", zone_num);
		ret = -EINVAL;
		goto out;
	}
	ret = rd_blk(bnum, buf, size);
out:
	log_msg("rd_zone return %d", ret);
	return(ret);
}

int wr_zone(blkcnt_t zone_num, const void *buf, size_t size)
{
	int	ret;
	blkcnt_t bnum;

	log_msg("wr_zone called, zone_num = %u", zone_num);
	if (!is_zvalid(zone_num)) {
		log_msg("wr_zone: znum out of range, znum = %u", zone_num);
		ret = -1;
		goto out;
	}
	if (buf == NULL) {
		log_msg("wr_zone: buf is NULL");
		ret = -1;
		goto out;
	}
	if (size != BLK_SIZE) {
		log_msg("wr_zone: size = %z, do not equals to %d",
				size, BLK_SIZE);
		ret = -1;
		goto out;
	}
	if ((bnum = zonenum2blknum(zone_num)) == 0) {
		log_msg("wr_zone: zone %u's block number is zero", zone_num);
		ret = -1;
		goto out;
	}
	ret = wr_blk(bnum, buf, size);
out:
	log_msg("wr_zone return %d", ret);
	return(ret);
}

blkcnt_t inum2blknum(ino_t inum)
{
	blkcnt_t ret = 0;

	log_msg("inum2blknum called, inum = %u", inum);
	if (!is_ivalid(inum))
		log_msg("inum2blknum: inode %u is not valid", inum);
	else
		ret = (inum - 1) / INUM_PER_BLK + sb.s_1st_inode_block;
	log_msg("inum2blknum return %u", ret);
	return(ret);
}

blkcnt_t zonenum2blknum(blkcnt_t zone_num)
{
	blkcnt_t ret = 0;

	log_msg("zonenum2blknum called, zone_num = %u", zone_num);
	if (!is_zvalid(zone_num)) {
		log_msg("zonenum2blknum: zone %s is not valid", zone_num);
		goto out;
	}
	ret = (zone_num - 1) + sb.s_1st_zone_block;
out:
	log_msg("zonenum2blknum return %u", ret);
	return(ret);
}

blkcnt_t datanum2zonenum(ino_t inum, blkcnt_t data_num)
{
	blkcnt_t ret = 0, znum;
	struct d_inode inode;
	char	buf[BLK_SIZE];

	log_msg("datanum2zonenum called, inum = %u, data_num = %u", inum,
			data_num);
	if (!is_ivalid(inum)) {
		log_msg("datanum2zonenum: inum %u is not valid", inum);
		goto out;
	}
	if (!is_dvalid(data_num)) {
		log_msg("datanum2zonenum: data_num %u is not valid",
				data_num);
		goto out;
	}
	if (rd_inode(inum, &inode) < 0) {
		log_msg("datanum2zonenum: rd_inode error");
		goto out;
	}

	if (data_num < 6) {
		ret = inode.i_zones[data_num];
		goto out;
	}

	data_num -= 6;
	if (rd_zone(inode.i_zones[6], buf, sizeof(buf)) < 0) {
		log_msg("datanum2zonenum: rd_zone error for %u",
				inode.i_zones[6]);
		goto out;
	}
	if (data_num < ZNUM_PER_BLK) {
		ret = ((blkcnt_t *)buf)[data_num];
		goto out;
	}

	/* double indirect */
	if (rd_zone(inode.i_zones[7], buf, sizeof(buf)) < 0) {
		log_msg("datanum2zonenum: rd_zone error for %u",
				inode.i_zones[7]);
		goto out;
	}
	data_num -= ZNUM_PER_BLK;
	znum = ((blkcnt_t *)buf)[(data_num / ZNUM_PER_BLK)];
	if (rd_zone(znum, buf, sizeof(buf)) < 0) {
		log_msg("datanum2zonenum: rd_zone error for %u",
				znum);
		goto out;
	}
	ret = ((blkcnt_t *)buf)[data_num % ZNUM_PER_BLK];
out:
	log_msg("datanum2zonenum return %u", ret);
	return(ret);
}

int rd_blk(blkcnt_t blk_num, void *buf, size_t size)
{
	int	ret = -1;

	log_msg("rd_blk called, blk_num = %u", blk_num);
	if (!is_bvalid(blk_num)) {
		log_msg("rd_blk: block num %u is not valid", blk_num);
		ret = -EINVAL;
		goto out;
	}
	if (buf == NULL) {
		log_msg("rd_blk: buf is NULL");
		ret = -EINVAL;
		goto out;
	}
	if (size != BLK_SIZE) {
		log_msg("rd_blk: size = %u", size);
		ret = -EINVAL;
		goto out;
	}
	if (pread(sb.s_fd, buf, size, blk_num << BLK_SIZE_SHIFT) != size) {
		log_ret("rd_blk: pread error");
		ret = -errno;
		goto out;
	}
	ret = 0;
out:
	log_msg("rd_blk return %d", ret);
	return(ret);
}

int wr_blk(blkcnt_t blk_num, const void *buf, size_t size)
{
	int	ret = -1;

	log_msg("wr_blk called, blk_num = %u", blk_num);
	if (!is_bvalid(blk_num)) {
		log_msg("wr_blk: block num %u is not valid", blk_num);
		goto out;
	}
	if (buf == NULL) {
		log_msg("wr_blk: buf is NULL");
		goto out;
	}
	if (size != BLK_SIZE) {
		log_msg("wr_blk: size = %u", size);
		goto out;
	}
	if (pwrite(sb.s_fd, buf, size, blk_num << BLK_SIZE_SHIFT) != size) {
		log_ret("wr_blk: pwrite error");
		goto out;
	}
	ret = 0;
out:
	log_msg("wr_blk return %d", ret);
	return(ret);
}

int dir2inum(const char *dirpath, ino_t *inum)
{
	int	ret;
	struct m_inode inode;

	log_msg("dir2inum called, dirpath = %s", (dirpath == NULL ? "NULL" :
				dirpath));
	if ((ret = path2inum(dirpath, inum)) < 0) {
		log_msg("dir2inum: path2inum error");
		goto out;
	}
	if ((ret = rd_inode(*inum, (struct d_inode *)&inode)) < 0) {
		log_msg("dir2inum: rd_inode error for %u", *inum);
		goto out;
	}
	if (!UFS_ISDIR(inode.i_mode)) {
		ret = -ENOTDIR;
		goto out;
	}
	ret = 0;
out:
	log_msg("dir2inum return %d", ret);
	return(ret);
}

int add_entry(ino_t dirinum, const char *file, struct dir_entry *entry)
{
	int	ret, i;
	struct m_inode inode;
	blkcnt_t dnum, znum;
	char	buf[BLK_SIZE];
	struct dir_entry *de;

	log_msg("add_entry called, adding %s in %u", (file == NULL ? "NULL"
				: file), dirinum);
	if (!is_ivalid(dirinum)) {
		log_msg("add_entry: dirinum = %u is not valid", dirinum);
		ret = -EINVAL;
		goto out;
	}
	if (file == NULL) {
		log_msg("add_entry: file is NULL");
		ret = -EINVAL;
		goto out;
	}
	if (entry == NULL) {
		log_msg("add_entry: entry is NULL");
		ret = -EINVAL;
		goto out;
	}

	/*
	 * allocate a new inode and initialize it,
	 * then write it.
	 */
	if ((entry->de_inum = new_inode()) == 0) {
		log_msg("add_entry: new_inode return zero");
		ret = -ENOSPC;
		goto out;
	}
	memset(&inode, 0, sizeof(inode));
	inode.i_nlink = 1;
	inode.i_size = 0;
	if ((ret = wr_inode(&inode)) < 0) {
		log_msg("add_entry: wr_inode error");
		goto out;
	}

	/*
	 * find a available entry in parent directory. write new entry and
	 * update parent directory's inode.
	 */
	dnum = 0;
	while (1) {
		if ((znum = creat_zone(dirinum, dnum)) == 0) {
			log_msg("add_entry: creat_zone return 0 for %u",
					znum);
			ret = -ENOSPC;
			goto out;
		}
		if ((ret = rd_zone(znum, &buf, sizeof(buf))) < 0) {
			log_msg("add_entry: rd_zone error");
			goto out;
		}
		for (de = (struct dir_entry *)&buf, i = 0;
				i < ENTRYNUM_PER_BLK; i++)
			if (de[i].de_inum == 0)
				break;
		if (i < ENTRYNUM_PER_BLK)
			break;
	}
	/* find a available entry in directory */
	de[i].de_inum = entry->de_inum;
	strncpy(de[i].de_name, file, NAME_LEN);
	de[i].de_name[NAME_LEN] = 0;
	memcpy(&entry, &de[i], sizeof(entry));
	if ((ret = wr_zone(znum, &buf, sizeof(buf))) < 0) {
		log_msg("add_entry: wr_zone error");
		goto out;
	}
	/*
	 * update parent directory's inode.
	 */
	if ((ret = rd_inode(dirinum, (struct d_inode *)&inode)) < 0) {
		log_msg("add_entry: rd_inode error for %u", dirinum);
		goto out;
	}
	inode.i_size += sizeof(entry);
	if ((ret = wr_inode(&inode)) < 0) {
		log_msg("add_entry: wr_inode error");
		goto out;
	}
	ret = 0;

out:
	if (de->de_inum)
		free_inode(de->de_inum);
	log_msg("add_entry return %d", ret);
	return(ret);
}

int path2inum(const char *path, ino_t *inum)
{
	struct m_inode inode;
	struct dir_entry ent;
	char	file[NAME_LEN + 1];
	int	i, start, ret;

	log_msg("path2inum called, path = %s", (path == NULL ? "NULL" : 
				path));
	if (path == NULL) {
		ret = -EINVAL;
		goto out;
	}
	inode.i_ino = ROOT_INO;
	if ((ret = rd_inode(inode.i_ino, (struct d_inode *)&inode)) < 0) {
		log_msg("path2inum: rd_inode error");
		goto out;
	}

	i = 1;
	while (path[i]) {
		if (path[i] == '/')
			start = ++i;
		else
			start = i++;
		while (path[i] != '/' && path[i])
			i++;
		if (path[start] == 0) /* /usr/bin/vim/ */
			break;
		memcpy(file, path + start, i - start);
		file[i - start] = 0;
		if (srch_dir_entry(&inode, file, &ent) == 0) {
			ret = -ENOENT;
			log_msg("path2inum: srch_dir_entry return zero, "
					"file %s not found", file);
			goto out;
		}
		inode.i_ino = ent.de_inum;
		ret = rd_inode(inode.i_ino, (struct d_inode *)&inode);
		if (ret  < 0) {
			log_msg("path2inum: rd_inode error");
			goto out;
		}
	}
	*inum = inode.i_ino;
	ret = 0;

out:
	log_msg("path2inum return %d", ret);
	return(ret);
}

ino_t srch_dir_entry(const struct m_inode *par, const char *file,
		struct dir_entry *res)
{
	off_t	i;
	blkcnt_t dnum, znum;
	char	buf[BLK_SIZE];
	struct dir_entry *de;

	log_msg("srch_dir_entry called");
	if (par == NULL || file == NULL || res == NULL)
		log_msg("srch_dir_entry: arguments is NULL");
	log_msg("srch_dir_entry: par->i_ino = %u, file = %s", par->i_ino,
			file);
	
	i = 0;
	res->de_inum = 0;
	dnum = 0;
	while (i < par->i_size) {
		if ((znum = datanum2zonenum(par->i_ino, dnum)) == 0) {
			log_msg("srch_dir_entry: datanum2zonenum return "
					"zero for data %u", dnum);
			goto out;
		}
		if (rd_zone(znum, &buf, sizeof(buf)) < 0) {
			log_msg("srch_dir_entry: rd_zone error for data"
					" %u", znum);
			goto out;
		}
		for (de = (struct dir_entry *)buf; de < (struct dir_entry *)
				(buf + ENTRYNUM_PER_BLK) &&
				i < par->i_size; de++) {
			if (de->de_inum == 0)
				continue;
			i += sizeof(*de);
			if (strncmp(de->de_name, file, NAME_LEN) == 0) {
				res->de_inum = de->de_inum;
				strncpy(res->de_name, de->de_name, NAME_LEN);
				res->de_name[NAME_LEN] = 0;
				goto out;
			}
		}
		dnum++;
	}
out:
	log_msg("srch_dir_entry return %u", res->de_inum);
	return(res->de_inum);
}
/*
 * convert file mode and permission from ufs to 
 * UNIX.
 */
mode_t conv_fmode(mode_t mode)
{
	mode_t ret = mode & 0x1ff;
	if (UFS_ISREG(mode))
		ret |= S_IFREG;
	if (UFS_ISDIR(mode))
		ret |= S_IFDIR;
	return(ret);
}
