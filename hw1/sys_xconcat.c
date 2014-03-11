#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include "structure.h"
#include <linux/fs.h> //Needed for filp
#include <asm/uaccess.h> //Needed by segment descriptors
#include <linux/slab.h> // For using kmalloc
#include <linux/errno.h>

asmlinkage extern long (*sysptr)(void *arg,int argslen);

//Declaration for validate input, output files and to verify if they are the same files by comparison of their inodes
int validate_input_file(const char *filename);
int validate_output_file(const char *filename);
int validate_input_output_file(const char *filename, struct file *filp);

//Declaration of functions to read and write data from and to files.
int reads_file(const char *filename,void *buf,int len,struct file *outfile,int flag, int mode,int oflag);
int write_file(struct file *filp,void *buf,int len);
int write_count=0;
int read_count=0;

asmlinkage long xconcat(void *arg,int argslen)
{
/*
        ---------------------------VARIABLES USED-------------------------------------------------

        i                - To iterate through the arguments received through void pointer (arg2)
        bytes_written    - Stores the number of bytes returned by the read syscall
        length           - Number of bytes to read / write
        infile           - file name to be read
        outfile          - file to be written to
        read_count       - returns the total bytes read by the syscall in each file
        total            - return the total bytes read in all the files
        argslen_syscall  - To verify argslen
        bufp             - Temporary buffer to copy the contents read and write them to outfile
        write_count      - To keep track of the count of total bytes read
        read_count       - To keep track of the count of total bytes written
        valid            - To check if the files are valid by comparing with infile_count
        --------------------------------------------------------------------------------------------
*/

        int i,bytes_written,length,total,return_value=0;
        struct myargs *temp = (struct myargs*)arg;
        char *bufp=NULL;
        char **infile=NULL;
        char *outfile=NULL;
        struct file *filpo=NULL;
        struct file *filpt=NULL;
        char tempfile[]="temp123";
        int flag=temp->flags;
        int argslen_syscall;
        int mode=temp->mode;
        int oflag=temp->oflags;
        int count=temp->infile_count;
        int valid=0;
        length=4096;
        total=0;
        argslen_syscall=sizeof(struct myargs);

        // syscall: returns EINVAL for null args

        if(arg == NULL)
        {
                printk("\n\nArgument is NULL.\n\n");
                return_value = -EINVAL;
                goto out3;
        }

        //Check for argument length

        if(argslen != argslen_syscall)
        {
                printk("\nInvalid argslen %d \n",argslen_syscall);
                return_value = -EINVAL;
                goto out3;
        }

        // Allocate memory for input/output files and do copy_from_user.

        //printk("Argument Length : %d\n",argslen_syscall);

        outfile=kmalloc(sizeof(char *),GFP_KERNEL);

        if (!outfile)
        {
                printk("Error allocating memory for outfile.\n");
                return_value = -ENOMEM;
                goto out3;
        }

        infile=(char**)kmalloc((sizeof(char *)*count),GFP_KERNEL);

        for(i=0;i<count;i++)
        {
                infile[i]=(char *)kmalloc(sizeof(char)*255,GFP_KERNEL);
                if (!infile[i])
                {
                        printk("Error allocating memory for infile %d.\n",i+1);
                        return_value = -ENOMEM;
                        goto out2;
                }
        }

        if(copy_from_user(outfile,temp->outfile,strlen(temp->outfile)+1))
        {
                printk("Copy from user failed \n");
                return_value= -EINVAL;
                goto out1;
        }

        outfile=getname(temp->outfile);

        if(!outfile)
        {
                printk("Getname failed.\n");
                return_value = -ENAMETOOLONG;
                goto out1;
        }

        printk("Outfile: %s\n",outfile);

        for(i=0;i<count;i++)
        {
                if(copy_from_user(infile[i],temp->arg2[i],strlen(temp->arg2[i])+1))
                {
                        printk("Copy from user failed \n");
                        kfree(infile[i]);
                        return_value= -EINVAL;
                        goto out1;
                }
                infile[i]=getname(temp->arg2[i]);
                if(!infile[i])
                {
                        printk("Getname failed.\n");
                        return -ENAMETOOLONG;
                }
                printk("\nInfile %d: %s\n",i+1,infile[i]);
        }

        // Allocate a temporary buffer of PAGE SIZE

        bufp = kmalloc(4096, GFP_ATOMIC);

        //GFP_atomic -> The allocation is high priority and must not sleep
        if (!bufp)
        {
                printk("Error allocating memory for temporary buffer.\n");
                return_value= -ENOMEM;
                goto out1;
        }

        if(infile[0]!=NULL && outfile !=NULL)
        {
                //Check to validate input and output files

                int value=0,samefiles=0;
                bytes_written=0;
                if(!(flag & 1<<5))
                {
                        printk("Normal mode \n");
                        for(i=0;i<count;i++)
                        {
                                filpo=filp_open(outfile, oflag|O_WRONLY, mode);

                                if(!filpo || IS_ERR(filpo))
                                {
                                        printk("Write error 11%d\n",(int) PTR_ERR(filpo));
                                        return_value = (int)PTR_ERR(filpo);
                                        goto out;
                                }

                                if(!filpo->f_op->write)
                                {
                                        printk("Write error 12%d\n",(int) PTR_ERR(filpo));
                                        return_value = (int)PTR_ERR(filpo);
                                        goto out;
                                }

                                //Validate the input file for read permissions.
                                value=validate_input_file(infile[i]);

                                if(value<0)
                                {
                                        printk("Validation error of input file %d.\n",value);
                                        return_value=value;
                                        goto exit;
                                }
                                //Check if outfile and infiles have soft link.

                                samefiles=validate_input_output_file(infile[i],filpo);

                                if(samefiles == 1)
                                {
                                        printk("Same input and output files\n.");
                                        return_value=-EINVAL;
                                        goto out;
                                }
                                valid+=value;
                                filp_close(filpo,NULL);
                        }
                }
                else
                {
                        for(i=0;i<count;i++)
                        {
                                value=validate_input_file(infile[i]);
                                if(value<0)
                                {
                                        return_value= value;
                                        goto out;
                                }
                                valid=valid+value;
                                //printk("\nFile %d(%s):%d",i+1,infile[i],valid);
                        }
                }
                if(valid!=count)
                {
                        printk("\nOne or more files do not have read permission\n");
                        return_value= -EPERM;
                        goto out;
                }

                //***********************************************ATOMIC concat mode************************************

                if(flag & 1<<5)
                {
                        //READ the contents of the output file into the temp file only if the outfile exists.
                        //Open the temp file in the mode the user requested and perform the syscall functions
                        struct dentry *d=NULL;
                        struct inode *dir=NULL;

                        printk("\nAtomic concat mode....!\n");

                        filpt=filp_open(tempfile, oflag|O_CREAT, S_IRWXU);

                        if(!filpt || IS_ERR(filpt))
                        {
                                printk("Write error 21%d\n",(int) PTR_ERR(filpt));
                                return_value = (int) PTR_ERR(filpt);
                                goto out;
                        }

                        if(!filpt->f_op->write)
                        {
                                printk("Write error 22%d\n",(int) PTR_ERR(filpt));
                                return_value = (int) PTR_ERR(filpt);
                                goto out;
                        }
                        //Read the contents of the infiles into the temp file.

                        dir=filpt->f_dentry->d_parent->d_inode;
                        d=filpt->f_dentry;

                        for(i=0;i<count;i++)
                        {
                                bytes_written=reads_file(infile[i],bufp,length,filpt, flag,mode,oflag);
                                if(bytes_written<0)
                                {
                                        printk("\nREAD failed in atomic mode.\n");
                                        return_value = -bytes_written;
                                        filp_close(filpt,NULL);
                                        goto out;
                                }
                                total+=bytes_written;
                        }

                        printk("\n All files can be successfully read.\nRead count %d\nWrite_count2 %d\n", read_count,write_count);

                        bytes_written=0;

                        //Write to the output file only if number of bytes read == number of bytes written. This means the sys call succeeded.
                        if(write_count==read_count)
                        {

                                int un=0,un1=0,temp=write_count,DataWritten=0;
                                read_count=0;
                                write_count=0;

                                printk("\nSuccessfully read and written to temp file\n");
                                filp_close(filpt,NULL);

                                filpo=filp_open(outfile, oflag|O_WRONLY, mode);

                                if(!filpo || IS_ERR(filpo))
                                {
                                        printk("Write error 31%d\n",(int) PTR_ERR(filpo));
                                        return_value = (int) PTR_ERR(filpo);
                                        goto out;
                                }

                                if(!filpo->f_op->write)
                                {
                                        printk("Write error 32%d\n",(int) PTR_ERR(filpo));
                                        return_value = (int) PTR_ERR(filpo);
                                        goto out;
                                }

                                DataWritten=reads_file(tempfile,bufp,length,filpo, flag,mode,oflag);

                                printk("Data written in temp file %d \t Data written in output file %d\n",temp,write_count);

                                if(write_count == temp)
                                {
                                        printk("Success. Unlink temp file.\n");
                                        //filp_close(filpt,NULL);
                                        un = vfs_unlink(dir,d);
                                        if(un<0)
                                        {
                                                printk("Unlink of temp file failed \n");
                                                return_value=un;
                                                goto exit;
                                        }
                                        filp_close(filpo,NULL);
                                }
                                else
                                {

                                        struct dentry *d1=filpo->f_dentry;
                                        struct inode *dir1=filpo->f_dentry->d_parent->d_inode;
                                        printk("Failed. Unlink outfile and temp file.\n");

                                        //Unlink outfile

                                        filp_close(filpo,NULL);
                                        un = vfs_unlink(dir1,d1);
                                        if(un<0)
                                        {
                                                printk("Unlink of temp file failed \n");
                                                return_value=un;
                                                goto out;
                                        }

                                        //Unlink tempfile

                                        un1 = vfs_unlink(dir,d);
                                        if(un1<0)
                                        {
                                                printk("Unlink of outfile failed \n");
                                                return_value=un1;
                                                goto out;
                                        }

                                }
                        }
                        else
                        {
                                int un=0;
                                printk("\nOperation failed\n");
                                filp_close(filpt,NULL);
                                un = vfs_unlink(dir,d);
                                if(un<0)
                                {
                                        printk("Unlink of temp file failed \n");
                                        return_value=un;
                                        goto out;
                                }
                                return_value = -EINTR;
                                goto out;
                        }

                        //Return the percentage of data written

                        if(flag & 1<<7)
                        {
                                return_value= total/temp->infile_count;
                                goto out;
                        }

                        //Return the total number of bytes written or bumber of files

                        else
                        {
                                return_value= total;
                                goto out;
                        }
                }

                //******************************************************************************************************

                else
                {
                        printk("\nNormal mode executing.\n");

                        filpo=filp_open(outfile, oflag|O_WRONLY, mode);

                        if(!filpo || IS_ERR(filpo))
                        {
                                printk("Write error 11%d\n",(int) PTR_ERR(filpo));
                                return_value = (int)PTR_ERR(filpo);
                                goto out;
                        }

                        if(!filpo->f_op->write)
                        {
                                printk("Write error 12%d\n",(int) PTR_ERR(filpo));
                                return_value = (int)PTR_ERR(filpo);
                                goto out;
                        }


                        for(i=0;i<count;i++)
                        {
                                printk("\n All files can be successfully read.\n");
                                bytes_written=reads_file(infile[i],bufp,length,filpo, flag,mode,oflag);
                                if(bytes_written<0)
                                {
                                        printk("Error while reading and writing in normal mode.\n");
                                        return_value=bytes_written;
                                        goto exit;
                                }
                                printk("Bytes read from infile %d: %d",i+1,bytes_written);
                                total+=bytes_written;
                        }
                        printk("Total is %d",total);

                        if(flag & 1<<7)
                        {
                                return_value= total/temp->infile_count;
                                goto exit;
                        }

                        else
                        {
                                return_value= total;
                                goto exit;
                        }
                }

        }
        else
        {
                printk("\n No file to be read...!!!\n");
                return_value= -EBADF;
                goto out;
        }

exit:
        filp_close(filpo, NULL);
out:
        kfree(bufp);
out1:
        for(i=0;i<count;i++)
                kfree(infile[i]);
out2:
        kfree(outfile);
out3:
        read_count=0;
        write_count=0;
        printk("Return value %d\n",return_value);
        return return_value;

}

