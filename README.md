Operating Systems [Prof. Erez Zadok]
======

Project 2: 

Unification File System.

Project 3:

Asynchronous and concurrent processing in linux kernel.

=======

HW1:

Concatenating or merging several files into one is a common task that many users perform in the user level.  Alas, it takes a lot of I/O and data copies between the user /bin/cat process and the kernel. My task was to create a new system call that can concatenate one or more files into a destination target file.

STEPS:

Create a Linux kernel module (in vanilla 3.2.y Linux that's in my HW1 GIT repository) that, when loaded into Linux, will support a new multi-mode system call called

	int sys_xconcat(void *args, int argslen)
	
The reason to use one system call for with a single void* is to use only ONE system call as if it's multiple ones, and it
would allow us to change the number and types of args we want to pass.  The "trick" would be that you'd be passing a void* generic pointer to the syscall, but inside of it you'd be packing all the args you need for different modes of this system call.  Note: "argslen" is the length of the void * buffer that the kernel should access.

Return value: number of bytes successfully concatenated on success; appropriate -errno on failure.

The system call opens each of the files listed in infiles, in order,read their content, and then concatenate the their content into the file named in outfile.  In Unix, this can be achieved by the cat(1) utility, for example:

$ /bin/cat file1 file2 file3 > newfile

The newly created file has a permission mode as defined in the 'mode' parameter. The oflags parameter behave exactly as the open-flags parameter to the open(2) syscall. The following describes how to run the sys call:

./xhw1 [flags] outfile infile1 infile2 ...

where flags is

-a: append mode (O_APPEND)
-c: O_CREATE
-t: O_TRUNC
-e: O_EXCL
-A: Atomic concat mode (extra credit only)
-N: return num-files instead of num-bytes written
-P: return percentage of data written out
-m ARG: set default mode to ARG (e.g., octal 755, see chmod(2) and umask(1))
-h: print short usage string
