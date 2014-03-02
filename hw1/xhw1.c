#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h> //Used for oflags

#define __NR_xconcat	349	/* our private syscall number */

//Structure to pass multiple arguments to the system call
struct myargs
{
	char arg2[24][255];
	char outfile[255];
	unsigned int infile_count;
	int oflags;
	mode_t mode;
	unsigned int flags;
}args;

int main(int argc, char *argv[])
{
	int i,c,j,num,rc,argslen;
	int length;
        int index;
	int input_flags[20];		
	args.oflags=0;
	i=0;
	num=9;
	for(j=0;j<num;j++)
	{	
		input_flags[j]=0;
	}

	while ((c = getopt (argc, argv, "acteANPhm:")) != -1)
        switch (c)
        {
           case 'a':
		if(input_flags[1]==0)
		{
		        args.oflags = args.oflags | O_APPEND;
			input_flags[1]=1;
		}
             break;
           case 'c':
		if(input_flags[2]==0)
       	        {
		        args.oflags = args.oflags | O_CREAT;
			input_flags[2]=1;
		}
             break;
           case 't':
		if(input_flags[3]==0)
	        {     
			args.oflags = args.oflags | O_TRUNC;
	         	input_flags[3]=1;
		}
             break;
           case 'e':
		if(input_flags[4]==0)
		{
             		args.oflags = args.oflags | O_EXCL;
			input_flags[4]=1;
		}
             break;
           case 'A':
		if(input_flags[5]==0)
             	{
			args.flags = args.flags | 1<<5;
			input_flags[5]=1;
		}
             break;
           case 'N':
		if(input_flags[6]==0)
        	{
		        args.flags = args.flags | 1<<6;
			input_flags[6]=1;
		}
             break;
           case 'P':
		if(input_flags[7]==0)
		{
             		args.flags = args.flags | 1<<7;
			input_flags[7]=1;
		}
            break;
           case 'm':
		if(input_flags[8]==0)
		{
		     args.mode=strtol(optarg,0,8);
		     printf("Mode %d\n",args.mode);
        	     args.flags = args.flags | 1<<8;
		     input_flags[8]=1;
		}
             break;
           case 'h':
		if(input_flags[0]==0)
             printf("\n-----------------------------HELP----------------------------\n\nConcatenate two or more input files into a single output file.\nMode of usage : \n\n./xhw1 [flags] outfile infile1 infile2 ...\n\nFlags \n\t -a : append \n\t -c : create \n\t -e : excl \n\t -t: truncate \n\t -m : mode \n\t -N : return number of files -P return percentage of data written \n\t -m ARG : specifies mode\n\t -h : help\n-------------------------------------------------------------\n");
		input_flags[0]=1;
             break;
           case '?':
	     if (optopt == 'm')
             //fprintf (stderr, "Option -%m requires an argument.\n", optopt);
	     return 1;
           default:
             printf(" ");
        } 
	//printf ("mflag = %d, mvalue = %d\n", args.flags & 1<<8, args.mode);
	//If argv[optind]==NULL, it means that we only have flags and no output files
	if(argv[optind]!=NULL)
	{
		length=strlen(argv[optind]);
		strncpy(args.outfile,argv[optind],length);
		//printf("Args outfile is %s %d",args.outfile,length);
		args.infile_count=0;
        	for (index = optind+1; index < argc; i++, index++)
		{
			//printf("\n%d\n%d\n\n",optind,argc);
			length=strlen(argv[index]);
			strncpy(args.arg2[i],argv[index],length);

			//printf("Input file %d  :  %s\n",i+1,*args.arg2[i]);
			args.infile_count+=1;
			if(!(strcmp(args.outfile,args.arg2[i])))
			{
				printf("\nInput file same as output file\n");
				return 0;
			}
        		//printf ("\nInput file%d: %s\n", i+1, args.arg2[i]);
		}

			//*args.infile=argv[index];
			//*args.infile=*args.infile+1;
			//printf("%s",*args.infile);
		//printf("Mode: %d %d %s %d",!(args.flags & 1<< 8),chmod(argv[optind],args.mode),argv[optind],args.mode);
		if(!(args.flags & 1<<8))
		{
			
			args.mode=0644;
		}
	        /*else if (chmod (argv[optind],args.mode) < 0)
	        {
         		printf("\nWrong mode specified, Taking default mode 0644");
	        	args.mode=0644;
    	        }*/		
	}
	else
	{
		errno=EINVAL;
		printf("\nERROR: No input or output files specified. (errno=%d)\n\n",errno);
		return -EINVAL;
	}
	if(args.infile_count==0)
	{
		errno=EINVAL;
		printf("\nERROR: Inavlid arguments. Type option -h to get the correct format. (errno=%d)\n\n",errno);
		return -EINVAL;
	}
	//argslen - holds the size of the structure
	argslen=sizeof(args);
	//printf("Argslen is %d\n",argslen);
	rc=syscall(__NR_xconcat,(void *) &args, argslen);	
	//printf("\n\n----------SYSTEM CALL IMPLEMENTATION------------\n\n");
	if (rc == 0)
		printf("\nsyscall returned %d\n\n", rc);
	else
	{
		printf("\nsyscall returned %d (errno=%d)\n\n", rc, errno);
	}
	exit(rc);
	return 1;
}
