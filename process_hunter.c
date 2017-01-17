/*******************************************
 * Name   : process_hunter.c
 * Version: 0.4.2
 * test Kernel : 2.6.5-2.6.12 (normal)/RedHat FC2 (2.6.5-4G patch)
 ******************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/mm.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/string.h>
#include <asm/types.h>

#include <linux/proc_fs.h>

struct array_cache {
        unsigned int avail;
        unsigned int limit;
        unsigned int batchcount;
        unsigned int touched;
};

struct kmem_list3 {
        struct list_head        slabs_partial;  /* partial list first, better asm code */
        struct list_head        slabs_full;
        struct list_head        slabs_free;
        unsigned long   free_objects;
        int             free_touched;
        unsigned long   next_reap;
        struct array_cache      *shared;
};

struct kmem_cache_s {
/* 1) per-cpu data, touched during every alloc/free */
        struct array_cache      *array[NR_CPUS];
        unsigned int            batchcount;
        unsigned int            limit;
/* 2) touched by every alloc & free from the backend */
        struct kmem_list3       lists;
        /* NUMA: kmem_3list_t   *nodelists[MAX_NUMNODES] */
        unsigned int            objsize;
        unsigned int            flags;  /* constant flags */
        unsigned int            num;    /* # of objs per slab */
        unsigned int            free_limit; /* upper limit of objects in the lists */
        spinlock_t              spinlock;

/* 3) cache_grow/shrink */
        /* order of pgs per slab (2^n) */
        unsigned int            gfporder;

        /* force GFP flags, e.g. GFP_DMA */
        unsigned int            gfpflags;

        size_t                  colour;         /* cache colouring range */
        unsigned int            colour_off;     /* colour offset */
        unsigned int            colour_next;    /* cache colouring */
        kmem_cache_t            *slabp_cache;
        unsigned int            slab_size;
        unsigned int            dflags;         /* dynamic flags */

        /* constructor func */
        void (*ctor)(void *, kmem_cache_t *, unsigned long);

        /* de-constructor func */
        void (*dtor)(void *, kmem_cache_t *, unsigned long);

/* 4) cache creation/removal */
        const char              *name;
        struct list_head        next;

/* 5) statistics */
#if STATS
        unsigned long           num_active;
        unsigned long           num_allocations;
        unsigned long           high_mark;
        unsigned long           grown;
        unsigned long           reaped;
        unsigned long           errors;
        unsigned long           max_freeable;
        atomic_t                allochit;
        atomic_t                allocmiss;
        atomic_t                freehit;
        atomic_t                freemiss;
#endif
#if DEBUG
        int                     dbghead;
        int                     reallen;
#endif
};

//typedef unsigned int kmem_bufctl_t;
#if defined(CONFIG_X86_4G) && defined(CONFIG_X86_SWITCH_PAGETABLES) && defined(CONFIG_X86_4G_VM_LAYOUT) && defined(CONFIG_X86_UACCESS_INDIRECT) && defined(CONFIG_X86_HIGH_ENTRY)
typedef unsigned int kmem_bufctl_t;  /* for RC2 */
#endif

#define BUFCTL_END	(((kmem_bufctl_t)(~0U))-0)
#define BUFCTL_FREE	(((kmem_bufctl_t)(~0U))-1)
#define	SLAB_LIMIT	(((kmem_bufctl_t)(~0U))-2)

struct slab{
        struct list_head        list;
        unsigned long           colouroff;
        void                    *s_mem;         /* including colour offset */
        unsigned int            inuse;          /* num of objs active in slab */
        kmem_bufctl_t           free;
};

static struct kmem_cache_s *dummy_cachep;
static int      array[128];

static inline kmem_bufctl_t *slab_bufctl(struct slab *slabp)
{
        return (kmem_bufctl_t *)(slabp+1);
}

