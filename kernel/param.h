#define NPROC       64                // maximum number of processes
#define NCPU        8                 // maximum number of CPUs
#define NOFILE      16                // open files per process
#define NFILE       100               // open files per system
#define NINODE      50                // maximum number of active i-nodes
#define NDEV        10                // maximum major device number
#define ROOTDEV     1                 // device number of file system root disk
#define MAXARG      32                // max exec arguments
#define MAXOPBLOCKS 10                // max # of blocks any FS op writes
#define LOGBLOCKS   (MAXOPBLOCKS * 3) // max data blocks in on-disk log
#define NBUF        (MAXOPBLOCKS * 3) // size of disk block cache
#define FSSIZE      2000              // size of file system in blocks
#define MAXPATH     128               // maximum file path name
#define USERSTACK   1                 // user stack pages

// scheduler (see kernel/sched.c and q4-spec.md)
#define NSCHEDLOG     4096 // events kept for schedlog()
#define DEF_PRIORITY  5    // 0 is the highest priority, 9 the lowest
#define DEF_BURST     2    // initial CPU burst estimate, in ticks
#define MLFQ_NLEVEL   4
#define MLFQ_SLICE0   1    // L0 time slice
#define MLFQ_SLICE2   2    // L2 re-checks priorities after this many ticks
#define MLFQ_ALLOT0   2    // CPU ticks one burst may use in L0 before demotion
#define MLFQ_ALLOT1   4
#define MLFQ_ALLOT2   8
#define MLFQ_AGE_STEP 2    // L2: +1 priority per this many waiting ticks
#define MLFQ_STARVE   20   // L3: promote to L2 after waiting this many ticks
