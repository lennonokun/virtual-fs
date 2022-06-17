#include "dirs.h"
#include "files.h"
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

#define BLUE "\033[0;34m"
#define RED "\033[0;31m"
#define NOCOLOR "\033[0m"
#define WHITEBG "\033[47m"
#define LINE_MAX 1024
#define ARG_MAX 1024

/*
TODO
better help message
remove magic number 28
group implementation? using pwd.h
tot_off per iter or after?
either make inode smaller or bigger
tokenize deal with quotes and backslash
add truncate and append modes
add relative file reading
update date when writing to file
how to deal with pwd and being able to go up a dir
should dirs have a stack?
open dirs with fd instead
*/

int tokenize(char *out[], char *str, int n) {
	int i=0;
	int j=0;
	int is_new = 1;

	// loop until too many args, newline, or EOF
	while (str[j] && str[j] != '\n' && j < n-1) {
		if (str[j] == ' ' || str[j] == '\t') {
			// split space
			str[j] = '\0';
			is_new = 1;
		} else if (is_new) {
			// add argument
			out[i++] = str+j;
			is_new = 0;
		}
		j++;
	}

	str[j] = '\0';
	out[i] = NULL;
	return i;
}

void print_info() {
	printf("\n");
	printf("SUPER INFO\n");
	printf("* num_iusgs: %u\n", fs.super->num_iusgs);
	printf("* num_busgs: %u\n", fs.super->num_busgs);
	printf("* num_inodes: %u\n", fs.super->num_inodes);
	printf("* num_ilists: %u\n", fs.super->num_ilists);
	printf("* num_blks: %u\n", fs.super->num_blks);
	printf("\n");
	printf("CONST INFO\n");
	printf("* BLK_SZ: %d\n", BLK_SZ);
	printf("* NBLKS: %d\n", NBLKS);
	printf("* USG_WRD_SZ: %lu\n", USG_WRD_SZ);
	printf("* USG_WRD_LEN: %d\n", USG_WRD_LEN);
	printf("* USG_WRD_CAP: %lu\n", USG_WRD_CAP);
	printf("* USG_BLK_LEN: %lu\n", USG_BLK_LEN);
	printf("* USG_BLK_CAP: %d\n", USG_BLK_CAP);
	printf("* INODE_SZ %d\n", INODE_SZ);
	printf("* ILIST_LEN %d\n", ILIST_LEN);
	printf("* DIRENT_SZ: %d\n", DIRENT_SZ);
	printf("* DIR_LEN: %d\n", DIR_LEN);
}

void populate(int n, int m) {
	char buf[m];
	for (int i=0; i<m; i++)
		buf[i] = '0' + i % 10;

	dir_t *dirp = opendir(fs.super->root_ino);
	for (int i=0; i<n; i++) {
		char name[DIRNAME_LEN];

		// add data file
		sprintf(name, "data_%d", i);
		int ino = diradd(dirp, name, TYPE_FILE, 0777);
		// write buf
		int fd = open(ino, OPEN_W);
		write(fd, buf, m);
		close(fd);

		// add dir
		sprintf(name, "dir_%d", i);
		ino = diradd(dirp, name, TYPE_DIR, 0777);
		// recurse dir
		closedir(dirp);
		dirp = opendir(ino);
	}
	closedir(dirp);
}

void visualize() {
	printf("\nBUSGS\n");
	for (uint32_t bno=0; bno < 20; bno++)
		printf("%u: %d\n", bno, get_busg(bno));
	printf("IUSGS\n");
	for (uint32_t ino=0; ino < 20; ino++)
		printf("%u: %d\n", ino, get_busg(ino));
}

