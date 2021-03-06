/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
	
	//定义各类异常/中断处理函数,其中不需要压入错误码的使用TRAPHANDLER_NOEC
	//需要压入错误码的使用TRAPHANDLER,两个参数分别为对应的函数名称及trapnumber
	TRAPHANDLER_NOEC(handler0,T_DIVIDE)        
	TRAPHANDLER_NOEC(handler1,T_DEBUG)         
	TRAPHANDLER_NOEC(handler2,T_NMI)
	TRAPHANDLER_NOEC(handler3,T_BRKPT)
	TRAPHANDLER_NOEC(handler4,T_OFLOW)
	TRAPHANDLER_NOEC(handler5,T_BOUND)
	TRAPHANDLER_NOEC(handler6,T_ILLOP)
	TRAPHANDLER_NOEC(handler7,T_DEVICE)
	TRAPHANDLER(handler8,T_DBLFLT)
	//TRAPHANDLER_NOEC(handler9,9)
	TRAPHANDLER(handler10,T_TSS)
	TRAPHANDLER(handler11,T_SEGNP)
	TRAPHANDLER(handler12,T_STACK)
	TRAPHANDLER(handler13,T_GPFLT)
	TRAPHANDLER(handler14,T_PGFLT)	
	//TRAPHANDLER(handler15,15)
	TRAPHANDLER_NOEC(handler16,T_FPERR)
	TRAPHANDLER_NOEC(handler17,T_ALIGN)
	TRAPHANDLER_NOEC(handler18,T_MCHK)
	TRAPHANDLER_NOEC(handler19,T_SIMDERR)	
	TRAPHANDLER_NOEC(handler48,T_SYSCALL)
	
	TRAPHANDLER_NOEC(irqhandler0,IRQ_OFFSET+IRQ_TIMER)
	TRAPHANDLER_NOEC(irqhandler1,IRQ_OFFSET+IRQ_KBD)
	TRAPHANDLER_NOEC(irqhandler2,IRQ_OFFSET+2)
	TRAPHANDLER_NOEC(irqhandler3,IRQ_OFFSET+3)
	TRAPHANDLER_NOEC(irqhandler4,IRQ_OFFSET+IRQ_SERIAL)
	TRAPHANDLER_NOEC(irqhandler5,IRQ_OFFSET+5)
	TRAPHANDLER_NOEC(irqhandler6,IRQ_OFFSET+6)
	TRAPHANDLER_NOEC(irqhandler7,IRQ_OFFSET+IRQ_SPURIOUS)
	TRAPHANDLER_NOEC(irqhandler8,IRQ_OFFSET+8)
	TRAPHANDLER_NOEC(irqhandler9,IRQ_OFFSET+9)
	TRAPHANDLER_NOEC(irqhandler10,IRQ_OFFSET+10)
	TRAPHANDLER_NOEC(irqhandler11,IRQ_OFFSET+11)
	TRAPHANDLER_NOEC(irqhandler12,IRQ_OFFSET+12)
	TRAPHANDLER_NOEC(irqhandler13,IRQ_OFFSET+13)
	TRAPHANDLER_NOEC(irqhandler14,IRQ_OFFSET+IRQ_IDE)
	TRAPHANDLER_NOEC(irqhandler15,IRQ_OFFSET+15)
	
/*
 * Lab 3: Your code here for _alltraps
 */
 _alltraps:
	pushl %ds            //根据trapframe的结构,依次将ds,es及pushal所包含的通用寄存器压栈
	pushl %es
	pushal          
	movl $GD_KD,%eax
	mov %eax,%ds         //将段选择符装入DS和ES寄存器,GD_KD为内核数据段
	mov %eax,%es
	pushl %esp           //将trapframe的地址(即esp)压栈,作为trap函数的参数
	call trap            //调用trap函数进行进一步的异常/中断处理





