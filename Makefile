vfs: vfs.c data.c data.h files.c files.h dirs.c dirs.h
	gcc -g -O0 -Wall vfs.c files.c data.c dirs.c -o vfs

clean:
	rm ifs

