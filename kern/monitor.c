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
#include <kern/trap.h>

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
	{"showmappings","display physical mappings about a certain range",mon_showmappings},
	{"setclearpermission","set or clear permisssion of any  mapping",mon_setclear},
	{"x","show the content of the corresponding virtual memory",mon_showvirtualmemory},
	{"xp","show the content of the corresponding physical memory",mon_showphysicalmemory},
	{"si","single step one instruction at a time",mon_singlestep},
	{"c","continue the execution of user environment",mon_continue}
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


int mon_singlestep(int argc,char **argv,struct Trapframe *tf)
{
	if (tf==NULL)
	{
		cprintf("no running user environment!");
		return 1;
	}
	tf->tf_eflags|=FL_TF;    //将eflags中的标志位TF置1,开启单步跟踪
	return -1;               //返回-1从而退出monitor,回到用户environment

}

int mon_continue(int argc,char **argv,struct Trapframe *tf)
{
	if (tf==NULL)
	{
		cprintf("no running user environment!");
		return 1;
	}
	tf->tf_eflags&=~FL_TF;   //将eflags中的标志位TF清0,不再单步跟踪
	return -1;               //返回-1从而推出monitor,回到用户environment
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
			if (vabegin>=KERNBASE)  //由于KERNBASE上的虚拟内存到物理内存的映射均为-KERNBASE,只打印PDE,更为简洁且不损失信息
			{
				cprintf("  [%08x %08x]\n",vabegin-KERNBASE,vabegin-KERNBASE+PTSIZE-1);
				uintptr_t vainit;
				vainit=vabegin;   //由于处理高地址如0xffc00000时再加PTSIZE会导致整数上溢,加此判断
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


int mon_setclear(int argc,char **argv,struct Trapframe *tf)
{
	//cprintf("%d\n",argc);
	if (argc!=4)   //检查参数个数,输出提示
	{ 
		cprintf("this function takes 3 parameter,for example:\n");
		cprintf("0xf0000000 1 U\n");
		cprintf("the first parameter shows the address\nsecond parameter means clear(0) or set(1)\nthe third can be 'U' or 'W' or 'P' corresponding to each flag\n");
		return 0;
	}
	char *endptr;
	uintptr_t  va=strtol(argv[1],&endptr,16);
	if (*endptr)
	{
		cprintf("format error!\n");
		return 0; 
	}                    //将字符串转化为地址
	pde_t pagedirectoryentry=kern_pgdir[PDX(va)]; // 取得该地址对应的Pagedirectoryentry
	if (pagedirectoryentry&PTE_P)  //假设该PDE存在 
	{
		pte_t *pagetable=(pte_t *)(PTE_ADDR(pagedirectoryentry)+KERNBASE); //取对应的page table
		pte_t *  pagetableentry=&pagetable[PTX(va)]; //在page table中找到对应的Page table entry
		if (*pagetableentry&PTE_P) //假设要修改的PTE存在 
		{                          //根据传入参数修改标志位
			if (argv[2][0]=='1')
			{
				if (argv[3][0]=='U') *pagetableentry|=PTE_U;
				if (argv[3][0]=='P') *pagetableentry|=PTE_P;
				if (argv[3][0]=='W') *pagetableentry|=PTE_W;
			}
			else if (argv[2][0]=='0')
			{
				if (argv[3][0]=='U') *pagetableentry&=~PTE_U;
				if (argv[3][0]=='P') *pagetableentry&=~PTE_P;
				if (argv[3][0]=='W') *pagetableentry&=~PTE_W;
			}	
			return 0;
		}
	}
	cprintf("this virtual address is not mapped to physical address");
	return 0;
}


int mon_showvirtualmemory(int argc,char **argv,struct Trapframe *tf)
{
	if (argc!=3) //检查参数,与qemu monitor和gdb类似,该功能打印从虚拟地址addr开始的n个单元的内容(默认每单元4字节)
	{
		cprintf("this function takes exactly 2 parameters,for example\n");
		cprintf("x 10 0xf0000000\n");
		cprintf("means show the content of virtual address 0xf000000 for 10 units(4 bytes per unit like the qemu monitor)\n");
		return 0;
	}
	char *endptr;
	uintptr_t  va=strtol(argv[2],&endptr,16);
	if (*endptr)
	{
		cprintf("format error!\n");
		return 0;
	}
	int n=strtol(argv[1],&endptr,10);
	if (*endptr)
	{
		cprintf("format error!\n");
		return 0;
	}
	uint32_t * vapointer=(uint32_t *)va;
	int i=0;              
	for (i=0;i<n;i++)       //从指定虚拟地址开始,打印内存
	{
		if (i!=0&&i%4==0)   
			cprintf("\n");
		cprintf("0x%08x ",*vapointer);
		vapointer++; //指针指向下一个4字节
	}
	cprintf("\n");	
	return 0;
}

int mon_showphysicalmemory(int argc,char **argv,struct Trapframe *tf)
{
	if (argc!=3) //检查参数,与qemu monitor的xp功能类似,打印物理地址addr开始的n个4字节内容
	{
		cprintf("this function takes exactly 2 parameters,for example\n");
		cprintf("xp 10 0x00000000\n");
		cprintf("means show the content of physical address 0x0000000 for 10 units(4 bytes per unit like the qemu monitor)\n");
		return 0;
	}
	char *endptr;
	physaddr_t pa=strtol(argv[2],&endptr,16);	
	if (*endptr)
	{
		cprintf("format error!\n");
		return 0;
	}	
	int n=strtol(argv[1],&endptr,10);
	if (*endptr)
	{
		cprintf("format error!\n");
		return 0;
	}
	int i=0;
	uint32_t *vapointer=(uint32_t *)(KERNBASE+pa); //由于程序只能直接访问虚拟地址,进行虚拟地址到物理地址的转换
	for (i=0;i<n;i++)        
	{
		if (i!=0&&i%4==0)
		{
			cprintf("\n");
		}
		cprintf("0x%08x ",*vapointer);
		vapointer++;	 //指针指向下一个4字节	
	}
	cprintf("\n");	
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
//<<<<<<< HEAD

	if (tf != NULL)
		print_trapframe(tf);

//=======
//	int x=1,y=3,z=4;
//	cprintf("x %d, y %x, z %d\n",x,y,z);
//	unsigned int i=0x00646c72;
//	cprintf("H%x Wo%s",57616,&i);
//	cprintf("x=%d y=%d",3);
//>>>>>>> lab2
	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
