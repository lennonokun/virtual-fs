#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

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

// TODO keep current search in super

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

fs_t fs;
uid_t user;
gid_t group;

/*
	returns pointer to blk at blkno
	TODO bounds check?
*/
block_t *get_blk(uint32_t blkno) {
	return &fs.data[blkno];
}

int get_busg(uint32_t blkno) {
	uint32_t bno = blkno / USG_BLK_CAP;
	uint32_t wno = (blkno % USG_BLK_CAP) / USG_WRD_CAP;
	uint32_t off = blkno % USG_WRD_CAP;
	return (fs.busgs[bno].words[wno] & (1llu << off)) != 0;
}

int get_iusg(uint32_t ino) {
	uint32_t bno = ino / USG_BLK_CAP;
	uint32_t wno = (ino % USG_BLK_CAP) / USG_WRD_CAP;
	uint32_t off = ino % USG_WRD_CAP;
	return (fs.iusgs[bno].words[wno] & (1llu << off)) != 0;
}

/*
	adds new block
	returns new bno normally
	returns 0 on error
*/
uint32_t add_blk() {
	for (uint32_t blkno = 0; blkno < fs.super->num_busgs; blkno++) {
		uint64_t *words = fs.busgs[blkno].words;
		for (uint32_t wordno = 0; wordno < USG_BLK_LEN; wordno++) {
			uint64_t word = words[wordno];
			// check if not all full
			if (word != USG_FULL_WRD) {
				// find empty
				for (uint32_t off = 0; off < USG_WRD_CAP; off++) {
					uint32_t out = off + USG_WRD_CAP * wordno
														 + USG_BLK_CAP * blkno;
					if (!(word & (1llu << off)) & (out < fs.super->num_blks)) {
						// mark used and return
						fs.busgs[blkno].words[wordno] |= 1llu << off;
						return out;
					}
				}
			}
		}
	}
	return 0;
}

/*
	create inode with filetype and mode
*/
inode_t make_inode(filetype_t type, mode_t mode) {
	inode_t inode;
	inode.type = type;
	inode.mode = mode;
	inode.user = user;
	inode.group = group;
	inode.num_blks = 0;
	inode.num_bytes = 0;
	inode.last_access = time(NULL);
	inode.last_modify = time(NULL);
	return inode;
}

/*
	get pointer to inode at ino
*/
inode_t *get_inode(uint32_t ino) {
	return &(fs.ilists[ino/ILIST_LEN].inodes[ino % ILIST_LEN]);
}

/*
	adds inodes into inode lists
	returns new ino normally
	returns 0 on error
*/
uint32_t add_inode(inode_t inode) {
	for (uint64_t blkno = 0; blkno < fs.super->num_iusgs; blkno++) {
		uint64_t *words = fs.iusgs[blkno].words;
		for (uint64_t wordno=0; wordno < USG_BLK_LEN; wordno++) {
			uint64_t word = words[wordno];
			// check if not all full
			if (word != USG_FULL_WRD) {
				// find empty
				for (uint64_t off = 0; off < USG_WRD_CAP; off++) {
					uint32_t out = off + USG_WRD_CAP * wordno + USG_BLK_CAP * blkno;
					if (!(word & (1lu << off)) & (out < fs.super->num_inodes)) {
						// mark used and return
						fs.iusgs[blkno].words[wordno] |= 1lu << off;
						*get_inode(out) = inode;
						// printf("NEW INODE: %d\n", out);
						// printf("bno %lu wno %lu off %lu\n", blkno, wordno, off);
						return out;
					}
				}
			}
		}
	}
	printf("NO NEW INODE\n");
	return 0;
}

/*
	mark block at blkno unused
	returns 0 if successful
	returns -1 otherwise
*/
int blkdel(uint32_t blkno) {
	if (blkno == 0)
		return -1;

	uint32_t usgno = blkno / USG_BLK_CAP;
	uint32_t wrdno = (blkno % USG_BLK_CAP) / USG_WRD_CAP;
	uint32_t off = blkno % USG_WRD_CAP;
	// error if already unused
	if (!(fs.busgs[usgno].words[wrdno] & (1 << off)))
		return -1;

	fs.busgs[usgno].words[wrdno] ^= (1 << off);
	return 0;
}

