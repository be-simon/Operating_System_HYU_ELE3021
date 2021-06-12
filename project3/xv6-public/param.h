#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       40000  // size of file system in blocks

// MLFQ
#define TQHIGH			5
#define TQMIDDLE		10
#define TQLOW			20
#define TAHIGH			20
#define TAMIDDLE		40
#define PRIORITYBOOST	200

// Stride
#define TOTALTICKETS	10000
#define MAXSHARE			80
#define TQSTRIDE		5

// Thread
#define MAXTHREAD			10