int main(int argc, char *argv[]) {
	char *help_msg = "Usage: ./ifs [-u <userid>] [-f <drive>]\n";
	int opt;
	int opt_u = 0;
	int opt_g = 0;
	int opt_f = 0;
	uid_t arg_u;
	char *arg_f;

	// parse flags
	while ((opt = getopt(argc, argv, ":u:f:")) != -1) {
		if (opt == 'u') {
			opt_u++;
			arg_u = atoi(optarg);
		} else if (opt == 'f') {
			opt_f++;
			arg_f = optarg;
		} else if (opt == '?') {
			printf("opt: %d, optopt: %c, optarg: %s", opt, optopt, optarg);
			fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
			fprintf(stderr, "%s", help_msg);
			return -1;
		}
	}

	// error or help
	if (argc == 1) {
		fprintf(stderr, "%s", help_msg);
		return -1;
	} else if (opt_u < 1 || opt_f < 1) {
		fprintf(stderr, "Error: Not enough flags specified\n");
		fprintf(stderr, "%s", help_msg);
		return -1;
	} else if (opt_u > 1 || opt_f > 1) {
		fprintf(stderr, "Error: Too many flags specified\n");
		fprintf(stderr, "%s", help_msg);
		return -1;
	} else if (optind != argc) {
		fprintf(stderr, "Error: Too many arguments specified\n");
		fprintf(stderr, "%s", help_msg);
		return -1;
	}

	// printf("Success: opt_u='%d', opt_f='%d'\n", opt_u, opt_f);
	user = arg_u;
	group = arg_u;
	if (read_fs(arg_f) == -1) {
		printf("MAKING\n");
		make_fs(4083, 12);
		make_root();
		populate(5, 20);
	}

	make_fdt();
	cwd = opendir(fs.super->root_ino);

	char line[LINE_MAX];
	char *args[ARG_MAX];
	int cont = 1;
	while(cont) {
		printf("%s>>>%s ", BLUE, NOCOLOR);
		if (!fgets(line, LINE_MAX, stdin))
			break;
		int n = tokenize(args, line, ARG_MAX);
		if (n == 0) {
			continue;
		} else if (n == 1 && strcmp(args[0], "exit") == 0) {
			cont = 0;
		} else if (strcmp(args[0], "ls") == 0) {
			// TODO ll, ls -l
			int hidden = n == 2 && !strcmp(args[1], "-a");
			resetdir(cwd);
			while (readdir(cwd, hidden) == 0)
				printf("%s\n", cwd->ent->filename);
		} else if (strcmp(args[0], "whoami") == 0) {
			printf("%d\n", user);
		} else if (strcmp(args[0], "pwd") == 0) {
			// TODO better
			char buf[256];
			getpath(cwd, buf, 256);
			printf("%s\n", buf);
		} else if (strcmp(args[0], "touch") == 0) {
			if (n < 2) {
				printf("touch: not enough arguments\n");
			} else {
				diradd(cwd, args[1], TYPE_FILE, 0777);
			}
		} else if (strcmp(args[0], "mkdir") == 0) {
			if (n < 2) {
				printf("mkdir: not enough arguments\n");
			} else {
				diradd(cwd, args[1], TYPE_DIR, 0777);
			}
		} else if (strcmp(args[0], "cd") == 0) {
			resetdir(cwd);
			if (n < 2) {
				printf("cd: not enough arguments\n");
			} else if (finddir(cwd, args[1]) == -1) {
				printf("cd: directory '%s' not found\n", args[1]);
			} else if (get_inode(cwd->ent->ino)->type != TYPE_DIR) {
				printf("cd: '%s' is not a directory\n", args[1]);
			} else {
				int ino = cwd->ent->ino;
				closedir(cwd);
				cwd = opendir(ino);
			}
		} else if (strcmp(args[0], "cat") == 0) {
			resetdir(cwd);
			if (n < 2) { 
				printf("cat: not enough arguments\n");
			} else if (finddir(cwd, args[1]) == -1) {
				printf("cat: file '%s' not found\n", args[1]);
			} else {
				int fd = open(cwd->ent->ino, OPEN_R);
				char buf[1024];
				int m;
				while (1) {
					int m2 = read(fd, buf, 1023);
					if (m2 <= 0)
						break;
					m = m2;
					buf[m] = '\0';
					printf("%s", buf);
				}
				if (buf[m-1] != '\n')
					printf("%s%%%s\n", WHITEBG, NOCOLOR);
			}
		} else if (strcmp(args[0], "write") == 0) {
			resetdir(cwd);
			if (n < 2) { 
				printf("write: not enough arguments\n");
			} else if (finddir(cwd, args[1]) == -1) {
				printf("write: file '%s' not found\n", args[1]);
			} else {
				int fd = open(cwd->ent->ino, OPEN_W);
				char line2[LINE_MAX];
				// using eof is better than SIGINT
				printf("%s> %s", BLUE, NOCOLOR);
				while (fgets(line2, LINE_MAX, stdin)) {
					write(fd, line2, strlen(line2));
					printf("%s> %s", BLUE, NOCOLOR);
				}

				if (line2[strlen(line2)-1] != '\n')
					printf("\n");
				else
					printf("\b\b");

				clearerr(stdin);
				close(fd);
			}
		} else if (strcmp(args[0], "stat") == 0)  {
			resetdir(cwd);
			if (n < 2) { 
				printf("stat: not enough arguments\n");
			} else if (finddir(cwd, args[1]) == -1) {
				printf("stat: file '%s' not found.\n", args[1]);
			} else {
				// TODO TYPE_DIR
				inode_t *inode = get_inode(cwd->ent->ino);
				printf("Name: '%s'\n", args[1]);
				printf("Type: %s\n", inode->type == TYPE_FILE
							 ? "regular" : "directory");
				printf("Size: %u\n", inode->num_bytes);
				printf("Blocks: %u\n", inode->num_blks);
				printf("User: %u\n", inode->user);
				printf("Group: %u\n", inode->group);
				// TODO pretty print
				printf("Permissions: %o\n", inode->mode);
				char buf[32];
				strftime(buf, 32, "%Y-%m-%d %H:%M:%S",
								 localtime(&inode->last_access));
				printf("Access: %s\n", buf);
				strftime(buf, 32, "%Y-%m-%d %H:%M:%S",
								 localtime(&inode->last_modify));
				printf("Modify: %s\n", buf);
			}
		} else if (strcmp(args[0], "tree") == 0) {
			// TODO for now just do cwd
			char decor[1024];
			int m = 2;
			decor[0] = decor[1] = ' ';
			decor[2] = '\0';
			dir_t *stack[64];
			int n = 0;
			resetdir(cwd);
			stack[0] = cwd;

			printf(".\n");
			// TODO reopen cwd copy?
			while (n >= 0) {
				if (readdir(stack[n], 0) == -1) {
					closedir(stack[n]);
					decor[m-2] = '\0';
					n--;
					m -= 2;
					continue;
				}

				printf("%s%s\n", decor, stack[n]->ent->filename);
				inode_t *inode = get_inode(stack[n]->ent->ino);
				if (inode->type == TYPE_DIR) {
					stack[n+1] = opendir(stack[n]->ent->ino);
					decor[m] = decor[m+1] = ' ';
					decor[m+2] = '\0';
					n++;
					m += 2;
				}
			}
		} else {
			printf("invalid command '%s'\n", args[0]);
		}
	}
	
	// read entries
	/*
	printf("\n");
	resetdir(dirp);
	for (int i=0; i<n; i++) {
		if (readdir(dirp, 0) == 0) {
			printf("fileno: %d\n", i);
			printf("ino: %u\n", dirp->ent->ino);
			printf("filename: \"%s\"\n", dirp->ent->filename);
		}
	}
	*/

	// remove entries
	/*
	for (int i=0; i<n; i++) {
		char name[28];
		sprintf(name, "test %d", i);
		if (dirdel(dirp, name) == 0)
			printf("deleted %s\n", name);
	}
	*/

	print_info();
	// visualize();
	write_fs(arg_f);
	
	return 0;
}
