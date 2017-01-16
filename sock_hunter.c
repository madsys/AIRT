
/*******************************************
 * Name   : sock_hunter.c
 * Version: 0.4
 * test Kernel : 2.6.5 (normal)/RedHat FC2 (2.6.5) ~ 2.6.12

 Changes:

 		2005.7.20	madsys	added support for 2.6.12 kernel
 ******************************************/


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include <linux/mm.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/tcp.h>
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
	struct list_head        slabs_partial;  /* partial list first, better asm code*/
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
	struct kmem_cache            *slabp_cache;
	unsigned int            slab_size;
	unsigned int            dflags;         /* dynamic flags */

	/* constructor func */
	void (*ctor)(void *, struct kmem_cache *, unsigned long);

	/* de-constructor func */
	void (*dtor)(void *, struct kmem_cache *, unsigned long);

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

/*****************************************
 * the kmem_bufctl_t definition is quite *
 * differente between FC2 kernel and the *
 * general kernel, FC2 is uint, but the     *
 * general kernel is ushort!             *
 * **************************************/
typedef unsigned int kmem_bufctl_t;		//for 3.x kernel

#if defined(CONFIG_X86_4G) && defined(CONFIG_X86_SWITCH_PAGETABLES) && defined(CONFIG_X86_4G_VM_LAYOUT) && defined(CONFIG_X86_UACCESS_INDIRECT) && defined(CONFIG_X86_HIGH_ENTRY)
typedef unsigned int kmem_bufctl_t;  /* for RC2 */
#endif

#define BUFCTL_END	(((kmem_bufctl_t)(~0U))-0)
#define BUFCTL_FREE	(((kmem_bufctl_t)(~0U))-1)
#define SLAB_LIMIT	(((kmem_bufctl_t)(~0U))-2)

struct slab{
	struct list_head        list;
	unsigned long           colouroff;
	void                    *s_mem;         /* including colour offset */
	unsigned int            inuse;          /* num of objs active in slab */
	kmem_bufctl_t           free;
};

static struct kmem_cache_s *dummy_cachep;
static int      my_array[128];

static inline kmem_bufctl_t *slab_bufctl(struct slab *slabp)
{
	return (kmem_bufctl_t *)(slabp+1);
}

static __inline__ char *parse_tcp_state(unsigned char state)
{
	switch (state){
		case 1:
			return	"TCP_ESTABLISHED";

		case 2:
			return	"TCP_SYN_SENT";

		case 3:
			return 	"TCP_SYN_RECV";

		case 4:
			return 	"TCP_FIN_WAIT1";

		case 5:
			return 	"TCP_FIN_WAIT2";

		case 6:
			return	"TCP_TIME_WAIT";

		case 7:
			return	 "TCP_CLOSE";

		case 8:
			return 	"TCP_CLOSE_WAIT";

		case 9:
			return 	"TCP_LAST_ACK";

		case 10:
			return 	"TCP_LISTEN";
			//TCP_CLOSING;

		default:
			return 	"unknow state";
	}
}

static __inline__ char *parse_raw_state(unsigned char state)
{
	switch (state){
		case 0:
			return	"IPPROTO_IP"	;

		case 1:
			return	"IPPROTO_ICMP";

		case 2:
			return	"IPPROTO_IGMP";

		case 4:
			return 	"IPPROTO_IPIP";

		case 6:
			return	"IPPROTO_TCP";

		case 8:
			return 	"IPPROTO_EGP";

		case 17:
			return 	"IPPROTO_UDP";

		case 41:
			return	"IPPROTO_IPV6";

		case 47:
			return	"IPPROTO_GRE";

		case 255:
			return	"IPPROTO_RAW";

		default:
			return 	"unknow state";
	}
}

__inline__ char *in_ntoa(__u32 in)
{
	static char buff[18];
	char *p;

	p = (char *) &in;
	sprintf(buff, "%d.%d.%d.%d",
			(p[0] & 255), (p[1] & 255), (p[2] & 255), (p[3] & 255));
	return(buff);
}

