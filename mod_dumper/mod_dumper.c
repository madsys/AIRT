/*********************************************
 * Name    : moddumper.c                     *
 * Version : 0.1                             *
 * Author  : CoolQ & madsys                  *
 * Usage   : insmod moddumper.ko mod_name=...*
 *           cat /proc/get_mod               *
 *           you'll get 2 files:             *
 *           dump.info and dump.dat          *
 * Intro   : This prog is a simple module    *
 *           dumper                          *
 ********************************************/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#define EOF             (-1)
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

struct file *klib_fopen(const char *filename, int flags, int mode);
void klib_fclose(struct file *filp);
int klib_fwrite(char *buf, int len, struct file *filp);

static struct module 	*mod;
static char buffer[256];
static char *mod_name = NULL;

module_param(mod_name, charp, 0);

ssize_t show_mod_read(struct file *fp, char *buf, size_t len, loff_t *off)
{
	struct file 	*filep;

	filep = klib_fopen("./dump.dat", O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(filep == NULL){
		printk("Error open files.\n");
		return 0;
	}
	klib_fwrite(mod->module_core, mod->core_size, filep);
	klib_fclose(filep);
	
	filep = klib_fopen("./dump.info", O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(filep == NULL){
		printk("Error open files.\n");
		return 0;
	}
	sprintf(buffer, "mod->module_init = 0x%p\n"
			"mod->module_core = 0x%p\n"
			"mod->init_size = %ld\n"
			"mod->core_size = %ld\n"
			"mod->init_text_size = %ld\n"
			"mod->core_text_size = %ld\n",
			mod->module_init, mod->module_core,
			mod->init_size, mod->core_size,
			mod->init_text_size, mod->core_text_size);
	klib_fwrite(buffer, strlen(buffer), filep);
	klib_fclose(filep);
	
	return 0;
}
static struct file_operations show_mod_fops = {

	.read = show_mod_read,
};

static int dummy_init(void)
{
	struct proc_dir_entry 	*entry;
	struct list_head 	*p;
	struct module		*head, *counter;
	
	mod = NULL;
	
	if(!mod_name)
		mod = THIS_MODULE;
	else{
		head = THIS_MODULE;
		list_for_each(p, head->list.prev){
			counter = list_entry(p, struct module, list);
			if(strcmp(counter->name, mod_name) == 0)
				mod = counter;
		}
	}
	if(!mod){
		printk("module %s not found.\n", mod_name);
		return -1;
	}
	//entry = create_proc_entry("get_mod", S_IRUSR, &proc_root);//old kernel
	proc_create_data("get_mod", 0, NULL, &show_mod_fops, NULL);	//3.5.x
	//entry->proc_fops = &show_mod_fops;						//old kernel
	
	return 0;
}

static void dummy_exit(void)
{
	//remove_proc_entry("get_mod", &proc_root);	//old kernel
	remove_proc_entry("get_mod", NULL); 		//第二个参数：parent dir
	return;
}

struct file *klib_fopen(const char *filename, int flags, int mode)
{
    struct file *filp = filp_open(filename, flags, mode);
    return (IS_ERR(filp)) ? NULL : filp;
}

void klib_fclose(struct file *filp)
{
    if (filp)
        fput(filp);
}

int klib_fwrite(char *buf, int len, struct file *filp)

{

	int writelen;
	mm_segment_t oldfs;

	if (filp == NULL)
		return -ENOENT;
	if (filp->f_op->write == NULL)
		return -ENOSYS;
	if (((filp->f_flags & O_ACCMODE) & (O_WRONLY | O_RDWR)) == 0)
		return -EACCES;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	writelen = filp->f_op->write(filp, buf, len, &filp->f_pos);
	set_fs(oldfs);

	return writelen;

}

module_init(dummy_init);
module_exit(dummy_exit);

MODULE_LICENSE("GPL");
