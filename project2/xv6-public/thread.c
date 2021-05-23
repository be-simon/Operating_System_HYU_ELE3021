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

extern int nextpid;
extern void forkret(void);
extern void trapret(void);

struct spinlock vmlock;

struct proc*
allocthread(struct proc *master)
{
  struct proc *p;
  char *sp;
	int i;

	if (master->t_cnt >= MAXTHREAD) {
		panic("full thread");
		return 0;
	}

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

	p->qlev = 0;
	p->qticks = 0;
	p->tickets = 0;
	p->pass = 0;

	for (i = 0; i < MAXTHREAD; i++) {
		if (master->threads[i] == 0){
			master->threads[i] = p;
			p->tid = i;
			break;
		}
	}
	master->t_cnt++;

	p->isthread = 1;

  return p;
}

int
t_allocustack(struct proc *master, struct proc *thread)
{
	int i;
	uint sz;

	acquire(&vmlock);
	for (i = 0; i < MAXTHREAD; i++) {
		if ((sz = master->freeupages[i]) != 0) {
			if((thread->sz = allocuvm(master->pgdir, sz, sz + 2 * PGSIZE)) == 0) {
				panic("thread ustack allocation fail");
				release(&vmlock);
				return -1;
			}
			master->freeupages[i] = 0;
			break;
		}
	}
	if (i >= MAXTHREAD) {
		sz = master->sz;
		if((thread->sz = allocuvm(master->pgdir, sz, sz + 2 * PGSIZE)) == 0) {
			panic("thread ustack allocation fail");
			release(&vmlock);
			return -1;
		}
		master->sz = thread->sz;
	}

	clearpteu(master->pgdir, (char*)(thread->sz - 2 * PGSIZE));
	release(&vmlock);
	return 0;
}

int
t_deallocustack(struct proc *thread)
{
	uint sz;
	int i;

	acquire(&vmlock);
	kfree(thread->kstack);

	if((sz = deallocuvm(thread->pgdir, thread->sz, thread->sz - 2 * PGSIZE)) == 0) {
		panic("thread ustack deallocation fail");
		release(&vmlock);
		return -1;
	}
	for (i = 0; i < MAXTHREAD; i++) {
		if (thread->master->freeupages[i] == 0){
			thread->master->freeupages[i] = sz;
			break;
		}
	}	

	release(&vmlock);
	return 0;
}

int 
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
	struct proc *master = myproc();
	struct proc *new;
	uint sp, ustack[2];
	int i;

	// thread & kernel stack allocation
	if((new = allocthread(master)) == 0){
		panic("thread alloc fail"); 
		return -1;
	}
	*thread = new->tid;
	cprintf("thread_create: *thread: %d\n", *thread);
	new->pgdir = master->pgdir;

	// user stack allocation
	if(t_allocustack(master, new) < 0){
		return -1;
	}
	sp = new->sz;

	ustack[0] = 0xffffffff;
	ustack[1] = (uint)arg;
	sp -= sizeof(ustack);
	if(copyout(new->pgdir, sp, ustack, sizeof(ustack)) < 0) {
		panic("thread ustack copy fail");
		return -1;
	}

	new->master = master;
	*new->tf = *master->tf;
	//new->tf->eax = 0;
	new->tf->eip = (uint)start_routine;
	new->tf->esp = sp;

	// file descriptor
	for (i = 0; i <NOFILE; i++)
		if (master->ofile[i])
			new->ofile[i] = master->ofile[i];
	new->cwd = master->cwd;

	acquire(&ptable.lock);
	new->state = RUNNABLE;
	release(&ptable.lock);
	
	return 0;
}


