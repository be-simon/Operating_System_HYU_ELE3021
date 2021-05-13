#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
}ptable;

struct proc threadtable[NPROC * MAXTHREAD];
struct spinlock tlock;

extern int nextpid;
extern void forkret(void);
extern void trapret(void);


int 
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
	struct proc *master = myproc();
	struct proc *new;
	char *ksp;
	uint usp, sz, ustack[2];
	int i;

	acquire(&tlock);
	for (i = 0; i < MAXTHREAD; i++) {
		if (master->threads[i]->state == UNUSED) {
			new = master->threads[i];
			new->state = EMBRYO;
			break;
		}
	}
	release(&tlock);

	if (i >= MAXTHREAD) {
		panic("full thread");
		return -1;
	}

	new->tid = i;
	new->pgdir = master->pgdir;
	new->parent = master;
	*thread = new->tid;
	
	//allocate kstack
	if((new->kstack = kalloc()) == 0) {
	  new->state = UNUSED;			
	  return 0;
	}
	ksp = new->kstack + KSTACKSIZE;

	ksp -= sizeof *new->tf;
	new->tf = (struct trapframe*)ksp;

	ksp -=4;
	*(uint*)ksp = (uint)trapret;

	ksp -=sizeof *new->context;
	new->context = (struct context*)ksp;
	memset(new->context, 0, sizeof *new->context);
	new->context->eip = (uint)forkret;
	
	//allocate ustack
	acquire(&tlock);
	sz = master->sz;	
	if((new->sz = allocuvm(master->pgdir, sz, sz + 2 * PGSIZE)) == 0) {
		panic("thread ustack allocation fail");
		release(&tlock);
		return -1;
	}
	clearpteu(new->pgdir, (char*)(sz - 2 * PGSIZE));
	usp = new->sz;
	master->sz = new->sz;

	usp -= 2 * sizeof(uint);
	ustack[0] = 0xffffffff;
	ustack[1] = (uint)arg;
	if(copyout(new->pgdir, usp, ustack, 2 * sizeof(uint)) < 0) {
		panic("thread ustack copy fail");
		release(&tlock);
		return -1;
	}
	release(&tlock);

	*new->tf = *master->tf;
	new->tf->eip = (uint)start_routine;
	new->tf->esp = usp;
	
	return 0;
}


int 
thread_join(thread_t thread, void **retval)
{
	struct proc *master = myproc();
	struct proc *t_join = master->threads[thread];
	
	acquire(&tlock);
	while (t_join->state != ZOMBIE)
		sleep(master, &ptable.lock);

	//retrieve resource
	*retval = t_join->t_retval;
	t_join->state = UNUSED;

	//deallocate stack
	kfree(t_join->kstack);
	if(deallocuvm(t_join->pgdir, t_join->sz, t_join->sz - 2 * PGSIZE) == 0) {
		panic("thread ustack deallocation fail");
		release(&tlock);
		return -1;
	}

	release(&tlock);	

	return 0;
}


void 
thread_exit(void *retval)
{
	struct proc *curproc = myproc();
	struct proc *master = curproc->parent;

	wakeup(master);

	curproc->t_retval = retval;
	curproc->state = ZOMBIE;
	
	sched();
	panic("ZOMBIE thread exit");
}
