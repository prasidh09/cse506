#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include "structure.h"
#include <linux/fs.h> //Needed for filp
#include <asm/uaccess.h> //Needed by segment descriptors
#include <linux/slab.h> // For using kmalloc
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/fcntl.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

asmlinkage extern long (*sysptr)(void *arg,int argslen);

//Declaration for validate,read and write functions
int validate_input_file(const char *filename);
int reads_file(const char *filename,void *buf,int len,char *outfile,int flag, int mode,int oflag);
int write_file(const char *filename,void *buf,int len, int flag,int mode,int oflag);

asmlinkage long xconcat(void *arg,int argslen)
{
/*
	---------------------------VARIABLES USED-------------------------------------------------

	i                - To iterate through the arguments received through void pointer (arg2)
	bytes_read       - Stores the number of bytes returned by the read syscall
	bytes_written    - Number of bytes written to the file 
	length		 - Number of bytes to read / write
	infile           - file name to be read
	outfile          - file to be written to	
	read_count       - returns the total bytes read by the syscall in each file
	total 		 - return the total bytes read in all the files
	--------------------------------------------------------------------------------------------

*/

	int i,bytes_read,length,total,return_value=0;
	struct myargs *temp = (struct myargs*)arg;

	char **infile;

	char *outfile=kmalloc(sizeof(char *),GFP_KERNEL);

	int flag=temp->flags;
	int argslen_syscall; // To check the argslen is correct
	int mode=temp->mode;
	int oflag=temp->oflags;
	int count=temp->infile_count;
	int valid=0;
	length=4096;
	total=0;//Total number of bytes read
	argslen_syscall=sizeof(struct myargs);

	infile=(char**)kmalloc((sizeof(char *)*count),GFP_KERNEL);

	if(argslen!=argslen_syscall)
	{
		//argslen_syscall=argslen;
		printk("\nInvalid argslen %d \n",argslen_syscall);
		return_value=-EINVAL;
		goto out;
	}
	/* syscall: returns EINVAL for null args, -EINVAL for NULL */
	if(arg == NULL)
	{
		printk("\n\nArgument is NULL.\n\n");
		return_value= -EINVAL;
		goto out;
	}

	for(i=0;i<count;i++)
	{
		infile[i]=(char *)kmalloc(sizeof(char)*255,GFP_KERNEL);
	}	

	if(copy_from_user(outfile,temp->outfile,strlen(temp->outfile)+1))
	{	
		kfree(outfile);
		return_value= -EINVAL;
		goto out;
	}
	
	for(i=0;i<count;i++)
	{
		if(copy_from_user(infile[i],temp->arg2[i],strlen(temp->arg2[i])+1))
		{	
			kfree(infile[i]);
			return_value= -EINVAL;
			goto out;
		}
		printk("\n\nLength:..............%d\n",strlen(temp->arg2[i]));
	}
	
	
	if(infile[0]!=NULL && outfile !=NULL)
	{
		//GFP_atomic -> The allocation is high priority and must not sleep
		//Check if we have valid input and output file names
		int value;
		char *bufp = kmalloc(4096, GFP_ATOMIC);  
		if (!bufp)
		{
			printk("error allocating memory \n");
			for(i=0;i<count;i++)
				kfree(infile[i]);
			kfree(bufp);
			return_value= -ENOMEM;
			goto out;
		}
		bytes_read=0;
		
		for(i=0;i<temp->infile_count;i++)
		{	
			value=validate_input_file(infile[i]);	
			printk("\nEntering file %d",i+1);		
			if(value<0)
			{
				return_value= value;
				goto out;
			}
			valid=valid+value;
			//printk("\nFile %d(%s):%d",i+1,infile[i],valid);
			
		}
		printk("\nValid :%d",valid);
		if(valid!=temp->infile_count)
		{
			printk("\nOne or more files do not have read permission\n");
			kfree(bufp);
			return_value= -EPERM;
			goto out;
		}
		for(i=0;i<temp->infile_count;i++)
		{	
			printk("\n All files can be successfully read.\n");		
			bytes_read=reads_file(infile[i],bufp,length,outfile, flag,mode,oflag);
			total+=bytes_read;
		}
		kfree(bufp);
		if(flag & 1<<7)
		{
			return_value= total/temp->infile_count;
			goto out;
		}
		//printk("\nMode....%d\n",mode);
		//printk("Bytes read : %d",total);
		else if(flag & 1<<6)
		{
			return_value= temp->infile_count;
			goto out;
		}
		else 
		{
			return_value= total;
			goto out;
		}
	}
	else
	{
		printk("\n No file to be read...!!!\n");
		return_value= -EBADF;
		goto out;
	}
out:
	return return_value;
}
/*
This function is used to check if all the read files have the read permission. If not it returns an error resulting in avoiding unwanted processing of data/unwanted writes.
*/
int validate_input_file(const char *filename)
{
	struct file *filp;
	filp=filp_open(filename, O_RDONLY, 644);
	if(!filp || IS_ERR(filp))
	{
		printk("\nRead file error %d\n",(int) PTR_ERR(filp));
		return -ENOENT;
	}
	if (!(filp->f_mode & FMODE_READ))
                 return -EBADF;
        if (!filp->f_op->read && !filp->f_op->aio_read)
                 return -EINVAL;
	filp_close(filp,NULL);
	return 1;
}