int 
thread_join(thread_t thread, void **retval)
{
	struct proc *master = myproc();
	struct proc *t_join = master->threads[thread];

	cprintf("t_join: %d\n", thread);	
	acquire(&ptable.lock);
	while (t_join->state != ZOMBIE)
		sleep(master, &ptable.lock);

	if (t_deallocustack(t_join) < 0){
		release(&ptable.lock);
		return -1;
	}

	//retrieve resource
	*retval = master->t_retval[t_join->tid];
	master->threads[t_join->tid] = 0;
	master->t_cnt--;
	t_join->kstack = 0;
	t_join->master = 0;
	t_join->tid = 0;
	t_join->killed = 0;
	t_join->name[0] = 0;
	t_join->state = UNUSED;

	cprintf("t_join: %d finish\n", thread);	
	release(&ptable.lock);	

	return 0;
}


void 
thread_exit(void *retval)
{
	struct proc *curproc = myproc();
	struct proc *master = curproc->master;
	struct proc *p;
	int fd;

	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd]){
			curproc->ofile[fd] = 0;
		}
	}
	curproc->cwd = 0;

	acquire(&ptable.lock);
	curproc->state = ZOMBIE;
	//wakeup(master);
	master->t_retval[curproc->tid] = retval;
	//master->t_cnt--;
	cprintf("thread_exit: isthread: %d, tid: %d\n", curproc->isthread, curproc->tid);

	for (p = ptable.proc; p <&ptable.proc[NPROC]; p++) 
		if (p->state == SLEEPING && p->chan == (void*)master){
			p->state = RUNNABLE;	
			p->qlev = 0;
		}

	sched();
	panic("ZOMBIE thread exit");
}

struct proc*
get_next_thread(struct proc *master)
{
	int i;
	struct proc *next = master;
	if (master->t_cnt == 0)
		return master;

	i = (master->t_lastsched + 1) % (MAXTHREAD + 1);
	while (i != master->t_lastsched) {
		if (i == MAXTHREAD) {
			if (master->state == RUNNABLE) {
				next = master;
				break;
			}
		}
		else if (master->threads[i] != 0 && master->threads[i]->state == RUNNABLE) {
			next = master->threads[i];
			break;
		}
		i = (i + 1) % (MAXTHREAD + 1);
	}
	if (i == master->t_lastsched){ 
		if (i == MAXTHREAD && master->state == RUNNABLE)
			next = master;
		else if (master->threads[i]->state == RUNNABLE)
			next = master->threads[i];
	}
	master->t_lastsched = i;

	if (next == master && master->state != RUNNABLE)
		next = 0;

	return next;
}

void
run_next_thread()
{
	struct proc *p = myproc();
	struct cpu *c = mycpu();
	struct proc *next;
	next = get_next_thread(p->master);
	if (next == p->master && p->master->state == RUNNABLE)
		cprintf("curproc: %d, next is master\n", p->tid);
//	cprintf("run_next_thread: isthread: %d, pid: %d, tid: %d, t_cnt: %d, nexttid: %d\n", p->isthread, p->pid, p->tid, p->master->t_cnt, next->tid);

	if (!next) 
		swtch(&(p->context), c->scheduler);
	else {

		c->proc = next;	

		if (next->kstack == 0)
			panic("run thread: no kstack");
		if (next->pgdir == 0)
			panic("run thread: no pgdir");

		pushcli();
		mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts, sizeof(mycpu()->ts)-1, 0);

		mycpu()->gdt[SEG_TSS].s = 0;
		mycpu()->ts.ss0 = SEG_KDATA << 3;
		mycpu()->ts.esp0 = (uint)next->kstack + KSTACKSIZE;

		mycpu()->ts.iomb = (ushort) 0xFFFF;
		ltr(SEG_TSS << 3);
		popcli();


		//	acquire(&ptable.lock);
		//	curproc->state = RUNNABLE;
		//	intena = mycpu()->intena;
		//	switchuvm(next);
		next->state = RUNNING;
		swtch(&(p->context), next->context);
		//	switchkvm();

	}

//	mycpu()->intena = intena;
//	release(&ptable.lock);
}