ssize_t showsocks_read(struct file *unused_file, char *buffer, size_t len, loff_t *off)
{

	struct list_head        *p, *q;
	struct kmem_cache_s     *cachep;
	struct slab             *slabp;
	struct tcp_sock		*tcp_sk;
	int                     i;


	/*************************************************************
	 * the way we use here is to tranverse the tcp_sock slab,    *
	 * get all the active slab objs. there are 3 lists, slabs_   *
	 * free, slabs_full, slabs_partial. you can get active objs  *
	 * from the full and partial lists.                          *
	 * Tip: first we must get the symbol tcp_sock, unfortunately *
	 *      the kernel doesn't EXPORT this symbol. we can use    *
	 *      the System.map file located in /boot, but Redhat FC2 *
	 *      doesn't have such entry ( whereas the general has )! *
	 *      so, we can create a dummy cache discriptor , use     *
	 *      double linked list (kmem_cache_s->next) to get all   *
	 *      the cache symbols.                                   *
	 ************************************************************/

//	dummy_cachep = kmem_cache_create("sock_dummy_cachep", 128, ARCH_MIN_TASKALIGN,SLAB_PANIC, NULL, NULL);	for 2.6.x
	dummy_cachep = kmem_cache_create("sock_dummy_cachep", 128, ARCH_MIN_TASKALIGN,SLAB_PANIC, NULL);
	if (!dummy_cachep)
		{
			printk("kmem_cache_create failed (creat dummy cache)\n");
			return -1;
		}
//printk("get in dummy_cachep->next.prev: %x\n", dummy_cachep->next.prev);

	list_for_each(p, dummy_cachep->next.prev){
		cachep = list_entry(p, struct kmem_cache_s, next);
		if(cachep->name){
			spin_lock(&cachep->spinlock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
			if(!strcmp(cachep->name, "TCP")){
#else
			if(!strcmp(cachep->name, "tcp_sock")){
#endif
				/*------------ slabs full ---------------*/
				printk("\nTCP---------------------------------------------\n");
				printk("\n%5s    %15s    %15s\n\n", "port", "ip", "state");
				list_for_each(q, &(cachep->lists.slabs_full)){
					slabp = list_entry(q, struct slab, list);
					for(i = 0; i < cachep->num; i++){
						tcp_sk = slabp->s_mem + i * cachep->objsize;
						//printk("port:%d, ip:%x, state:%d\n", tcp_sk->inet.sport, tcp_sk->inet.saddr, tcp_sk->sk.sk_state);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->sk.sk_state));
#else
						//printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->inet.sk.sk_state));	//old kernel
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet_conn.icsk_inet.inet_sport), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_saddr), parse_tcp_state(tcp_sk->inet_conn.icsk_inet.sk.sk_state));		//3.5.x
#endif
					}
				}
				/*---------- slabs partial --------------*/
				list_for_each(q, &(cachep->lists.slabs_partial)){
					slabp = list_entry(q, struct slab, list);
					for(i = 0; i < cachep->num; i++)
						my_array[i] = 0;
					i = slabp->free;

					/* get all free objs and store the num in my_array */
					do{
						my_array[i] = 1;
						i = slab_bufctl(slabp)[i];
					}while(i != BUFCTL_END);

					for(i = 0; i < cachep->num; i++){
						if(!my_array[i]){
							tcp_sk = slabp->s_mem + i * cachep->objsize;
							//printk("port-s:%d, ip:%x, state:%d\n", ntohs(tcp_sk->inet.sport), tcp_sk->inet.saddr, tcp_sk->sk.sk_state);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->sk.sk_state));
#else
						//printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->inet.sk.sk_state));	//old kernel
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet_conn.icsk_inet.inet_sport), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_saddr), parse_tcp_state(tcp_sk->inet_conn.icsk_inet.sk.sk_state));		//3.5.x
#endif
						}
					}
				}
			}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
			else if(!strcmp(cachep->name, "UDP")){
#else
			else if(!strcmp(cachep->name, "udp_sock")){
#endif
				/*------------ slabs full ---------------*/
				printk("\nUDP---------------------------------------------\n");
				printk("\n%5s    %15s    %15s\n\n", "port", "ip", "state");
				list_for_each(q, &(cachep->lists.slabs_full)){
					slabp = list_entry(q, struct slab, list);
					for(i = 0; i < cachep->num; i++){
						tcp_sk = slabp->s_mem + i * cachep->objsize;
						//printk("port:%d, ip:%x, state:%d\n", tcp_sk->inet.sport, tcp_sk->inet.saddr, tcp_sk->sk.sk_state);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->sk.sk_state));
#else
						//printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->inet.sk.sk_state));	old kernel
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet_conn.icsk_inet.inet_sport), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_saddr), parse_tcp_state(tcp_sk->inet_conn.icsk_inet.sk.sk_state));		//3.5.x
#endif
					}
				}
				/*---------- slabs partial --------------*/
				list_for_each(q, &(cachep->lists.slabs_partial)){
					slabp = list_entry(q, struct slab, list);
					for(i = 0; i < cachep->num; i++)
						my_array[i] = 0;
					i = slabp->free;

					/* get all free objs and store the num in my_array */
					do{
						my_array[i] = 1;
						i = slab_bufctl(slabp)[i];
					}while(i != BUFCTL_END);

					for(i = 0; i < cachep->num; i++){
						if(!my_array[i]){
							tcp_sk = slabp->s_mem + i * cachep->objsize;
							//printk("port-s:%d, ip:%x, state:%d\n", ntohs(tcp_sk->inet.sport), tcp_sk->inet.saddr, tcp_sk->sk.sk_state);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->sk.sk_state));
#else
						//printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_tcp_state(tcp_sk->inet.sk.sk_state));
						printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet_conn.icsk_inet.inet_sport), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_saddr), parse_tcp_state(tcp_sk->inet_conn.icsk_inet.sk.sk_state));		//3.5.x
