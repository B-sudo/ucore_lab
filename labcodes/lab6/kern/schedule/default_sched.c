#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

#define USE_SKEW_HEAP 0
#define USE_RB_TREE 1

/* You should define the BigStride constant here*/
/* LAB6: YOUR CODE */
#define BIG_STRIDE    0x7FFFFFFF /* ??? */
#define THRESHOLD		1024

const int sched_prio_to_weight[40] = {
	/* -20 */	88761,	71755,	56483,	46273,	36291,
	/* -15 */	29154,	23254,	18705,	14949,	11916,	
	/* -10 */	9548,	7620,	6100,	4904,	3906,
	/* -5 */	3121,	2501,	1991,	1586,	1277,	
	/* 0 */		1024,	820,	655,	526,	423,
	/* 5 */		335,	272,	215,	172,	137,
	/* 10 */	110,	87,		70,		56,		45,
	/* 15 */	36,		29,		23,		18,		15,
};

int nice_to_prio(int nice) {
	// nice -20 ~ 19
	return sched_prio_to_weight[nice + 20];
}

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
static int
proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, lab6_run_pool);
     struct proc_struct *q = le2proc(b, lab6_run_pool);
     int32_t c = p->lab6_stride - q->lab6_stride;
     if (c > 0) return 1;
     else if (c == 0) return 0;
     else return -1;
}

/* The compare function for two rb_node's and the
 * corresponding procs*/
static int
proc_cfs_comp_f(void *a, void *b)
{
	struct proc_struct *p = le2proc(a, cfs_run_pool);
	struct proc_struct *q = le2proc(b, cfs_run_pool);
	int32_t c = p->vruntime - q->vruntime;
	if (c > 0) return 1;
	else if (c == 0) return 0;
	else return -1;
}

/*
 * stride_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be a empty list after initialization.
 *   - lab6_run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: no need here, the variable would be assigned by the caller.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void
stride_init(struct run_queue *rq) {
     /* LAB6: YOUR CODE */
     list_init(&(rq->run_list));
     rq->lab6_run_pool = NULL;
     rq->proc_num = 0;
}

/*
 * stride_enqueue inserts the process ``proc'' into the run-queue
 * ``rq''. The procedure should verify/initialize the relevant members
 * of ``proc'', and then put the ``lab6_run_pool'' node into the
 * queue(since we use priority queue here). The procedure should also
 * update the meta date in ``rq'' structure.
 *
 * proc->time_slice denotes the time slices allocation for the
 * process, which should set to rq->max_time_slice.
 * 
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
#if USE_SKEW_HEAP
     rq->lab6_run_pool =
          skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
#else
     assert(list_empty(&(proc->run_link)));
     list_add_before(&(rq->run_list), &(proc->run_link));
#endif
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
          proc->time_slice = rq->max_time_slice;
     }
     proc->rq = rq;
     rq->proc_num ++;
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
#if USE_SKEW_HEAP
     rq->lab6_run_pool =
          skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
#else
     assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
     list_del_init(&(proc->run_link));
#endif
     rq->proc_num --;
}
/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see proj13.1/kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
     /* LAB6: YOUR CODE */
#if USE_SKEW_HEAP
     if (rq->lab6_run_pool == NULL) return NULL;
     struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
#else
     list_entry_t *le = list_next(&(rq->run_list));

     if (le == &rq->run_list)
          return NULL;
     
     struct proc_struct *p = le2proc(le, run_link);
     le = list_next(le);
     while (le != &rq->run_list)
     {
          struct proc_struct *q = le2proc(le, run_link);
          if ((int32_t)(p->lab6_stride - q->lab6_stride) > 0)
               p = q;
          le = list_next(le);
     }
#endif
     if (p->lab6_priority == 0)
          p->lab6_stride += BIG_STRIDE;
     else p->lab6_stride += BIG_STRIDE / p->lab6_priority;
     return p;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     if (proc->time_slice > 0) {
          proc->time_slice --;
     }
     if (proc->time_slice == 0) {
          proc->need_resched = 1;
     }
}

static void
cfs_init(struct run_queue *rq) {
	rq->proc_num = 0;
	rq->rb_run_pool = rb_tree_create(proc_cfs_comp_f);
	rq->min_vruntime = 0;
}

static void
cfs_enqueue(struct run_queue *rq, struct proc_struct *proc) {
	if (proc->vruntime < rq->min_vruntime + nice_to_prio(proc->nice))
		proc->vruntime = rq->min_vruntime + nice_to_prio(proc->nice);
	rb_insert(rq->rb_run_pool, &proc->cfs_run_pool);

	rb_node *le = rb_min_search(rq->rb_run_pool);
	struct proc_struct* p = le2proc(le, cfs_run_pool);
	rq->min_vruntime = p->vruntime;	

	rq->proc_num++;
	proc->rq = rq;
}

static void
cfs_dequeue(struct run_queue *rq, struct proc_struct *proc) {
	rb_delete(rq->rb_run_pool, &proc->cfs_run_pool);
	rq->proc_num--;
	if (rq->proc_num == 0)	//empty tree
		rq->min_vruntime = 0;
	else {
		rb_node *le = rb_min_search(rq->rb_run_pool);
		struct proc_struct* p = le2proc(le, cfs_run_pool);
		rq->min_vruntime = p->vruntime;	
	}
}

static void
cfs_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
	proc->vruntime += nice_to_prio(proc->nice);
	if (proc->vruntime > rq->min_vruntime + THRESHOLD)
		proc->need_resched = 1;
}

static struct proc_struct *
cfs_pick_next(struct run_queue *rq) {
	rb_node *le = rb_min_search(rq->rb_run_pool);
	if (le == NULL)
		return NULL;
	struct proc_struct* p = le2proc(le, cfs_run_pool);
	return p;
}

struct sched_class default_sched_class = {
	.name = "cfs_scheduler",
	.init = cfs_init,
	.enqueue = cfs_enqueue,
	.dequeue = cfs_dequeue,
	.pick_next = cfs_pick_next,
	.proc_tick = cfs_proc_tick,
     /*.name = "stride_scheduler",
     .init = stride_init,
     .enqueue = stride_enqueue,
     .dequeue = stride_dequeue,
     .pick_next = stride_pick_next,
     .proc_tick = stride_proc_tick,*/
};

