#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "dllist.h"
#include "cache.h"
#include <stdio.h>
/*
    There are some subtleties in this cache.

    - Simple hash is used.
    - All subpages of a page are in the same hash chain.
    - The newest subpage is at the front.


    Hmm... maybe a tree would be better...
*/

//static struct cache_ops cops;

struct cache_page
{
    struct dl_node node[1];
    struct vt_page page[1];

    /* dynamic size, no fields below */
};


static inline int
hash(int pgno)
{
    // very simple...
    return pgno % HASH_SIZE;
}

static void
cache_close(struct cache *ca)
{
    struct cache_page *cp;
    int i;

    for (i = 0; i < HASH_SIZE; ++i)
	while (not dl_empty(ca->hash + i))
	{
	    cp = $ ca->hash[i].first;
	    dl_remove(cp->node);
	    free(cp);
	}
    free(ca);
}

static void
cache_reset(struct cache *ca)
{
    struct cache_page *cp, *cpn;
    int i;

    for (i = 0; i < HASH_SIZE; ++i)
	for (cp = $ ca->hash[i].first; (cpn = $ cp->node->next); cp = cpn)
	    if (cp->page->pgno / 256 != 9) // don't remove help pages
	    {
		dl_remove(cp->node);
		free(cp);
		ca->npages--;
	    }
    memset(ca->hi_subno, 0, sizeof(ca->hi_subno[0]) * 0x900);
}



/*
    Get a page from the cache.
    If subno is SUB_ANY, the newest subpage of that page is returned
*/

static struct vt_page *
cache_get(struct cache *ca, int pgno, int subno, int subno_mask)
{
    struct cache_page *cp;
    int h = hash(pgno);

    for (cp = $ ca->hash[h].first; cp->node->next; cp = $ cp->node->next) {
	if (cp->page->pgno == pgno)
	    if (subno == ANY_SUB || (cp->page->subno & subno_mask) == subno)
	    {
		// found, move to front (make it 'new')
		dl_insert_first(ca->hash + h, dl_remove(cp->node));
		return cp->page;
	    }
	}
    return 0;
}


/*
    Put a page in the cache.
    If it's already there, it is updated.
*/

static struct vt_page *
cache_put(struct cache *ca, struct vt_page *vtp)
{
    struct cache_page *cp;
    int h = hash(vtp->pgno);
    int size = vtp_size(vtp);


    for (cp = $ ca->hash[h].first;
     cp->node->next; cp = $ cp->node->next)
	if (cp->page->pgno == vtp->pgno && cp->page->subno == vtp->subno)
	    break;

	if (cp->node->next) {
		if (vtp_size(cp->page) == size) {
			// move to front.
			dl_insert_first(ca->hash + h, dl_remove(cp->node));
		} else {
			struct cache_page *new_cp;

			if (!(new_cp = malloc(sizeof(*cp) - sizeof(cp->page) + size)))
				return 0;
			dl_remove(cp->node);
			free(cp);
			cp = new_cp;
			dl_insert_first(ca->hash + h, cp->node);
		}
	} else {
		if (!(cp = malloc(sizeof(*cp) - sizeof(cp->page) + size)))
			return 0;
		if (vtp->subno >= ca->hi_subno[vtp->pgno])
			ca->hi_subno[vtp->pgno] = vtp->subno + 1;
		ca->npages++;
		dl_insert_first(ca->hash + h, cp->node);
	}

	memcpy(cp->page, vtp, size);

    return cp->page;
}



/////////////////////////////////
// this is for browsing the cache
/////////////////////////////////

/*
    Same as cache_get but doesn't make the found entry new
*/

static struct vt_page *
cache_lookup(struct cache *ca, int pgno, int subno)
{
    struct cache_page *cp;
    int h = hash(pgno);

    for (cp = $ ca->hash[h].first; cp->node->next; cp = $ cp->node->next)
	if (cp->page->pgno == pgno)
	    if (subno == ANY_SUB || cp->page->subno == subno)
		return cp->page;
    return 0;
}



static struct vt_page *
cache_foreach_pg(struct cache *ca, int pgno, int subno, int dir,
						    int (*func)(), void *data)
{
    struct vt_page *vtp, *s_vtp = 0;

    if (ca->npages == 0)
	return 0;

    if ((vtp = cache_lookup(ca, pgno, subno)))
	subno = vtp->subno;
    else if (subno == ANY_SUB)
	subno = dir < 0 ? 0 : 0xffff;

    for (;;)
    {
	subno += dir;
	while (subno < 0 || subno >= ca->hi_subno[pgno])
	{
	    pgno += dir;
	    if (pgno < 0x100)
		pgno = 0x9ff;
	    if (pgno > 0x9ff)
		pgno = 0x100;
	    subno = dir < 0 ? ca->hi_subno[pgno] - 1 : 0;
	}
	if ((vtp = cache_lookup(ca, pgno, subno)))
	{
	    if (s_vtp == vtp)
		return 0;
	    if (s_vtp == 0)
		s_vtp = vtp;
	    if (func(data, vtp))
		return vtp;
	}
    }
}

static int
cache_foreach_pg2(struct cache *ca, int pgno, int subno,
		  int dir, int (*func)(), void *data)
{
    struct vt_page *vtp;
    int wrapped = 0;
    int r;

    if (ca->npages == 0)
	return 0;

    if ((vtp = cache_lookup(ca, pgno, subno)))
	subno = vtp->subno;
    else if (subno == ANY_SUB)
	subno = 0;

    for (;;)
    {
	if ((vtp = cache_lookup(ca, pgno, subno)))
	{
	    if ((r = func(data, vtp, wrapped)))
		return r;
	}

	subno += dir;

	while (subno < 0 || subno >= ca->hi_subno[pgno])
	{
	    pgno += dir;
	    if (pgno < 0x100) {
		pgno = 0x9ff;
		wrapped = 1;
	    }
	    if (pgno > 0x9ff) {
		pgno = 0x100;
		wrapped = 1;
	    }
	    subno = dir < 0 ? ca->hi_subno[pgno] - 1 : 0;
	}
    }
}



static int
cache_mode(struct cache *ca, int mode, int arg)
{
    int res = -1;

    switch (mode)
    {
	case CACHE_MODE_ERC:
	    res = ca->erc;
	    ca->erc = arg ? 1 : 0;
	    break;
    }
    return res;
}


static struct cache_ops cops =
{
    cache_close,
    cache_get,
    cache_put,
    cache_reset,
    cache_foreach_pg,
    cache_foreach_pg2,
    cache_mode,
};


struct cache *
cache_open(void)
{
    struct cache *ca;
//    struct vt_page *vtp;
    int i;

    if (not(ca = malloc(sizeof(*ca))))
	goto fail1;

    for (i = 0; i < HASH_SIZE; ++i)
	dl_init(ca->hash + i);

    memset(ca->hi_subno, 0, sizeof(ca->hi_subno));
    ca->erc = 1;
    ca->npages = 0;
    ca->op = &cops;

    return ca;

//fail2:
    free(ca);
fail1:
    return 0;
}