int validate_input_output_file(const char *filename,struct file *filp)
{
        int ret=0;
        struct file *filp1;

        filp1=filp_open(filename, O_RDWR, 644);

        if (filp->f_dentry->d_inode != NULL && filp1->f_dentry->d_inode != NULL)
        {
                if (filp->f_dentry->d_inode == filp1->f_dentry->d_inode)
                {
                        ret=1;
                }
        }

        filp_close(filp1,NULL);
        return ret;
}
int validate_output_file(const char *filename)
{
        struct file *filp;
        filp=filp_open(filename, O_RDWR, 644);
        if(!filp || IS_ERR(filp))
        {
                printk("\nRead file error output file%d\n",(int) PTR_ERR(filp));
                return (int)PTR_ERR(filp);
        }
        if (!(filp->f_mode & FMODE_READ))
                return (int)PTR_ERR(filp);
        if (!filp->f_op->read && !filp->f_op->aio_read)
                return (int)PTR_ERR(filp);
        filp_close(filp,NULL);
        return 1;
}
/*
This function is used to check if all the read files have the read permission. If not it returns an error resulting in avoiding unwanted processing of data/unwanted writes.
*/
int validate_input_file(const char *filename)
{
        struct file *filpi;
        filpi=filp_open(filename, O_RDONLY, 644);
        if(!filpi || IS_ERR(filpi))
        {
                printk("\nRead file error input file %d\n",(int) PTR_ERR(filpi));
                return (int)PTR_ERR(filpi);
        }
        if (!(filpi->f_mode & FMODE_READ))
                return (int)PTR_ERR(filpi);
        if (!filpi->f_op->read && !filpi->f_op->aio_read)
                return (int)PTR_ERR(filpi);
        filp_close(filpi,NULL);
        return 1;
}

