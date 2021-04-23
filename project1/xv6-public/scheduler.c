#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

struct scheduler mlfqsched = {{0, }, 0, TOTALTICKETS, 0, 0};
struct scheduler stridesched = {{0, }, 0, 0, 0, 0};

//int
//set_cpu_share(int share){
//}

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

		if (end->qlev > mlfqsched.heap[child]->qlev) {
			mlfqsched.heap[parent] = mlfqsched.heap[child];
			parent = child;
			child = parent * 2;
		} else 
			break;
	}

	if (parent > 1)
		mlfqsched.heap[parent] = p;

	return p;
}

//int
//stride_enqueue(int tickets, int stride, int pass, struct proc *p) {
//}



//int
//stride_dequeue(void) {
//}

