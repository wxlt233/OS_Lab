// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{"backtrace","stack backtrace",mon_backtrace},
	{"showmappings","display physical mappings about a certain range",mon_showmappings}
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t ebp=read_ebp();
	struct Eipdebuginfo info;
	while (ebp!=0)
	{
		uint32_t  eip=*((uint32_t *)(ebp+4));
		cprintf("  ebp %08x eip %08x args ",ebp,eip); //内存中地址为ebp+4的空间存储着该函数的返回地址，通常也在调用该函数的函数内部
		uint32_t * ebpt=(uint32_t *)(ebp);   
		cprintf("%08x %08x %08x %08x %08x\n",*(ebpt+2),*(ebpt+3),*(ebpt+4),*(ebpt+5),*(ebpt+6));
		//本次函数调用的参数分别存储在地址为ebp+8,ebp+12,ebp+16……的内存中，实际参数个数不一定为5,由于ebpt为指向32位整数的指针
		//所以每往上寻找一个参数只需加1 （sizeof(uint32_t)=4）	
		debuginfo_eip(eip,&info);
		//通过已有函数接口，传入eip，查询是哪个函数调用了当前函数及其详细信息
		cprintf("    %s:%d: %.*s+%d\n",info.eip_file,info.eip_line,info.eip_fn_namelen,info.eip_fn_name,eip-info.eip_fn_addr);
		ebp=*(ebpt);
	}
	return 0;
}


void processflag(pte_t pagetableentry)
{
	cprintf("--");
	if (pagetableentry&PTE_D)
		cprintf("D");
	else 
		cprintf("-");
	if (pagetableentry&PTE_A)
		cprintf("A");
	else 
		cprintf("-");
	cprintf("--");
	if (pagetableentry&PTE_U)
		cprintf("U");
	else 
		cprintf("-");
	if (pagetableentry&PTE_W)
		cprintf("W");
	else 
		cprintf("-");
	cprintf("P");
}

extern pde_t * kern_pgdir;
int mon_showmappings(int argc,char **argv,struct Trapframe *tf)
{
	char *endptr;
	uintptr_t  vabegin=strtol(argv[1],&endptr,16);
	if (*endptr)
	{
		cprintf("format error!\n");
	}
	uintptr_t vaend=strtol(argv[2],&endptr,16);
	if (*endptr)
	{
		cprintf("format error!\n");
	}
	cprintf("va range ,     entry,  flag ,  pa range\n");
	//将字符串转为2个地址
	bool pan=0;
	while ((vabegin<vaend)&&!pan)
	{

		pde_t pagedirectoryentry=kern_pgdir[PDX(vabegin)];  //根据起始地址找到page directory 中对应的PDE
		if  (pagedirectoryentry&PTE_P)  //假设该PTSIZE虚拟内存对应的page table 存在
		{
			cprintf("[%08x %08x]  ,",vabegin,((PDX(vabegin)+1)<<22)-1);  //打印该PDE包括的虚拟地址
			cprintf("  PDE[%x]  ",PDX(vabegin));               //page directory 中对应的第几项
			processflag(pagedirectoryentry);                   //处理标志位
			if (vabegin>=KERNBASE)
			{
				cprintf("  [%08x %08x]\n",vabegin-KERNBASE,vabegin-KERNBASE+PTSIZE-1);
				uintptr_t vainit;
				vainit=vabegin;   //由于处理高地址如0xfffff000时再加PGSIZE会导致整数上溢,加此判断
				vabegin+=PTSIZE;
				if (vainit>vabegin)  //若vabegin+PGSIZE<vabegin,则说明vabegin已超过0xffffffff(32位无符号数的上限,溢出,同时也							   说明已到达虚拟地址最高处,可以结束循环)
				{
					pan=1;
				}
				continue;
			}
			cprintf("\n");
			int i=0;
			pte_t * pagetable=(pte_t *)(PTE_ADDR(pagedirectoryentry)+KERNBASE); //根据PDE_T表项中的物理地址找到对应的pagetable
//			cprintf("%08x\n",pagetable);
			for (i=PTX(vabegin);i<1024&&vabegin<vaend;i++) //遍历对应的PTE_T
			{
				pte_t thispagetableentry=pagetable[i];   //找到page table中对应的项
				if (thispagetableentry&PTE_P)    //如果对应的物理页面存在
				{
					cprintf("  [%08x %08x]  ",vabegin,vabegin+PGSIZE-1);
					cprintf("  PTE[%03x]  ",PTX(vabegin));
					processflag(thispagetableentry);
					cprintf("  [%08x %08x]\n",PTE_ADDR(thispagetableentry),PTE_ADDR(thispagetableentry)+PGSIZE-1);
				}
				vabegin+=PGSIZE;		
			}
		}
		else 
		{
			vabegin=((PDX(vabegin)+1)<<22);	 //若该PTSIZE对应的page table不存在,则说明该PTSIZE的虚拟内存没有被映射到物理内存
							//直接跳到下一个PTSIZE的虚拟内存
		}
	}
	return 0;	
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");
//	int x=1,y=3,z=4;
//	cprintf("x %d, y %x, z %d\n",x,y,z);
//	unsigned int i=0x00646c72;
//	cprintf("H%x Wo%s",57616,&i);
//	cprintf("x=%d y=%d",3);
	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