/* 
READ the contents of the file
*/

int reads_file(const char *filename, void *buf, int len, char *outfile, int flag, int mode, int oflag)
{
	char *buffer;
	struct file *filp;
	mm_segment_t fs;
	int bytes,percent;
	int read_count=0, write_count=0;	
	// Validate user buffer
	// buf = (char*)0x1231231
	buffer=(char *)buf;
	percent=0;
	filp=filp_open(filename, O_RDONLY, 644);
	if(!filp || IS_ERR(filp))
	{
		printk("Read file error %d\n",(int) PTR_ERR(filp));
		return -ENOENT;
	}
	if (!(filp->f_mode & FMODE_READ))
                 return -EBADF;
        if (!filp->f_op->read && !filp->f_op->aio_read)
                 return -EINVAL;/*
        if (unlikely(!access_ok(VERIFY_WRITE, buf, len)))
                 return -EFAULT;*/
	fs=get_fs(); 	  // Get the current segment descriptor
	set_fs(get_ds()); //Set the segment descriptor associated to kernel space
	while( (bytes=vfs_read(filp,buffer,len,&filp->f_pos))!=0) //Read the file and return the number of bytes read
	{
		read_count+=bytes;
		bytes=write_file(outfile,buffer,bytes,flag,mode,oflag);
		write_count+=bytes;
	}
	set_fs(fs); //Restore the segment descriptor
	//printk("Contents of file are :%s\n",buffer); //Print the file contents read
	filp_close(filp,NULL);
	percent=(read_count/write_count)*100;
	if(flag & 128)
		return percent;	
	return write_count;
}

//WRITE the contents to another file

int write_file(const char *filename, void *buf, int len, int flag,int mode, int oflag)
{

	char *buffer;
	struct file *filp;
	mm_segment_t fs;
	int bytes;

	buffer=(char *)buf;

/*
Through O_CREAT the filp_open() system call creates a new file if it does not exist at the speicified path. The third argument "S_IRWXU" specifies the access permissions for the file
*/
	//printk("\nMode in write file : %s....................................................\n",filename);
	
	filp=filp_open(filename, oflag, mode);	
		
	//printk("Outfile: %s......Mode:%d ....Flag:%d",filename,mode,oflag);
	if(!filp || IS_ERR(filp))
	{
		printk("Write error %d\n",(int) PTR_ERR(filp));
		return -ENOENT;
	}
	if(!filp->f_op->write)
		return -EPERM;

	fs=get_fs();
	set_fs(get_ds());
	bytes=filp->f_op->write(filp,buffer,len,&filp->f_pos);
	set_fs(fs);
	printk("Data written : %d\n",bytes);
	filp_close(filp, NULL);

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
