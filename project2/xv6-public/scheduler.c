#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

struct scheduler mlfqsched = {{0, }, 0, TOTALTICKETS, 1, 0};
struct scheduler stridesched = {{0, }, 0, 0, 0, 0};
uint mlfqticks = 0;

int
set_cpu_share(int share){
	int tickets = TOTALTICKETS * share / 100;
	if (stridesched.tickets + tickets > TOTALTICKETS * MAXSHARE / 100)
		return -1;

	mlfqsched.tickets -= tickets;
	stridesched.tickets += tickets;

	if (stridesched.stride == 0)
		stridesched.pass = mlfqsched.pass;

	mlfqsched.stride = TOTALTICKETS / mlfqsched.tickets;
	stridesched.stride = TOTALTICKETS / stridesched.tickets;

	myproc()->tickets = tickets;
	myproc()->stride = stridesched.tickets / tickets;
	if (stridesched.count > 0)
		myproc()->pass = stridesched.heap[1]->pass;
	else
		myproc()->pass = 0;

	stride_enqueue(myproc());

	return 0;
}

int
mlfq_enqueue(struct proc *p){
	// queue is full
	if (mlfqsched.count >= NPROC){
		return -1;
	}

	int parent;
	int child;
	
	child = ++mlfqsched.count;
	parent = child / 2;
	
	while (child > 1) {
		if (p->qlev < mlfqsched.heap[parent]->qlev) {
			mlfqsched.heap[child] = mlfqsched.heap[parent];
			child = parent;
			parent = child / 2;
		} else
			break;
	}
	mlfqsched.heap[child] = p;

	return 0;
}

struct proc*
mlfq_dequeue(void) {
	// heap is empty
	if (mlfqsched.count == 0){
		return 0;
	}

	struct proc *p = mlfqsched.heap[1];
	struct proc *end = mlfqsched.heap[mlfqsched.count--];
	int parent = 1;
	int child = 2;

	while (child <= mlfqsched.count) {
		if (child < mlfqsched.count) {
			child = (mlfqsched.heap[child]->qlev < mlfqsched.heap[child + 1]->qlev) ? child : child + 1;
		}

		if (end->qlev >= mlfqsched.heap[child]->qlev) {
			mlfqsched.heap[parent] = mlfqsched.heap[child];
			parent = child;
			child = parent * 2;
		} else 
			break;
	}

	if (mlfqsched.count > 0)
		mlfqsched.heap[parent] = end;

	return p;
}

int
stride_enqueue(struct proc *p){
	// queue is full
	if (stridesched.count >= NPROC){
		return -1;
	}

	int parent;
	int child;
	
	child = ++stridesched.count;
	parent = child / 2;
	
	while (child > 1) {
		if (p->pass < stridesched.heap[parent]->pass) {
			stridesched.heap[child] = stridesched.heap[parent];
			child = parent;
			parent = child / 2;
		} else
			break;
	}
	stridesched.heap[child] = p;

	return 0;
}

struct proc*
stride_dequeue(void) {
	// heap is empty
	if (stridesched.count == 0){
		return 0;
	}

	struct proc *p = stridesched.heap[1];
	struct proc *end = stridesched.heap[stridesched.count--];
	int parent = 1;
	int child = 2;

	while (child <= stridesched.count) {
		if (child < stridesched.count) {
			child = (stridesched.heap[child]->pass < stridesched.heap[child + 1]->pass) ? child : child + 1;
		}

		if (end->pass >= stridesched.heap[child]->pass) {
			stridesched.heap[parent] = stridesched.heap[child];
			parent = child;
			child = parent * 2;
		} else 
			break;
	}

	if (stridesched.count > 0)
		stridesched.heap[parent] = end;

	return p;
}

void mlfq_check_on_timer(){
	struct proc *p = myproc()->master;
	int ta = TAHIGH, tq = TQHIGH;

	// set tq & ta
	switch(p->qlev){
		case 0:
			tq = TQHIGH;
			ta = TAHIGH;
			break;
		case 1:
			tq = TQMIDDLE;
			ta = TAMIDDLE;
			break;
		case 2:
			tq = TQLOW;
			break;
	}

	if (p->qlev < 2 && p->qticks % ta == 0){
		//check time allotment
		p->qlev++;
		p->qticks = 0;
		p->isexhausted = 1;
	} else if (p->qticks > 0 && p->qticks % tq == 0)
		// check time quantum
		p->isexhausted = 1;
	else
		p->isexhausted = 0;
}
