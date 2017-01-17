/*
 * name: module hunter 2.6 ver 1.5
 * first release on: 04.9.6
 * test on 2.6.5-2.6.12 (normal)/RedHat FC2 (2.6.5)
 *
 *  Author:
 *  2003 		madsys   Implemented original code
 * 
 *  Fix:
 *  2004.7	madsys	made it works on 2.6 kernel
 *  2004.10	CoolQ 	Added the ability of finding module on 4g/4g patch(ingo) and PAE mode
 *  2004.12	madsys	Added a trick for compiling on SUSE
 *  2004.12	madsys	Added the ability of revealing hidden module
 *  2004.12	madsys	Fixed the serious bug about revealing hidden module
 *  2004.12	madsys	Made the output more heuristic
 *  2005.1	madsys&Cool	Fixed a bug of seeking modules throughout memory 
 * 					(because the stepping is not 4k, mayb the module is across a page)
 *
 *
 *  usage: 
 *  list all modules:	 insmod mh.ko && cat /proc/showmodules
 *  reveal the hidden module:  echo "0xaabbccdd" > /proc/showmodules
 *  Note: 0xaabbccdd is the address of the suspect module and it must be 10 characters
 *                                        
 */


#include <linux/config.h>

#ifdef CONFIG_SMP
#define __SMP__ 
#endif

#if CONFIG_MODVERSIONS == 1
#define MODVERSIONS
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/mm.h>

#include <linux/proc_fs.h>

#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/fixmap.h>
#include <asm/page.h>
#include <asm/types.h>

#define HARDCORE_PD 0xc041f000
#define STEPPING (PAGE_SIZE/32)
#define __VMALLOC_RESERVE	(128 << 20)
#define MAX_MODULE 128

#ifdef __VMALLOC_RESERVE_DEFAULT
#define VMALLOC_RESERVE __VMALLOC_RESERVE_DEFAULT //for SUSE
#endif

static unsigned int reveal  = 0;
module_param(reveal, int, 0);


typedef enum RAM_MODE{
	MODE_NORMAL_4K,
	MODE_NORMAL_4M,
}MODE;

static MODE 	mode;
static u32 	cr3, cr4;

static struct module *listed_modules[MAX_MODULE];
static struct list_head *global_module_head;
static unsigned int total_normal_modules;

/*
 * valid_addr_normal_4K,valid_addr_normal_4M
 * valid_addr_pae_4K, valid_addr_pae_2M
 * do the dirty job separately
 * return 1 for valid
 * return 0 for invalid
 */

static int valid_addr_normal_4K(u32 address)
{
	u32 	pde, pte;
	u32	cr3_tmp;
	u32	pde_idx, pte_idx;
	u32	addr;
	
	if (!address)
		return 0;

	cr3_tmp = cr3 & 0xfffffe00;
	//cr3_tmp = HARDCORE_PD;
	pde_idx = address >> 22;
	pte_idx = (address & 0x003ff000) >> PAGE_SHIFT;
	
	pde = ((unsigned long *) __va(cr3_tmp))[pde_idx]; /* pde entry*/
	
	if (pde & _PAGE_PRESENT){

		addr = pde & PAGE_MASK;
		//address &= 0x003ff000;
		/* pte */
		pte = ((unsigned long *) __va(addr))[pte_idx];
		if (pte)
			return 1;
		else
			 return 0;
	}else
		return 0;


}


static int valid_addr_normal_4M(u32 address)
{

	u32	pde;
	u32	cr3_tmp;
	u32	pde_idx;

	if(!address)
		return 0;

	cr3_tmp = cr3 & 0xfffffe00;
	pde_idx = address >> 22;

	pde = ((u32 *) __va(cr3_tmp))[pde_idx];
	if(pde)
		return 1;
	else
		return 0;

}


int valid_addr(u32 address)
{
	switch(mode){
		
		case MODE_NORMAL_4K:
			return valid_addr_normal_4K(address);
			break;

		case MODE_NORMAL_4M:
			return valid_addr_normal_4M(address);
			break;

		default:
			return 0;
			break;
	}
}

unsigned int reveal_mod(struct module *evil)
{
	if (evil && valid_addr_normal_4K((u32)&evil->list))
	{
		list_add(&evil->list, &THIS_MODULE->list);
		printk("module revealed\n");
		
		return 0;
	}
	
	else 
		 return -EFAULT;
}

void get_listed_mod(void)
{
	struct module *rover;
	int listed_count=0;

	if (global_module_head->next == global_module_head)
	{
		printk("No module\n");
		return;
	}

	for (rover = list_entry(global_module_head->next, struct module, list); ; rover=list_entry(rover->list.next, struct module, list), listed_count++)
	{
		if (listed_count>=MAX_MODULE)
		{
			total_normal_modules = MAX_MODULE;
			printk("too many module!\n");
			return;
		}

		listed_modules[listed_count]=rover;
		
		if (rover->list.next == global_module_head)	//first module
		{
			total_normal_modules = listed_count+1;
			break;
		}
	}
}

int is_normal_mod(struct module *addr)
{
	int i;

	for (i=0; i<total_normal_modules; i++)
	{
		if (listed_modules[i] == addr)
			return 1;
	}

	return 0;
}


