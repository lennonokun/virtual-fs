#pragma once
#include "data.h"
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

typedef struct fdt {
	fdc_t fdcs[FDT_LEN];
	int fd_min; // least unopened
} fdt_t;

extern fdt_t fdt;

void make_fdt();
mode_t mask_perms(uint32_t ino);
int open(uint32_t ino, open_mode_t mode);
int close(int fd);
int read(int fd, char *buf, size_t n);
int write(int fd, char *buf, size_t n);

// TODO add mode
typedef struct file {
	inode_t *inode;
	char buf[BLK_SZ];
	int blk_num;
	int blk_off;
	int num_bytes;
	int num_blocks;
} file_t;
