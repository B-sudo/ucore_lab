#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <assert.h>
#include <emalloc.h>
#include <sync.h>
#include <pmm.h>
#include <stdio.h>

//some helper
#define spin_lock_irqsave(l, f) local_intr_save(f)
#define spin_unlock_irqrestore(l, f) local_intr_restore(f)
#ifndef PAGE_SIZE
#define PAGE_SIZE PGSIZE
#endif

struct eslob_block {
	char flag;					//1 for allocated ; 0 for not
	struct eslob_manager *manager;
	struct eslob_block *next;
};
typedef struct eslob_block eslob_t;
#define ESLOB_T (sizeof(char) + sizeof(struct eslob_block *) + sizeof(struct eslob_manager *))

struct eslob_manager {
	int size;				//the number of obj
	int num;				//total num
	int nr_free;			//current available
	eslob_t * freeslob;		//the first empty obj
	struct eslob_manager *next;
};
typedef struct eslob_manager manager_t;
#define MANAGER_T (3 * sizeof(int) + sizeof(struct eslob_manager *) + sizeof(eslob_t *))

struct slob_units {
	manager_t *empty_next;
	manager_t *full_next;
};
typedef struct slob_units units_t;

units_t slob_list[EMALLOC_MAX_ORDER];

inline void 
emalloc_init(void) {
	int i;
	for (i = 0; i< EMALLOC_MAX_ORDER ; i++)
		slob_list[i].full_next = slob_list[i].empty_next = NULL;
	cprintf("emalloc_init() succeeded!\n");
};

int
Order(int size) {
	assert(size >= 0);
	
	int i=size/32;
	if (i < EMALLOC_MAX_ORDER - 1)
		return i;
	else
		return EMALLOC_MAX_ORDER - 1;
}
static void
eslob_init(manager_t *p, size_t size) {
	int num = (PGSIZE-MANAGER_T)/(size + ESLOB_T), i;
	eslob_t * slob;
	p->nr_free = p->num = num;
	p->size = size;
	p->next = NULL;
	slob = p->freeslob = (eslob_t *)((void *)p + MANAGER_T);
	for (i = 0; i < num - 1; i++) {
		slob->flag = 0;
		slob->manager = p;
		slob->next = (eslob_t *)((void *)slob + size + ESLOB_T);
		slob = slob->next;
	}
	slob->flag = 0;
	slob->manager = p;
	slob->next = NULL;
}

static void
Empty2Full(int order, manager_t *p) {						//from empty chain to full chain
	assert(p->nr_free == 0);
	if (slob_list[order].empty_next == p) {
		slob_list[order].empty_next = p->next;
		p->next = slob_list[order].full_next;
		slob_list[order].full_next = p;
	}
	else {
		manager_t *hook = slob_list[order].empty_next;
		while (hook->next != p && hook != NULL)
			hook = hook->next;
		if (hook == NULL)
			panic("wrong");
		hook->next = p->next;
		p->next = slob_list[order].full_next;
		slob_list[order].full_next = p;
	}		
}

static void
Full2Empty(int order, manager_t *p) {					//from full chain to empty chain
	assert(p->nr_free != 0);
	if (slob_list[order].full_next == p) {
		slob_list[order].full_next = p->next;
		p->next = slob_list[order].empty_next;
		slob_list[order].empty_next = p;
	}
	else {
		manager_t *hook = slob_list[order].full_next;
		while (hook->next != p && hook != NULL)
			hook = hook->next;
		if (hook == NULL)
			panic("wrong");
		hook->next = p->next;
		p->next = slob_list[order].empty_next;
		slob_list[order].empty_next = p;
	}		
}

static void
free_manager(manager_t *p) {
	assert(p != NULL);
	assert(p->nr_free == p->num);
	int order = Order(p->size);
	if (slob_list[order].empty_next == p)
		slob_list[order].empty_next = p->next;
	else {
		manager_t *hook = slob_list[order].empty_next;
		while (hook->next != p)
			hook = hook->next;
		hook->next = p->next;
	}
	struct Page *page = kva2page(p);
	free_pages(page, 1);
}

static void *
emalloc(size_t size){
	manager_t *p = slob_list[Order(size)].empty_next;
	
	while (p != NULL && p->size != size)
		p = p->next;
	
	if (p == NULL) {							//cannot get the proper pool
		struct Page * page = alloc_page();				//alloc a page for pool
		assert(page != NULL);
		p = (manager_t *)page2kva(page);
		eslob_init(p,size);
		
		p->next = slob_list[Order(size)].empty_next;
		slob_list[Order(size)].empty_next = p;
		
		eslob_t * slob = p->freeslob;
		slob->flag = 1;
		p->freeslob = slob->next;
		slob->next = NULL;
		p->nr_free--;
		if (p->freeslob == NULL)
			Empty2Full(Order(size), p);
		return ((void *)slob + ESLOB_T);
	}
	else if (p->size == size) {								//get the proper pool
		eslob_t * slob = p->freeslob;
		slob->flag = 1;
		p->freeslob = slob->next;
		slob->next = NULL;
		p->nr_free--;
		if (p->freeslob == NULL)
			Empty2Full(Order(size), p);
		return ((void *)slob + ESLOB_T);
	}
	else
		panic("error");
}

static void 
efree(void *objp) {
	eslob_t * slob = (eslob_t *)(objp - ESLOB_T);
	manager_t * p = slob->manager;
	int order = Order(p->size);
	
	slob->flag = 0;
	slob->next = p->freeslob;
	p->freeslob = slob;
	p->nr_free++;
	if (p->nr_free == 1)
		Full2Empty(order, p);
	if (p->nr_free == p->num)
		free_manager(p);
}

void
check_eslob() {
	int *p1 = emalloc(sizeof(int));
	int *p2 = emalloc(sizeof(int));
	long int *p3 = emalloc(sizeof(long int));
	long int *p4 = emalloc(sizeof(long int));
	
	assert(p1 != p2);
	assert(p3 != p4);
	
	efree(p1);
	efree(p2);
	efree(p3);
	efree(p4);
	cprintf("check_eslob() secceeded!\n");
}
