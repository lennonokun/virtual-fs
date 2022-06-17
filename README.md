# virtual-fs

## Overview

This is a virtual file system written in C for a challenge problem in
my System Programming course (CS392).

## How to Use

### Running

Create the binary by running `make` in the source directory. Then, run
`./vfs -u <UID> -f <FILE>` to start the virtual shell as UID and storing the file system in FILE. Currently, vfs populates the file system for testing purposes. 

### Commands

The virtual shell tries to emulate basic unix commands.
* `exit`: exit the virtual shell and save the virtual file system.
* `whoami`: prints the current uid.
* `pwd`: prints the current working directory.
* `touch <FNAME>`: creates empty file FNAME in the cwd.
* `mkdir <DNAME>`: creates empty directoy DNAME in the cwd.
* `ls [-a]`: list files in the cwd (including dotfiles with -a).
* `cd DNAME`: changes directory to DNAME.
* `cat FNAME`: prints the contents of FNAME.
* `write FNAME`: prompts the user to write to the contents of FNAME,
  quit with ctrl+C.
* `stat FNAME`: prints the status of FNAME.
* `tree`: recursively prints the contents of the cwd as a tree.