ssize_t showmodule_read(struct file *unused_file, char *buffer, size_t len, loff_t *off)
{
	struct module *p;
	unsigned int count =1;
	
	printk("\n      address                name     size"
		"      core_addr     flags  \n\n");
	
	__asm__ __volatile__ ( "movl %%cr3, %0" : "=r"(cr3) );

	get_listed_mod();
	
	/* brute force all the possible vms */
	for (p = (struct module *)VMALLOC_START;
	     p <= (struct module*)(VMALLOC_START + VMALLOC_RESERVE - PAGE_SIZE);
	     p = (struct module *)((u32)p + STEPPING)){
		
		if (valid_addr((u32)p + (u32)&((struct module *)NULL)->name)  && valid_addr((u32)p+sizeof(struct module)-1))
		{

			/* the valid module struct should match some rules */
			if ((     (p->name[0]>=0x30 && p->name[0]<=0x39)
			       || (p->name[0]> 0x41 && p->name[0]<=0x7a )) 
			    && (p->core_size < 1 <<20) 
			    && (p->core_size>= sizeof(struct module))
			    && p->state <3
			    && (u32)p->module_core > 0x02000000UL 
			    && ((u32)p->module_core & 0x00000fff)  == 0)

				printk("%3d  0x%p%18s   %6lu     0x%4p     %3d    %s\n", count++, 
					p, p->name, p->core_size, 
					p->module_core, p->state, (is_normal_mod(p)?" ":"Warning"));
		}			
	}
		 
	return 0;
}


ssize_t showmodule_write(struct file *unused_file, const char *buffer, size_t len, loff_t *off)
{
	char mod_string[11];
	unsigned int evil_addr;
	int error;

	if (len != (sizeof(mod_string)))
	{
		printk("wrong address, it must be 10 chars,len: %d buf: %s \n", len, buffer);
		return(-EINVAL);
	}

	if (copy_from_user(mod_string, buffer, len))
	{
		printk("copy characters to kernel failed\n");
		return(-EFAULT);
	}
	
	mod_string[10] = '\0';		//it should be '\0' already
	evil_addr = simple_strtoul(mod_string, NULL, 0);
//printk("evil_addr: 0x%x\n", evil_addr);

	if (evil_addr < PAGE_OFFSET)
	{
		printk("wrong address\n");
		return -EFAULT;
	}

	get_listed_mod();
	if (is_normal_mod((struct module *)evil_addr))
	{
		printk("the module which u want to reveal is not hidden\n");
		return -EFAULT;
	}

	__asm__ __volatile__ ( "movl %%cr3, %0" : "=r"(cr3) );

	if ((error = reveal_mod((void *)evil_addr)))
	{	
		printk("reveal module failed: %d\n", error);
		return -EFAULT;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
//printk("kobj refcount: %ud\n", ((struct module *)evil_addr)->mkobj->kobj.kref.refcount.counter); //mkobj->kobj.refcount.counter if 2.6.7 <= kernel <2.6.9
	if (kobject_register(&((struct module *)evil_addr)->mkobj->kobj))
	{
		printk("kobject already registered or registered failed\n");
		return -EFAULT;
	}
	
#else // >= 2.6.11 mkobj is a real structure inside the module structure, not a pointer anymore
printk("kobj refcount: %ud\n", ((struct module *)evil_addr)->mkobj.kobj.kref.refcount.counter);
	if (kobject_register(&((struct module *)evil_addr)->mkobj.kobj))
	{
		printk("kobject already registered or registered failed\n");
		return -EFAULT;
	}

#endif
#endif

	return len;
}


static struct file_operations showmodules_ops = {

		read:showmodule_read,
		write:showmodule_write,
};


int init_module(void)
{

	struct proc_dir_entry *entry;
	u32	addr;
	u32	pde, pde_idx;
	/* get cr3,cr4 registers and tell the VM mode */
	
	__asm__ __volatile__ ( "movl %%cr3, %0" : "=r"(cr3) );
	__asm__ __volatile__ ( "movl %%cr4, %0" : "=r"(cr4) );

	addr = (u32)&__this_module;
	
	pde_idx = addr >> 22;
	pde = ((u32 *) __va(cr3))[pde_idx];
		
	if(pde & (1 << 7))
		mode = MODE_NORMAL_4M;
	else
		mode = MODE_NORMAL_4K;

	printk("cr3 = 0x%x\n", cr3);

	printk("VM mode is %s\n", (mode == MODE_NORMAL_4K) ? "normal_4k" :"normal_4M");
	
	entry = create_proc_entry("showmodules", S_IRUSR, &proc_root);
	if (!entry)
	{
		printk("create_proc_entry failed\n");
		return -1;
	}
	
	entry->proc_fops = &showmodules_ops;

	global_module_head = THIS_MODULE->list.prev;

	return 0;

}


void cleanup_module()
{
	remove_proc_entry("showmodules", &proc_root);
}



MODULE_LICENSE("GPL");
MODULE_AUTHOR("madsys<at>ercist.iscas.ac.cn; qufuping<at>ercist.iscas.ac.cn");


