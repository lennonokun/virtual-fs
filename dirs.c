#include "data.h"
#include "files.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
typedef enum open_flags {
} open_flags_t;
*/

// TODO STRCPY

// todo ent should not be pointer

typedef struct dir {
	uint32_t ino;
	inode_t *inode;
	dir_blk_t *blk;
	int blk_num;
	int blk_off;
	int reset;
	dirent_t *ent;
} dir_t;

dir_t *cwd;

void resetdir(dir_t *dirp) {
	dirp->reset = 1;
}

int extenddir(dir_t *dirp) {
	// error if max size
	if (dirp->inode->num_blks == NBLKS)
		return -1;
	// add block
	uint32_t blkno = add_blk();
	dir_blk_t *blk = &(fs.data[blkno].dir);
	dirp->inode->blknos[dirp->inode->num_blks++] = blkno;
	// empty entries
	for (int i=0; i<DIR_LEN; i++)
		blk->dirents[i].ino = 0;
	return 0;
}

// now bad?
dir_t *opendir(uint32_t ino) {
	// if inode.num_bytes == 0?
	inode_t *inode = get_inode(ino);
	if (inode->type != TYPE_DIR)
		return NULL;
	if (!(mask_perms(ino) & 0111))
		return NULL;
	// create dirp
	dir_t *dirp = malloc(sizeof(dir_t));
	dirp->ino = ino;
	dirp->inode = inode;
	resetdir(dirp);
	return dirp;
}

dir_t *copydir(dir_t *dirp) {
	dir_t *out = malloc(sizeof(dir_t));
	*out = *dirp;
	return out;
}

void closedir(dir_t *dirp) {
	free(dirp);
}

/*
	iterate dirp to next entry
	returns 0 normally, -1 on error
*/
int iterdir(dir_t *dirp) {
	// reset
	// printf("ITER\n");
	// printf("blk_num: %d\n", dirp->blk_num);
	// printf("blk_off: %d\n", dirp->blk_off);
	// printf("blk_ent: %p\n", dirp->ent);

	if (dirp->reset) {
		if (dirp->inode->num_blks == 0)
			return -1;
		dirp->blk = &(fs.data[dirp->inode->blknos[0]].dir);
		dirp->blk_num = 0;
		dirp->blk_off = 0;
		dirp->ent = dirp->blk->dirents;
		dirp->reset = 0;
		return 0;
	}

	// error if past num_blks
	if (dirp->blk_num >= dirp->inode->num_blks)
		return -1;

	dirp->ent++;
	dirp->blk_off++;
	if (dirp->blk_off == DIR_LEN) {
		// update blk
		uint32_t blkno = dirp->inode->blknos[++dirp->blk_num];
		dirp->blk = &fs.data[blkno].dir;
		dirp->ent = dirp->blk->dirents;
		dirp->blk_off = 0;
	}

	if (dirp->blk_num >= dirp->inode->num_blks)
		return -1;

	return 0;
}

/*
	reads next filled entry of dirp
	returns 0 for normal, -1 for error
*/
int readdir(dir_t *dirp, int hidden) {
	while (iterdir(dirp) == 0)
		if ((dirp->ent->ino != 0) &&
				(hidden || dirp->ent->filename[0] != '.'))
			return 0;
	return -1;
}

/*
	reads dirp until name is found
	returns ino for normal, -1 for error
*/
int finddir(dir_t *dirp, char *name) {
	while (readdir(dirp, 1) == 0)
		if (strcmp(dirp->ent->filename, name) == 0)
			return 0;
	return -1;
}

/*
	add . and .. entries to cno
	returns 0 normally, -1 on error
*/
int populatedir(uint32_t cno, uint32_t pno) {
	dir_t *newdir = opendir(cno);
	if (newdir == NULL)
		return -1;

	extenddir(newdir);
	iterdir(newdir);
	strcpy(newdir->ent->filename, ".");
	newdir->ent->ino = cno;
	iterdir(newdir);
	strcpy(newdir->ent->filename, "..");
	newdir->ent->ino = pno;
	closedir(newdir);
	return 0;
}

