#pragma once
#include <stdint.h>
#include <sys/types.h>

// general constants
#define BLK_SZ (1024)
#define NBLKS (10)
// usage constants
#define USG_FULL_WRD ((uint64_t) - 1)
#define USG_WRD_SZ (sizeof(uint64_t))
#define USG_WRD_LEN (8)
#define USG_WRD_CAP (USG_WRD_LEN * USG_WRD_SZ)
#define USG_BLK_LEN (BLK_SZ / USG_WRD_SZ)
#define USG_BLK_CAP (USG_WRD_LEN * BLK_SZ)
// inode constants
#define INODE_SZ (128)
#define ILIST_LEN (BLK_SZ / INODE_SZ)
// dir constants
#define DIRENT_SZ (32)
#define DIR_LEN (BLK_SZ / DIRENT_SZ)
#define DIRNAME_LEN (DIRENT_SZ - sizeof(uint32_t))
// macros
#define CHECK_FREAD(A,M,N,F) if (fread(A,M,N,F) != (N)) return -1
#define CHECK_FWRITE(A,M,N,F) if (fwrite(A,M,N,F) != (N)) return -1

typedef struct super_blk {
	uint32_t num_blks;
	uint32_t num_ilists;
	uint32_t num_inodes;
	uint32_t num_iusgs;
	uint32_t num_busgs;
	uint32_t root_ino;
	char padding[BLK_SZ - 6 * sizeof(uint32_t)];
} super_blk_t;

typedef struct usg_blk {
	uint64_t words[USG_BLK_LEN];
} usg_blk_t;

typedef enum filetype {
	TYPE_FILE,
	TYPE_DIR,
} filetype_t;

typedef struct inode {
	filetype_t type;
  mode_t mode;
	uid_t user;
	gid_t group;
	short num_blks;
	// for now num_bytes is num_entries?
	uint32_t num_bytes;
	uint32_t blknos[NBLKS];
	time_t last_access;
	time_t last_modify;
	// TODO magic number?
	char padding[INODE_SZ - 80];
} inode_t;

typedef struct ilist_blk {
	inode_t inodes[ILIST_LEN];
} ilist_blk_t;

typedef struct dirent {
	uint32_t ino;
	char filename[DIRNAME_LEN];
} dirent_t;

typedef struct dir_blk {
	dirent_t dirents[DIR_LEN];
} dir_blk_t;

typedef struct data_blk {
	char data[BLK_SZ];
} data_blk_t;

typedef union block {
	super_blk_t super;
	ilist_blk_t ilist;
	dir_blk_t dir;
	data_blk_t data;
	usg_blk_t usg;
} block_t;

typedef struct fs {
	super_blk_t *super;
	usg_blk_t *iusgs;
	usg_blk_t *busgs;
	ilist_blk_t *ilists;
	block_t *data;
} fs_t;

extern fs_t fs;
extern uid_t user;
extern gid_t group;


int get_busg(uint32_t blkno);
int get_iusg(uint32_t ino);

// blocks
block_t *get_blk(uint32_t blkno);
uint32_t add_blk();
int blkdel(uint32_t blkno);

// inodes + files
inode_t make_inode(filetype_t type, mode_t mode);
inode_t *get_inode(uint32_t ino);
uint32_t add_inode(inode_t inode);
int filedel(uint32_t ino);

// filesystem
void make_fs(int num_blks, int num_ilists);
int read_fs(char *fn);
int write_fs(char *fn);
void free_fs();
