#include "files.h"

typedef struct dir {
	uint32_t ino;
	inode_t *inode;
	dir_blk_t *blk;
	int blk_num;
	int blk_off;
	int reset;
	dirent_t *ent;
} dir_t;

extern dir_t *cwd;
	
void resetdir(dir_t *dirp);
int extenddir(dir_t *dirp);
dir_t *opendir(uint32_t ino);
void closedir(dir_t *dirp);
int iterdir(dir_t *dirp);
int readdir(dir_t *dirp, int hidden);
int finddir(dir_t *dirp, char *name);
int populatedir(uint32_t cno, uint32_t pno);
void make_root();
int diradd(dir_t *dirp, char *name, filetype_t type, mode_t mode);
int dirdel(dir_t *dirp, char *name);
int getpath(dir_t *dirp, char *out, int n);
uint32_t read_abspath(char *path);