// create root and populate
void make_root() {
	fs.super->root_ino = add_inode(make_inode(TYPE_DIR, 0777));
	// populate root with itself as its parent
	populatedir(fs.super->root_ino, fs.super->root_ino);
}

// returns 0 on error, new ino otherwise
int diradd(dir_t *dirp, char *name, filetype_t type, mode_t mode) {
	// extend if necessary
	if (dirp->inode->num_bytes == DIR_LEN * dirp->inode->num_blks)
		if (extenddir(dirp) != 0)
			return -1;

	/* 
	// TODO check
	// return -1 if already exists
	resetdir(dirp);
	if (finddir(dirp, name) == 0)
		return -1;
	printf("DIDN'T FIND DUP\n");
	*/

	// find empty entry
	resetdir(dirp);
	while (iterdir(dirp) == 0) {
		if (dirp->ent->ino == 0) {
			uint32_t ino = add_inode(make_inode(type, mode));
			dirp->blk->dirents[dirp->blk_off].ino = ino;
			strncpy(dirp->ent->filename, name, DIRNAME_LEN);
			dirp->inode->num_bytes++;
			dirp->ent->ino = ino;

			if (type == TYPE_DIR)
				populatedir(ino, dirp->ino);
			
			return ino;
		}
	}
	// full directory
	return 0;
}

int dirdel(dir_t *dirp, char *name) {
	resetdir(dirp);
	if (finddir(dirp, name) == -1)
		return -1;
	
	// TODO filedel
	if (filedel(dirp->ent->ino) == -1)
		return -1;

	dirp->ent->ino = 0;
	dirp->inode->num_bytes--;
	return 0;
}

char *finddir_ino(dir_t *dirp, uint32_t ino) {
	resetdir(dirp);
	while (iterdir(dirp) == 0)
		if (dirp->ent->ino == ino)
			return dirp->ent->filename;

	return NULL;
}

// hacky+slow, but i dont wanna make a dcache
int getpath(dir_t *dirp, char *out, int n) {
	if (n == 0)
		return -1;

	char names[256][DIRNAME_LEN];
	
	int i = 0;
	int sz = 0;
	dir_t *dirp2 = copydir(dirp);

	// todo reverse
	while(dirp2->ino != fs.super->root_ino) {
		if (i >= 256 || finddir(dirp2, "..") == -1)
			return -1;
		dir_t *dirp3 = opendir(dirp2->ent->ino);
		if (finddir_ino(dirp3, dirp2->ino) == 0)
			return -1;
		sz += strlen(dirp3->ent->filename) + 1;
		if (sz >= n)
			return -1;
		strcpy(names[i++], dirp3->ent->filename);
		closedir(dirp2);
		dirp2 = dirp3;
		resetdir(dirp2);
	}
	closedir(dirp2);

	// "/" if root
	if (i == 0) {
		if (n == 1) return -1;
		strcpy(out, "/");
		return 0;
	}
	
	int m=1;
	for (int j=i-1; j>=0; j--) {
		out[m++] = '/';
		strcpy(out+m, names[j]);
		m += strlen(names[j]);
	}
	out[m] = '\0';
	return 0;
}

/*
	returns ino of path
	returns 0 for error
*/
uint32_t read_abspath(char *path) {
	if (*(path++) != '/')
		return 0;

	uint32_t cur_ino = fs.super->root_ino;
	char *found;
	char *last = strrchr(path, '/');
	while((found = strchr(path, '/'))) {
		*found = '\0';
		dir_t *dirp = opendir(cur_ino);
		if (dirp == NULL)
			return 0;
		if (finddir(dirp, path))
			return 0;
		cur_ino = dirp->ent->ino;
		closedir(dirp);
		path = found;
	}
	
	return cur_ino;
}

