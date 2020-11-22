#ifndef __KERN_MM_EMALLOC_H__
#define __KERN_MM_EMALLOC_H__

#include <defs.h>
#define EMALLOC_MAX_ORDER       10

void emalloc_init(void);

static void *emalloc(size_t size);
static void efree(void *objp);
void check_eslob(void);

#endif /* !__KERN_MM_EMALLOC_H__ */
