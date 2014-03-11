#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h> //Used for oflags

#define __NR_xconcat    349     /* our private syscall number */

//Structure to pass multiple arguments to the system call
struct myargs
{
        char **arg2;
        char *outfile;
        unsigned int infile_count;
        int oflags;
        mode_t mode;
        unsigned int flags;
}args;

/*     ********************************* VARIABLES USED ******************************

        inp_dec - Stores the ascii to integer value of mode
        i       - Used for iteration
        c`      - To store the value returned by getopt
        inp     - To validate the octal value of mode
        rc      - Store the return value of the system call
        argslen - Holds the argument length to be passed to the sys call for validation
        length  - Stores the string length
        index   -
*/

int main(int argc, char *argv[])
{
        //mode_t mode_val;
        int inp_dec,i,c,inp,rc,argslen,length,set=0;
        int index;
        int input_flags[10];
        int return_value=1;
        args.oflags=0;
        for(i=0;i<9;i++)
        {
                input_flags[i]=0;
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
                        if(input_flags[7]==1)
                        {
                                printf("Error: Incompatible flags specified (-N and -P).\nUse -h for help.\n");
                                return_value = EINVAL;
                                goto out;
                        }
                        args.flags = args.flags | 1<<6;
                        input_flags[6]=1;
                }
             break;
           case 'P':
                if(input_flags[7]==0)
                {
                        if(input_flags[6]==1)
                        {
                                printf("Error: Incompatible flags specified (-N and -P).\nUse -h for help.\n");
                                return_value = EINVAL;
                                goto out;
                        }
                        args.flags = args.flags | 1<<7;
                        input_flags[7]=1;
                }
            break;
           case 'm':
                if(input_flags[8]==0)
                {
                        inp_dec=atoi(optarg);
                        inp=inp_dec%10;
                        if(inp_dec>777)
                        {
                                printf("Error: Wrong mode specified1.\nEnter mode in octal xxx.\n");
                                return_value = EINVAL;
                                goto out;
                        }
                        else if(inp>=0 && inp <8)
                        {
                                inp=inp_dec/10;
                                inp=inp%10;
                                if( inp>=0 && inp <8)
                                {
                                        inp=inp%10;
                                        if(inp>=0 && inp<8)
                                        {
                                             args.mode=strtol(optarg,0,8);
                                             //mode_val=umask(0);
                                        }
                                        else
                                        {
                                                printf("Error: Wrong mode specified2.\nEnter mode in octal xxx.\n");
                                                return_value = EINVAL;
                                                goto out;
                                        }
                                }
                                else
                                {
                                        printf("Error: Wrong mode specified3.\nEnter mode in octal xxx.\n");
                                        return_value = EINVAL;
                                        goto out;
                                }

                        }
                        else
                        {
                                printf("Error: Wrong mode specified4.\nEnter mode in octal xxx.\n");
                                return_value = EINVAL;
                                goto out;
                        }
                }

                //printf("Mode %d\n",args.mode);
                args.flags = args.flags | 1<<8;
                input_flags[8]=1;
                break;
           case 'h':
                if(input_flags[0]==0)
             printf("\n-----------------------------HELP----------------------------\n\nConcatenate two or more input files into a single output file.\nMode of usage : \n\n./xhw1 [flags] outfile infile1 infile2 ...\n\nFlags \n\t -a : append \n\t -c : create \n\t -e : excl \n\t -t: truncate \n\t -m : mode \n\t -N : return number of files \n\t -P return percentage of data written \n\t -m ARG : specifies mode (OCTAL 777)\n\t -h : help\n-------------------------------------------------------------\n\n");
                input_flags[0]=1;
                return 1;
             break;
           case '?':
             if (optopt == 'm')
             {
                printf ("ERROR: Option -m requires an argument.\n");
                return_value = EINVAL;
                goto out2;
             }
             else
             {
                printf("ERROR: Invalid options passed. Use -h for help.\n");
                return_value = EINVAL;
                goto out2;
             }
           default:
        printf(" ");
       }

        //printf ("mflag = %d, mvalue = %d\n", args.flags & 1<<8, args.mode);
        //If argv[optind]==NULL, it means that we only have flags and no output files
        if(argv[optind]!=NULL)
        {
                args.outfile=(char *) malloc (sizeof(char*));
                if (!args.outfile)
                {
                        printf("\nERROR allocating memory \n");
                        return_value= ENOMEM;
                        goto out2;
                }

                length=strlen(argv[optind]);

                if(length>=255)
                {
                        printf("ERROR: Outfile too long. Can be only 255 characters.\n");
                        return_value = EINVAL;
                        goto out1;
                }

                strncpy(args.outfile,argv[optind],length);

                args.infile_count=0;

                for(index=optind+1;index<argc;index++)
                        args.infile_count+=1;

                if(args.infile_count>10)
                {
                        errno=EINVAL;
                        printf("ERROR: Only 10 input files allowed. (errno=%d)\n\n",errno);
                        return_value = EINVAL;
                        goto out1;
                }

                args.arg2=(char**)malloc((sizeof(char *)*args.infile_count));

                for(i=0;i<args.infile_count;i++)
                {
                        args.arg2[i]=(char *) malloc (sizeof(char*)*255);
                        if (!args.arg2[i])
                        {
                                printf("\nERROR allocating memory \n");
                                return_value= ENOMEM;
                                goto out;
                        }
                }

                args.infile_count=0;

                for (index = optind+1,i=0; index < argc; i++, index++)
                {

                        //printf("\n%d\n%d\n\n",optind,argc);
                        length=strlen(argv[index]);
                        if(length>=255)
                        {
                                printf("\nERROR: Input file too long. Can be only 255 characters.\n");
                                return_value = EINVAL;
                                goto out;
                        }
                        strncpy(args.arg2[i],argv[index],length);
                        //printf("Input file %d  :  %s\n",i+1,*args.arg2[i]);
                        args.infile_count+=1;

                        if(!(strcmp(args.outfile,args.arg2[i])))
                        {
                                        printf("\nERROR: Input file same as output file\n");
                                        return_value = EPERM;
                                        goto out;
                        }
                        //printf ("\nInput file%d: %s\n", i+1, args.arg2[i]);
                }

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
                return_value = EINVAL;
                goto out;
        }
        if(args.infile_count==0)
        {
                errno=EINVAL;
                printf("\nERROR: Inavlid arguments. Type option -h to get the correct format. (errno=%d)\n\n",errno);
                return_value = EINVAL;
                goto out;
        }

        //***************TO PRINT THE ARGUMENTS OF void * **************************************************************

        //printf("ARGS: \nOutfile %s\n",args.outfile);
        //for(i=0;i<args.infile_count;i++)
        //      printf("Infile %d : %s\n",i,args.arg2[i]);
        //printf("Mode %d\noflag: %d\ninfile count : %d\nflag : %d\n",args.mode,args.oflags,args.infile_count,args.flags);

        //printf("\nHERE..... !!\n");

        //argslen - holds the size of the structure

        //****************************************************************************************************************

        argslen=sizeof(args);

        //printf("Argslen is %d\n",argslen);

        rc=syscall(__NR_xconcat,(void *) &args, argslen);

        //printf("\n\n----------SYSTEM CALL IMPLEMENTATION------------\n\n");

        if (rc < 0)
        {
                return_value=errno;
                switch(errno)
                {
                case 1:
                        printf("\nERROR: Operation not permitted \n\n");
                        break;
                case 2:
                        printf("\nERROR: No such file or directory\n\n");
                        break;
                case 3:
                        printf("\nERROR: No such process\n\n");
                        break;
                case 4:
                        printf("\nERROR: Interrupted system call\n\n");
                        break;
                case 5:
                        printf("\nERROR: I/O Error\n\n");
                        break;
                case 6:
                        printf("\nERROR: No such device or address\n\n");
                        break;
                case 7:
                        printf("\nERROR: Argument list too long\n\n");
                        break;
                case 8:
                        printf("\nERROR: Exec format error\n\n");
                        break;
                case 9:
                        printf("\nERROR: Bad file number\n\n");
                        break;
                case 11:
                        printf("\nERROR: Try again\n\n");
                        break;
                case 12:
                        printf("\nERROR: Out of memory\n\n");
                        break;
                case 13:
                        printf("\nERROR: Permission denied\n\n");
                        break;
                case 14:
                        printf("\nERROR: Bad address\n\n");
                        break;
                case 15:
                        printf("\nERROR: Block device required\n\n");
                        break;
                case 16:
                        printf("\nERROR: Device or resource busy\n\n");
                        break;
                case 17:
                        printf("\nERROR: File exists\n\n");
                        break;
                case 18:
                        printf("\nERROR: Cross- device link\n\n");
                        break;
                case 19:
                        printf("\nERROR: No such device\n\n");
                        break;
                case 20:
                        printf("\nERROR: Not a directory\n\n");
                        break;
                case 21:
                        printf("\nERROR: Is a directory\n\n");
                        break;
                case 22:
                        printf("\nERROR: Invalid argument\n\n");
                        break;
                case 23:
                        printf("\nERROR: File table overflow\n\n");
                        break;
                case 24:
                        printf("\nERROR: Too many open files\n\n");
                        break;
                case 26:
                        printf("\nERROR: Text file busy\n\n");
                        break;
                case 27:
                        printf("\nERROR: File too large\n\n");
                        break;
                case 28:
                        printf("\nERROR: No space left on device\n\n");
                        break;
                case 29:
                        printf("\nIllegal seek\n\n");
                        break;
                case 30:
                        printf("\nRead-only file system\n\n");
                        break;
                case 31:
                        printf("\nToo many links\n\n");
                        break;
                default:
                        printf("\nERROR\n\n");
                        break;
                }
        }
        else
        {
                set=100;
                printf("\nSUCCESS: %d.\n\n", rc);
        }
        //umask(mode_val);
out:
        for(i=0;i<args.infile_count;i++)
                free(args.arg2[i]);
out1:
        free(args.outfile);
        if(set == 0)
                printf("errno : %d\n",return_value);
out2:
        exit(rc);
        return return_value;
}