/*
	delete file at ino
	return 0 if successful
	return -1 otherwise
*/
int filedel(uint32_t ino) {
	if (ino == 0)
		return -1;

	inode_t *inode = get_inode(ino);
	if (inode->type != TYPE_FILE)
		return -1;

	// remove busgs
	for (int i=0; i<inode->num_blks; i++)
		if (blkdel(inode->blknos[i]) == -1)
			return -1;

	// remove iusg
	uint32_t usgno = ino / USG_BLK_CAP;
	uint32_t wrdno = (ino % USG_BLK_CAP) / USG_WRD_CAP;
	uint32_t off = ino % USG_WRD_CAP;
	if (!(fs.iusgs[usgno].words[wrdno] & (1 << off)))
		return -1;

	fs.iusgs[usgno].words[wrdno] ^= (1 << off);
	return 0;
}

void make_fs(int num_blks, int num_ilists) {
	// create super
	fs.super = malloc(BLK_SZ);
	fs.super->num_blks = num_blks;
	fs.super->num_ilists = num_ilists;
	fs.super->num_inodes = ILIST_LEN * num_ilists;
	fs.super->num_busgs =
		(num_blks - 1) / 8 / BLK_SZ + 1;
	fs.super->num_iusgs =
		(fs.super->num_inodes - 1) / 8 / BLK_SZ + 1 ;
	fs.super->root_ino = 1;
	// allocate usgs, ilists, and data
	fs.busgs = calloc(BLK_SZ, fs.super->num_busgs);
	fs.iusgs = calloc(BLK_SZ, fs.super->num_iusgs);
	fs.ilists = malloc(BLK_SZ * num_ilists);
	fs.data = malloc(BLK_SZ * num_blks);
	// set inode 0 and 1 to used (0 reserved as null, 1 for root)
	fs.iusgs[0].words[0] = 0b1;
	// set block 0 to be used (0 reserved as null)
	fs.busgs[0].words[0] = 0b1;
}

int read_fs(char *fn) {
	FILE *fp = fopen(fn, "r");
	if (!fp) return -1;

	fs.super = malloc(BLK_SZ);
	CHECK_FREAD(fs.super, BLK_SZ, 1, fp);
	fs.iusgs = malloc(BLK_SZ * fs.super->num_iusgs);
	CHECK_FREAD(fs.iusgs, BLK_SZ, fs.super->num_iusgs, fp);
	fs.busgs = malloc(BLK_SZ * fs.super->num_busgs);
	CHECK_FREAD(fs.busgs, BLK_SZ, fs.super->num_busgs, fp);
	fs.ilists = malloc(BLK_SZ * fs.super->num_ilists);
	CHECK_FREAD(fs.ilists, BLK_SZ, fs.super->num_ilists, fp);
	fs.data = malloc(BLK_SZ * fs.super->num_blks);
	CHECK_FREAD(fs.data, BLK_SZ, fs.super->num_blks, fp);

	return fclose(fp);
}

int write_fs(char *fn) {
	FILE *fp = fopen(fn, "w");
	if (!fp) return -1;
	CHECK_FWRITE(fs.super, BLK_SZ, 1, fp);
	CHECK_FWRITE(fs.iusgs, BLK_SZ, fs.super->num_iusgs, fp);
	CHECK_FWRITE(fs.busgs, BLK_SZ, fs.super->num_busgs, fp);
	CHECK_FWRITE(fs.ilists, BLK_SZ, fs.super->num_ilists, fp);
	CHECK_FWRITE(fs.data, BLK_SZ, fs.super->num_blks, fp);
	return fclose(fp);
}

void free_fs() {
	free(fs.super);
	free(fs.iusgs);
	free(fs.busgs);
	free(fs.ilists);
	free(fs.data);
}
