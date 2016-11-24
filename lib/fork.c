// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va; //取得发生page fault的虚拟地址
	uint32_t err = utf->utf_err;
	int r;
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
	// LAB 4: Your code here.
	if (!(err&FEC_WR)&&(uvpd[PDX(addr)]&PTE_P)&&(uvpt[PGNUM(addr)]&PTE_P)&&(uvpt[PGNUM(addr)]&PTE_COW))
	//检查当前的page fault是由写操作造成的,且目标是一个标志位为PTE_COW的页
	{
		
		panic("pgfault check failed!");
	}
	addr=ROUNDDOWN(addr,PGSIZE);
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	// LAB 4: Your code here.
	int t=sys_page_alloc(0,PFTEMP,PTE_P|PTE_U|PTE_W); //分配一个新的页,将其映射在PFTEMP处
	if (t)
		panic("sys_page_alloc failed!");
	memcpy(PFTEMP,addr,PGSIZE);                 //将addr所在页的内容复制到临时内存
	t=sys_page_map(0,PFTEMP,0,addr,PTE_P|PTE_U|PTE_W);//使用sys_page_map,将PFTEMP处的页映射至addr处
	if (t)
	{	
		cprintf("%e\n",t);
		panic("sys_page_map failed!");	
	}
	t=sys_page_unmap(0,PFTEMP);              //取消PFTEMP处的页
	if (t)
		panic("sys_page_unmap failed!");	
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *addr=(void *)(pn*PGSIZE);//获取pn页对应的地址

	if (uvpt[pn]&PTE_W||uvpt[pn]&PTE_COW)  //判断当前页的标志位是否为W或COW
	{
		r=sys_page_map(0,addr,envid,addr,PTE_COW|PTE_U|PTE_P);
		//将当前页映射至对应envid的environment,标志位需添加PTE_COW
		if (r)
		{
			panic("sys_page_map failed!");
		}
		r=sys_page_map(0,addr,0,addr,PTE_COW|PTE_U|PTE_P);
		//将当前页映射至本进程原处,标志位为PTE_COW|PTE_U|PTE_P
		if (r)
		{	
			panic("sys_page_map failed!");
		}
	}
	else r=sys_page_map(0,addr,envid,addr,PTE_P|PTE_U);
	//如果为只读页,只需简单映射即可
	if (r)
		panic("sys_page_map failed!");
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{	
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault); //设置page fault handler
	envid_t envid;
	envid=sys_exofork(); //使用sys_exofork生成一个子environment
	if (envid==0)   //若envid为0,说明为子environment,只需设置thisenv并return 0即可
	{
		thisenv=&envs[ENVX(sys_getenvid())];
		return 0;
	}
	if (envid<0) //若小于0,说明sys_exofork出错,panic
		panic("sys_exofork failed!");
	uint32_t  i=0;
	for (i=0;i<USTACKTOP;i+=PGSIZE)             //遍历USTACKTOP以下的地址空间
	{
		if ((uvpd[PDX(i)]&PTE_P)&&(uvpt[PGNUM(i)]&PTE_P)&&(uvpt[PGNUM(i)]&PTE_U))
		//如果该页在父environment的地址空间中,使用duppage复制映射
		{
			duppage(envid,PGNUM(i));
		}
	}
	int t=sys_page_alloc(envid,(void *)(UXSTACKTOP-PGSIZE),PTE_U|PTE_W|PTE_P);
	//为子environment分配user exception stack
	if (t)
	{
		panic("sys_page_alloc failed!");
	}	
	extern void _pgfault_upcall();
	t=sys_env_set_pgfault_upcall(envid,_pgfault_upcall);//为子environment设置pgfault_upcall
	t=sys_env_set_status(envid,ENV_RUNNABLE);     //将子environment设为可以运行
	if (t)
		panic("sys_env_status failed!");
	return envid;	 //返回子environment的envid
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
