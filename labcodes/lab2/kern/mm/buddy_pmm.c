#include <pmm.h>
#include <string.h>
#include <buddy_pmm.h>

int IS_POWER_OF_2(size_t size) {
	if (size & (size - 1) == 0)
		return 1;
	else 
		return  0;
}

size_t LEFT_LEAF(size_t parent) {
	return (parent + 1) * 2 - 1;
}

size_t RIGHT_LEAF(size_t parent) {
	return (parent + 1) *2;
}

size_t PARENT(size_t child) {
	if (child == 0)
		panic("bad child node");
	return (child + 1) / 2 -1;
}

size_t fixsize(size_t n) {
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	if (n < 0)
		panic("too large size");
	return n + 1;
}

size_t MAX(size_t a, size_t b) {
	return a >= b ? a : b;
}

struct buddy2 buddy_head;
struct buddy2 *buddy = &buddy_head;
#define self buddy

static void
buddy_init(void) {
	buddy->nr_free = 0;
	buddy->size = 0;
	buddy->longest = NULL;
}

static void
buddy_init_memmap(struct Page *base, size_t size) {
    size_t node_size;
	int i;
	size_t pages[size];
	
	if (size < 1 || !IS_POWER_OF_2(size))
		return NULL;
	
	self->nr_free = self->size = size;
	self->longest = pages;
	node_size = size * 2;
	
	for (i = 0; i < 2 * size - 1; ++i) {
		if (IS_POWER_OF_2(i+1))
			node_size /= 2;
		self->longest[i] = node_size;
	}
}
static size_t 
buddy_alloc(size_t size){
	size_t index = 0;
	size_t node_size;
	size_t offset = 0;
	
	if (self == NULL)
		panic("wrong buddy");
	
	if (size < 0)
		panic("bad size");
	else if (size == 0)
		size = 1;
	else
		size = fixsize(size - 1);
	
	if (self->longest[index] < size)
		panic("too large");
	
	for(node_size = self->size; node_size != size; node_size /= 2) {
		if (self->longest[LEFT_LEAF(index)] >= size)
			index = LEFT_LEAF(index);
		else
			index = RIGHT_LEAF(index);
	}
	
	self->longest[index] = 0;
	offset = (index + 1) * node_size - self->size;
	
	while(index) {
		index = PARENT(index);
		self->longest[index] = 
			MAX(self->longest[LEFT_LEAF(index)], self->longest[RIGHT_LEAF(index)]);
	}
	self->nr_free -= size;
	return offset;
}

static void 
buddy_free(struct Page *base, size_t offset) {
	size_t node_size, index = 0;
	size_t left_longest, right_longest;
	
	assert(self && offset >=0 && offset < self->size);
	
	node_size = 1;
	index = offset + self->size - 1;
	
	for (; self->longest[index] ; index = PARENT(index)) {
		node_size *= 2;
		if (index == 0)
			return ;
	}
	self->longest[index] = node_size;
	self->nr_free += node_size;
	
	while (index) {
		index = PARENT(index);
		node_size *= 2;
		
		left_longest = self->longest[LEFT_LEAF(index)];
		right_longest = self->longest[RIGHT_LEAF(index)];
		
		if (left_longest + right_longest == node_size)
			self->longest[index] = node_size;
		else
			self->longest[index] = MAX(left_longest, right_longest);
	}
}

static size_t
buddy_nr_free(void) {
    return self->nr_free;
}

static void
basic_check(void) {}

static void
default_check(void) {}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc,
    .free_pages = buddy_free,
    .nr_free_pages = buddy_nr_free,
    .check = default_check,
};
