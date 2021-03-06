/*	$OpenBSD: alloc.c,v 1.16 2017/05/29 13:09:17 tb Exp $	*/

/* Public domain, like most of the rest of ksh */

/*
 * area-based allocation built on malloc/free
 */

#include <stdint.h>
#include <stdlib.h>

#include "sh.h"

struct link {
	struct link *prev;
	struct link *next;
};

Area *
ainit(Area *ap)
{
	ap->freelist = NULL;
	return ap;
}

void
afreeall(Area *ap)
{
	struct link *l, *l2;

	for (l = ap->freelist; l != NULL; l = l2) {
		l2 = l->next;
		free(l);
	}
	ap->freelist = NULL;
}

#define L2P(l)	( (void *)(((char *)(l)) + sizeof(struct link)) )
#define P2L(p)	( (struct link *)(((char *)(p)) - sizeof(struct link)) )

void *
alloc(size_t size, Area *ap)
{
	struct link *l;

	/* ensure that we don't overflow by allocating space for link */
	if (size > SIZE_MAX - sizeof(struct link))
		internal_errorf(1, "unable to allocate memory");

	l = calloc(1, sizeof(struct link) + size);
	if (l == NULL)
		internal_errorf(1, "unable to allocate memory");
	l->next = ap->freelist;
	l->prev = NULL;
	if (ap->freelist)
		ap->freelist->prev = l;
	ap->freelist = l;

	return L2P(l);
}

/*
 * Copied from calloc().
 *
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	(1UL << (sizeof(size_t) * 4))

void *
areallocarray(void *ptr, size_t nmemb, size_t size, Area *ap)
{
	/* condition logic cloned from calloc() */
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		internal_errorf(1, "unable to allocate memory");
	}

	return aresize(ptr, nmemb * size, ap);
}

void *
aresize(void *ptr, size_t size, Area *ap)
{
	struct link *l, *l2, *lprev, *lnext;

	if (ptr == NULL)
		return alloc(size, ap);

	/* ensure that we don't overflow by allocating space for link */
	if (size > SIZE_MAX - sizeof(struct link))
		internal_errorf(1, "unable to allocate memory");

	l = P2L(ptr);
	lprev = l->prev;
	lnext = l->next;

	l2 = realloc(l, sizeof(struct link) + size);
	if (l2 == NULL)
		internal_errorf(1, "unable to allocate memory");
	if (lprev)
		lprev->next = l2;
	else
		ap->freelist = l2;
	if (lnext)
		lnext->prev = l2;

	return L2P(l2);
}

void
afree(void *ptr, Area *ap)
{
	struct link *l, *l2;

	if (!ptr)
		return;

	l = P2L(ptr);

	for (l2 = ap->freelist; l2 != NULL; l2 = l2->next) {
		if (l == l2)
			break;
	}
	if (l2 == NULL)
		internal_errorf(1, "afree: %p not present in area %p", ptr, ap);

	if (l->prev)
		l->prev->next = l->next;
	else
		ap->freelist = l->next;
	if (l->next)
		l->next->prev = l->prev;

	free(l);
}
