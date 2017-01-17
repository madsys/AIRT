/***************************************
 * Name	  : dismod.c                   *
 * Version: 0.1                        *
 * User   : CoolQ                      *
 * License: GPL                        *
 * Intro  : This little prog reads the *
 *          dump.dat and give you the  *
 *          opcodes.                   *
 **************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "bfd.h"
#include "dis-asm.h"
#include "utils.h"

#define DEF_SYMBOL_FILE "/boot/System.map"
#define DEF_DIS_SIZE 100
#define DEF_BASE_ADDR 0xc0000000
#define DEF_DUMP_FILE "./dump.dat"

#define PANIC(str) do{ perror(str); exit(EXIT_FAILURE); }while(0)



static char		*symbol_file;
static int		is_base64;
static unsigned long 	base_addr;
static unsigned long	dis_size;
static char		*dump_file;
static nodep		root;

static struct disassemble_info myinfo;

int my_read_func(bfd_vma memaddr, 
		bfd_byte *myaddr, 
		unsigned int length, 
		struct disassemble_info *myinfo)
{
	unsigned long bytes;
	
	bytes = memaddr - myinfo->buffer_vma;
	
	memcpy(myaddr, myinfo->buffer + bytes, length);
	return 0;
}

void my_error_func(int status, 
		bfd_vma memaddr,
		struct disassemble_info *myinfo)
{
	myinfo->fprintf_func(myinfo->stream, "Error\n");

	return;
}

void my_address_func(bfd_vma memaddr, 
		struct disassemble_info *myinfo)
{
	char 	*p;

	p = NULL;
	myinfo->fprintf_func(myinfo->stream, "0x%x", memaddr);
	p = find_symbol(root, memaddr);
	if(p)
		myinfo->fprintf_func(myinfo->stream, " \t<%s>", p);
	return;
}

static void print_usage(const char *prog)
{
	fprintf(stdout, "Usage:%s [-t SymbolFile] [-b] [-s BaseAddr]", prog);
	fprintf(stdout, " [-l DisSize] [-f DumpFile] [-h]\n");
	fprintf(stdout, "Params:\n");
	fprintf(stdout, "\t-t SymbolFile\t: specify symbol file, if not, use /boot/System.map.\n");
	fprintf(stdout, "\t-b\t\t: the dump file is BASE64 encoded.\n");
	fprintf(stdout, "\t-s BaseAddr\t: use BaseAddr as start_vma.\n");
	fprintf(stdout, "\t-l DisSize\t: specify the bytes to disassemble.\n");
	fprintf(stdout, "\t-f DumpFile\t: specify dump file, if not, use ./dump.dat.\n");
	fprintf(stdout, "\t-h\t\t: show this help.\n");

	exit(EXIT_FAILURE);
}
static void info_init(void)
{
	myinfo.mach = bfd_mach_i386_i386;
	myinfo.disassembler_options = "i386,att,addr32,data32";
	myinfo.fprintf_func = (int (*)(void *, const char*, ...))fprintf;
	myinfo.stream = stdout;
	myinfo.read_memory_func = my_read_func;
	myinfo.memory_error_func = my_error_func;
	myinfo.print_address_func = my_address_func;
	myinfo.buffer_vma = base_addr;
	myinfo.buffer_length = dis_size;
	myinfo.buffer = malloc(dis_size);

}
static void load_symbol(void)
{
	FILE 		*fp;
	char		buf[256], *symbol;
	unsigned long 	addr;
	
	if((fp = fopen(symbol_file, "r")) == NULL){
		fprintf(stdout, "No symbol file found.\n");
		return;
	}

	root = NULL;
	fprintf(stdout, "Start loading symbol table.\n");
	while(fgets(buf, 256, fp)){
		*strchr(buf, '\n') = 0;
		buf[255] = 0;
		symbol = &buf[11];
		addr = get_addr_2(buf);
		root = add_node(root, addr, symbol);
	};
	fprintf(stdout, "symbol table loading OK.\n");
	fclose(fp);
	return;
}

static void load_data(void)
{
	FILE 	*fp;
	
	if((fp = fopen(dump_file, "r")) == NULL)
		PANIC("error open file dump.dat\n");
	if(!is_base64)
		fread(myinfo.buffer, dis_size, 1, fp);
	else{
	/* TODO: Add BASE64 support  */
		
	}
	fclose(fp);

	return;
}

static void disassemble(void)
{
	int	i;
	
	i = 0;
	do{
		fprintf(stdout, "<%x+%x>\t", (unsigned int)base_addr, i);
		i += print_insn_i386_att(base_addr + i, &myinfo);
		fprintf(stdout, "\n");
	
	}while(i < myinfo.buffer_length);

	return;
}

int main(int argc, char *argv[])
{

	int		ret;
	
	symbol_file = dump_file = NULL;
	is_base64 = 0;
	base_addr = 0;
	dis_size = 0;
	
	while(1){
		ret = getopt(argc, argv, "t:bs:l:f:h");
		if(ret == -1)
			break;
		switch(ret){
			case '?':
			case ':':
			case 'h':
				print_usage(argv[0]);
				break;
			case 't':
				symbol_file = strdup(optarg);
				break;
			case 'b':
				is_base64 = 1;
				break;
			case 's':
				base_addr = get_addr(optarg);
				break;
			case 'l':
				dis_size = atoi(optarg);
				break;
			case 'f':
				dump_file = strdup(optarg);
				break;
		};
	};
	
	if(!symbol_file)
		symbol_file = strdup(DEF_SYMBOL_FILE);
	if(!dump_file)
		dump_file = strdup(DEF_DUMP_FILE);
	if(!dis_size)
		dis_size = DEF_DIS_SIZE;
	if(!base_addr)
		base_addr = DEF_BASE_ADDR;
	
	
	info_init();
	load_data();
	load_symbol();
	disassemble();
	
	return 0;
}
