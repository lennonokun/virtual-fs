ifs: ifs.c data.c data.h files.c files.h dirs.c dirs.h
	gcc -g -O0 -Wall ifs.c files.c data.c dirs.c -o ifs

clean:
	rm ifs