/*
READ the contents of the file
*/

int reads_file(const char *filename, void *buf, int len, struct file *outfile, int flag, int mode, int oflag)
{
        char *buffer = (char *)buf;
        struct file *filp=NULL;
        mm_segment_t fs;
        int bread=0, bwrite=0,percent=0, read=0, write=0;

        filp=filp_open(filename, O_RDONLY, 644);

        if(!filp || IS_ERR(filp))
        {
                printk("Read file error reads file%d\n",(int) PTR_ERR(filp));
                return (int)PTR_ERR(filp);
        }

        if (!( filp->f_mode & FMODE_READ))
                return (int)PTR_ERR(filp);

        if (!filp->f_op->read && !filp->f_op->aio_read)
                return (int)PTR_ERR(filp);

        fs=get_fs();      // Get the current segment descriptor
        set_fs(get_ds()); //Set the segment descriptor associated to kernel space

        while( (bread=vfs_read(filp,buffer,len,&filp->f_pos))!=0) //Read the file and return the number of bytes read
        {
                if(bread < 0)
                {
                        printk("Error while reading. \n");
                        set_fs(fs);
                        filp_close(filp,NULL);
                        return bread;
                }

                read+=bread;
                bwrite=write_file(outfile,buffer,bread);
                if(bwrite < 0)
                {
                        printk("Error while writing. \n");
                        set_fs(fs);
                        filp_close(filp,NULL);
                        return bwrite;
                }
                write+=bwrite;
        }
        printk("Write2 %d\nRead2 %d\nWrite count2 %d\nRead count2 %d\n\n",write,read,write_count, read_count);
        write_count+=write;
        read_count+=read;
        printk("Write %d\nRead %d\nWrite count %d\nRead count %d\n",write,read,write_count, read_count);
        set_fs(fs); //Restore the segment descriptor
        //printk("Contents of file are :%s\n",buffer); //Print the file contents read
        filp_close(filp,NULL);
        percent=(read/write)*100;
        if(flag & 128)
                return percent;
        if(read==write && (flag & 1<<6))
        {
                return 1;
        }
        return write;
}

//WRITE the contents to another file

int write_file(struct file *filp, void *buf, int len)
{
        char *buffer;
        mm_segment_t fs;
        int bytes=0;
        buffer=(char *)buf;

        //printk("Outfile: %s\nMode:%d\nFlag:%d\n",filename,mode,oflag);

        fs=get_fs();
        set_fs(get_ds());
        bytes=filp->f_op->write(filp,buffer,len,&filp->f_pos);

        //bytes=vfs_write(filp,buffer,len,&filp->f_pos);
        set_fs(fs);
        printk("Data written : %d\n",bytes);
        return bytes;
}

/*
End of my code
*/


static int __init init_sys_xconcat(void)
{
        printk("installed new sys_xconcat module\n");
        if (sysptr == NULL)
                sysptr = xconcat;
        return 0;
}
static void  __exit exit_sys_xconcat(void)
{
        if (sysptr != NULL)
                sysptr = NULL;
        printk("removed sys_xconcat module\n");
}
module_init(init_sys_xconcat);
module_exit(exit_sys_xconcat);
MODULE_LICENSE("GPL");