static __inline__ char *parse_state(unsigned char state)
{
	switch (state){
		case TASK_RUNNING:
		return "TASK_RUNNING";

		case TASK_INTERRUPTIBLE:
		return "TASK_INTERRUPTIBLE";

		case TASK_UNINTERRUPTIBLE:
		return "TASK_UNINTERRUPTIBLE";

		case TASK_STOPPED:
		return "TASK_STOPPED";

#ifdef TASK_TRACED
		case TASK_TRACED:
		return "TASK_TRACED";
#endif

#if defined(TASK_ZOMBIE)
		case TASK_ZOMBIE:
		return "TASK_ZOMBIE";
#elif defined(EXIT_ZOMBIE)
		case EXIT_ZOMBIE:
		return "EXIT_ZOMBIE";
#endif

#if defined(EXIT_DEAD)
		case EXIT_DEAD:
		return "EXIT_DEAD";
#elif defined(TASK_DEAD)
		case TASK_DEAD:
		return "TASK_DEAD";
#endif

		default:
		return "strange state!";
	}	
}
	
ssize_t showprocess_read(struct file *unused_file, char *buffer, size_t len, loff_t *off)
{

        struct list_head        *p, *q;
        struct kmem_cache_s     *cachep;
        struct slab             *slb;
        struct task_struct      *ts;
        int                     i;
        int                     count;
        
        
	count = 0;
        dummy_cachep = kmem_cache_create("process_dummy_cachep", 128, ARCH_MIN_TASKALIGN, SLAB_PANIC, NULL, NULL);
	if (!dummy_cachep)
	{
		printk("kmem_cache_create failed\n");
		return -1;
	}
		
	list_for_each(p, dummy_cachep->next.prev){
		cachep = list_entry(p, struct kmem_cache_s, next);
		if(cachep->name){
			spin_lock(&cachep->spinlock);   
			if(!strcmp(cachep->name, "task_struct")){
				printk("----------slabs full----------\n");
//printk("size of task_struck: %d  cachep->objsize: %d\n", sizeof(struct task_struct), cachep->objsize);
				printk("\n%16s    %5s    %18s\n\n", "name", "pid", "state");
				list_for_each(q, &(cachep->lists.slabs_full)){
					slb = list_entry(q, struct slab, list);
					count += slb->inuse;
//printk("this slab->in_use = %d\n", slb->inuse);
                                        
					for(i = 0; i < cachep->num; i++){
						ts = slb->s_mem + i * cachep->objsize;
						if ((ts->pid >= 0 && ts->pid< 65536) && (ts->state >= 0 && ts->state < 64))		//aviod to get messy
							printk("%16s    %5d    %18s %ld\n",  ts->comm, ts->pid, parse_state(ts->state), ts->state);
	                             }
                             }
				printk("\n---------slabs partial--------\n");

				list_for_each(q, &(cachep->lists.slabs_partial)){
					slb = list_entry(q, struct slab, list);
					count += slb->inuse;
//printk("this slab->in_use = %d\n", slb->inuse);
					memset(array, 0, sizeof(array));
					i = slb->free;
//printk("slb->free = %d\n", i);
					do{
						array[i] = 1;
						i = slab_bufctl(slb)[i];
					}while(i != BUFCTL_END);

					for(i = 0; i < cachep->num; i++)
						if(!array[i]){
                                          	ts = slb->s_mem + i * cachep->objsize;
							printk("%16s    %5d    %18s %ld\n",  ts->comm, ts->pid, parse_state(ts->state), ts->state);
					}

				}
				printk("\n---------slabs free---------\n");
				list_for_each(q, &(cachep->lists.slabs_free)){
					slb = list_entry(q, struct slab, list);
					count += slb->inuse;
//                         	printk("this slab->in_use = %d\n", slb->inuse);
				}
				
				printk("\nTotal processes = %d\n\n", count);

			}
			spin_unlock(&cachep->spinlock); 
		}
	}

	kmem_cache_destroy(dummy_cachep);
	dummy_cachep = NULL;
	
        return 0;
}

static struct file_operations showprocess_ops = {

		read:showprocess_read,
};

int init_module(void)
{
	struct proc_dir_entry *entry;
	
	entry = create_proc_entry("showprocess", S_IRUSR, &proc_root);
	if (!entry)
	{
		printk("create_proc_entry failed\n");
		return -1;
	}
	
	entry->proc_fops = &showprocess_ops;

	return 0;
}

void cleanup_module(void)
{
	remove_proc_entry("showprocess", &proc_root);
	if (dummy_cachep)
		kmem_cache_destroy(dummy_cachep);
	dummy_cachep = NULL;
}


MODULE_LICENSE("GPL");