#endif
						}
					}
				}
			}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
			else if(!strcmp(cachep->name, "RAW")){
#else
			else if(!strcmp(cachep->name, "raw_sock")){
#endif
				/*------------ slabs full ---------------*/
				printk("\nRAW---------------------------------------------\n");
				printk("\n%5s    %15s    %15s    %15s\n\n", "protocol", "source ip", "destination ip", "state");
				list_for_each(q, &(cachep->lists.slabs_full)){
					slabp = list_entry(q, struct slab, list);
					for(i = 0; i < cachep->num; i++){
						tcp_sk = slabp->s_mem + i * cachep->objsize;
						//printk("port:%d, ip:%x, state:%d\n", tcp_sk->inet.sport, tcp_sk->inet.saddr, tcp_sk->sk.sk_state);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)						
						printk("%5d    %15s    %15s       %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), in_ntoa(tcp_sk->inet.daddr), parse_raw_state(tcp_sk->sk.sk_state));
#else
						//printk("%5d    %15s    %15s       %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), in_ntoa(tcp_sk->inet.daddr), parse_raw_state(tcp_sk->inet.sk.sk_state));
						  printk("%5d    %15s    %15s       %15s\n", ntohs(tcp_sk->inet_conn.icsk_inet.inet_sport), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_saddr), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_daddr), parse_tcp_state(tcp_sk->inet_conn.icsk_inet.sk.sk_state));		//3.5.x
#endif
					}
				}
				/*---------- slabs partial --------------*/
				list_for_each(q, &(cachep->lists.slabs_partial)){
					slabp = list_entry(q, struct slab, list);
					for(i = 0; i < cachep->num; i++)
						my_array[i] = 0;
					i = slabp->free;

					/* get all free objs and store the num in my_array */
					do{
						my_array[i] = 1;
						i = slab_bufctl(slabp)[i];
					}while(i != BUFCTL_END);

					for(i = 0; i < cachep->num; i++){
						if(!my_array[i]){
							tcp_sk = slabp->s_mem + i * cachep->objsize;
							//printk("%5d    %15s    %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), parse_raw_state(tcp_sk->sk.sk_state));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)						
						printk("%5d    %15s    %15s       %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), in_ntoa(tcp_sk->inet.daddr), parse_raw_state(tcp_sk->sk.sk_state));
#else
						//printk("%5d    %15s    %15s       %15s\n", ntohs(tcp_sk->inet.sport), in_ntoa(tcp_sk->inet.saddr), in_ntoa(tcp_sk->inet.daddr), parse_raw_state(tcp_sk->inet.sk.sk_state));
						printk("%5d    %15s    %15s       %15s\n", ntohs(tcp_sk->inet_conn.icsk_inet.inet_sport), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_saddr), in_ntoa(tcp_sk->inet_conn.icsk_inet.inet_daddr), parse_tcp_state(tcp_sk->inet_conn.icsk_inet.sk.sk_state));		//3.5.x
#endif
						}
					}
				}
			}



			spin_unlock(&cachep->spinlock); 
		}
	}        

	kmem_cache_destroy(dummy_cachep);
	return 0;
}


static struct file_operations showsocks_ops = {

read:showsocks_read,
};


int init_module(void)
{
	struct proc_dir_entry *entry;

	//entry = create_proc_entry("showsocks", S_IRUSR, &proc_root);	//old kernel
	proc_create_data("showsocks", 0, NULL, &showsocks_ops, NULL);
	
	/*if (!entry)
	{
		printk("create_proc_entry failed\n");
		return -1;
	}
	*/
	//entry->proc_fops = &showsocks_ops;

	return 0;
}

void cleanup_module()
{
	//remove_proc_entry("showsocks", &proc_root);	//old kernel
	remove_proc_entry("showsocks", NULL); 			//第二个参数：parent dir
}


MODULE_LICENSE("GPL");
