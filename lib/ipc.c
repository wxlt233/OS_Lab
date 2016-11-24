// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
	if (perm_store)
		*perm_store=0;
	if (from_env_store)
		*from_env_store=0;
	int t;
	if (pg!=NULL)        //如果pg不为NULL,则将其作为dstva传入sys_ipc_recv
		 t=sys_ipc_recv(pg);
	else 
		t=sys_ipc_recv((void*)UTOP);//否则传入一个>=UTOP的值,sys_ipc_recv则当作不接收页来处理
	if (t)	
		return t;	
	if (from_env_store!=NULL) //如果from_env_store不为NULL
	{ 
		*from_env_store=thisenv->env_ipc_from;//将发送信息的environment的envid存在*from_env_store
	}
	if (perm_store) //如果perm_store不为NULL
	{
		*perm_store=thisenv->env_ipc_perm;//将对应的标志位传入*perm_store
	}
	return thisenv->env_ipc_value;   //返回IPC传送的值
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	int t; 
	while (1)        //不断尝试发送
	{
		if (pg!=NULL) //如果pg不为NULL
			t=sys_ipc_try_send(to_env,val,pg,perm);
			//传送值val,页pg,标志位perm给目的environment to_env
		else 
			t=sys_ipc_try_send(to_env,val,(void *)UTOP,0);
			//否则仅传送值val
		if (t==0) //如果t==0,说明传送成功,退出
			break;
		if (t!=-E_IPC_NOT_RECV) //假设出错且不为-E_IPC_NOT_RECV,panic
		{	
			cprintf("%e\n",t);	
			panic("not ipc not recv!");
		}
		sys_yield(); //使用sys_yield进行CPU调度,运行其他可运行的environment
	}
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
