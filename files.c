#include "data.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define FDT_LEN 1024

typedef enum open_mode {
	OPEN_R,
	OPEN_W,
	OPEN_RW,
} open_mode_t;

// todo name fd?
typedef struct fdc {
	inode_t *inode;
	data_blk_t *blk;
	uint32_t blk_num;
	uint32_t blk_off;
	uint64_t tot_off;
	open_mode_t mode;
} fdc_t;

// TODO reserve fd 0???

typedef struct fdt {
	fdc_t fdcs[FDT_LEN];
	int min; // least unopened
} fdt_t;

fdt_t fdt;

void make_fdt() {
	for (int fd=0; fd<FDT_LEN; fd++)
		fdt.fdcs[fd].inode = NULL;
	fdt.min = 0;
}

mode_t mask_perms(uint32_t ino) {
	inode_t *inode = get_inode(ino);
	mode_t mask = 0007;
	if (inode->user == user)
		mask |= 0700;
	if (inode->group == group)
		mask |= 0070;

	return mask & inode->mode;
}

int open(uint32_t ino, open_mode_t mode) {
	if (ino == 0)
		return -1;
	
	// TODO check permissions
	mode_t mask = mask_perms(ino);
	if ((mode == OPEN_R || mode == OPEN_RW) && !(mask & 0700))
		return -1;
	if ((mode == OPEN_W || mode == OPEN_RW) && !(mask & 0070))
		return -1;

	for (int fd=fdt.min; fd<FDT_LEN; fd++) {
		if (fdt.fdcs[fd].inode == NULL) {
			// TODO what if empty?
			fdt.min = fd;
			fdc_t *fdc = &fdt.fdcs[fd];
			fdc->inode = get_inode(ino);
			fdc->blk = &get_blk(fdc->inode->blknos[0])->data;
			fdc->blk_num = 0;
			fdc->blk_off = 0;
			fdc->tot_off = 0;
			fdc->mode = mode;
			return fd;
		}
	}

	// no result
	return -1;
}

// TODO check if open?
int close(int fd) {
	if (fd < 0)
		return -1;

	fdt.fdcs[fd].inode = NULL;
	if (fd < fdt.min)
		fdt.min = fd;

	return 0;
}

// todo void *buf?
int read(int fd, char *buf, size_t n) {
	// printf("READING\n");
	if (fd < 0)
		return -1;
	fdc_t *fdc = &fdt.fdcs[fd];
	if (fdc->inode == NULL)
		return -1;
	if (fdc->mode != OPEN_R && fdc->mode != OPEN_RW)
		return -1;

	// dont read over
	// printf("n: %lu\n", n);
	// printf("%lu %lu %d\n", fdc->tot_off, n, fdc->inode->num_bytes);
	if (fdc->tot_off + n > fdc->inode->num_bytes)
		n = fdc->inode->num_bytes - fdc->tot_off;

	// read blocks
	size_t m = n;
	// printf("m: %lu\n", m);
	while (fdc->blk_off + m >= BLK_SZ) {
		// printf("m: %lu\n", m);
		// printf("data: %s\n", fdc->blk->data);
		/*
		printf("bnum: %d\n", fdc->blk_num);
		printf("boff: %d\n", fdc->blk_off);
		printf("toff: %lu\n", fdc->tot_off);
		printf("nbytes: %d\n", fdc->inode->num_bytes);
		*/
		memcpy(buf,
					 fdc->blk->data + fdc->blk_off,
					 BLK_SZ - fdc->blk_off);
		fdc->blk = &get_blk(fdc->inode->blknos[++fdc->blk_num])->data;
		m -= BLK_SZ - fdc->blk_off;
		buf += BLK_SZ - fdc->blk_off;
		fdc->tot_off += BLK_SZ - fdc->blk_off;
		fdc->blk_off = 0;
	}
	
	// read rest
	memcpy(buf, fdc->blk->data + fdc->blk_off, m);
	fdc->blk_off += m;
	fdc->tot_off += m;
	return n;
}

int write(int fd, char *buf, size_t n) {
	// printf("WRITING\n");
	if (fd < 0)
		return -1;

	fdc_t *fdc = &fdt.fdcs[fd];
	if (fdc->inode == NULL)
		return -1;
	if (fdc->mode != OPEN_W && fdc->mode != OPEN_RW)
		return -1;

	if (fdc->tot_off + n > BLK_SZ * NBLKS)
		n = BLK_SZ * NBLKS - fdc->tot_off;
	// printf("n: %lu\n", n);
	
	// add blocks
	int num_extra = (fdc->tot_off + n -
									 BLK_SZ * fdc->inode->num_blks - 1) / BLK_SZ + 1;
	// printf("extra: %d\n", num_extra);
	for (int i=0; i<num_extra; i++) {
		uint32_t blkno = add_blk();
		if (blkno == 0)
			return -1;
		fdc->inode->blknos[fdc->inode->num_blks++] = blkno;
	}
	// refresh block
	fdc->blk = &get_blk(fdc->inode->blknos[fdc->blk_num])->data;

	// write blocks
	size_t m = n;
	while (fdc->blk_off + m >= BLK_SZ) {
		// printf("m: %lu\n", m);
		// printf("bnum: %d\n", fdc->blk_num);
		// printf("blk: %p\n", fdc->blk);
		memcpy(fdc->blk->data + fdc->blk_off,
					 buf,
					 BLK_SZ - fdc->blk_off);
		fdc->blk = &get_blk(fdc->inode->blknos[++fdc->blk_num])->data;
		m -= BLK_SZ - fdc->blk_off;
		buf += BLK_SZ - fdc->blk_off;
		fdc->tot_off += BLK_SZ - fdc->blk_off;
		fdc->blk_off = 0;
	}

	// write rest
	// printf("bnum: %d\n", fdc->blk_num);
	// printf("blk: %p\n", fdc->blk);
	memcpy(fdc->blk->data + fdc->blk_off, buf, m);
	fdc->blk_off += m;
	fdc->tot_off += m;
	// TODO change to max, or only change at end
	if (fdc->tot_off > fdc->inode->num_bytes)
		fdc->inode->num_bytes = fdc->tot_off;
	return n;
}

// TODO add mode
typedef struct file {
	inode_t *inode;
	char buf[BLK_SZ];
	int blk_num;
	int blk_off;
	int num_bytes;
	int num_blocks;
} file_t;
