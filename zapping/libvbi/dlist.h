/*
 *  libzvbi - Double linked wheel, reinvented
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: dlist.h,v 1.2 2004-12-07 17:30:40 mschimek Exp $ */

#ifndef DLIST_H
#define DLIST_H

#include <assert.h>
#include "macros.h"
#include "misc.h"

#ifndef DLIST_CONSISTENCY
#define DLIST_CONSISTENCY 0
#endif

typedef struct node node;

struct node {
	node *			succ;
	node *			pred;
};

typedef struct {
	node *			head;
	node *			null;
	node *			tail;
} list;

vbi3_inline void
list_verify			(const list *		l)
{
	unsigned int ct;
	const node *n;

	if (!DLIST_CONSISTENCY)
		return;

	for (n = (const node *) &l->head, ct = 0; n->succ; n = n->succ) {
		assert (ct++ < 30000);
		assert (n == n->succ->pred);
	}

	assert (n == (const node *) &l->null);
	assert (NULL == l->null);
}

/**
 * @internal
 *
 * Traverses a list. p points to the parent structure of a node. p1 is
 * a pointer of same type as p, used to remember the next node in the
 * list. This permits unlink_node(p) in the loop. Resist the temptation
 * to unlink p->succ or p->pred. l points to the list to traverse.
 * _node is the name of the node element. Example:
 *
 * struct mystruct { node foo; int bar; };
 *
 * list mylist; // assumed initialized
 * struct mystruct *p, *p1;
 *
 * FOR_ALL_NODES (p, p1, &mylist, foo)
 *   do_something (p);
 */
#define FOR_ALL_NODES(p, p1, l, _node)					\
for (list_verify (l), p = PARENT ((l)->head, __typeof__ (* p), _node);	\
     (p1 = PARENT ((p)->_node.succ, typeof (* p1), _node)); p = p1)

#define FOR_ALL_NODES_REVERSE(p, p1, l, _node)				\
for (list_verify (l), p = PARENT ((l)->tail, __typeof__ (* p), _node);	\
     (p1 = PARENT ((p)->_node.pred, typeof (* p1), _node)); p = p1)

/**
 * @internal
 * Destroys list l (not its nodes).
 */
vbi3_inline void
list_destroy			(list *			l)
{
	list_verify (l);

	CLEAR (*l);
}

/**
 * @internal
 * Initializes list l.
 */
vbi3_inline list *
list_init			(list *			l)
{
	l->head = (node *) &l->null;
	l->null = (node *) 0;
	l->tail = (node *) &l->head;

	list_verify (l);

	return l;
}

/**
 * @internal
 * TRUE if list l is empty.
 */
vbi3_inline int
empty_list			(const list *		l)
{
	list_verify (l);

	return l->head == (const node *) &l->null;
}

/**
 * @internal
 * Adds node n at begin of list l.
 */
vbi3_inline node *
add_head			(list *			l,
				 node *			n)
{
	list_verify (l);

	n->pred = (node *) &l->head;
	n->succ = l->head;
	l->head->pred = n;
	l->head = n;

	return n;
}

/**
 * @internal
 * Adds node n at end of list l.
 */
vbi3_inline node *
add_tail			(list *			l,
				 node *			n)
{
	list_verify (l);

	n->succ = (node *) &l->null;
	n->pred = l->tail;
	l->tail->succ = n;
	l->tail = n;

	return n;
}

/**
 * @internal
 * Removes all nodes from list l2 and adds them at end of list l1.
 */
vbi3_inline node *
add_tail_list			(list *			l1,
				 list *			l2)
{
	node *n = l2->head;

	list_verify (l1);
	list_verify (l2);

	n->succ = (node *) &l1->null;
	n->pred = l1->tail;
	l1->tail->succ = n;
	l1->tail = l2->tail;

	l2->head = (node *) &l2->null;
	l2->tail = (node *) &l2->head;

	return n;
}

/**
 * @internal
 * TRUE if node n is at head of list l.
 */
vbi3_inline vbi3_bool
is_head				(const list *		l,
				 const node *		n)
{
	list_verify (l);

	return l->head == n;
}

/**
 * @internal
 * TRUE if node n is a member of list l.
 */
vbi3_inline vbi3_bool
is_member			(const list *		l,
				 const node *		n)
{
	const node *q;

	list_verify (l);

	for (q = l->head; q->succ; q = q->succ)
		if (__builtin_expect (n == q, 0))
			return TRUE;

	return FALSE;
}

/**
 * @internal
 * Removes first node of list l, returns NULL if empty list.
 */
vbi3_inline node *
rem_head			(list *			l)
{
	node *n = l->head, *s = n->succ;

	list_verify (l);

	if (__builtin_expect (s != NULL, 1)) {
		s->pred = (node *) &l->head;
		l->head = s;
	} else {
		n = NULL;
	}

	return n;
}

/**
 * @internal
 * Removes last node of list l, returns NULL if empty list.
 */
vbi3_inline node *
rem_tail			(list *			l)
{
	node *n = l->tail, *p = n->pred;

	list_verify (l);

	if (__builtin_expect (p != NULL, 1)) {
		p->succ = (node *) &l->null;
		l->tail = p;
	} else {
		n = NULL;
	}

	return n;
}

/**
 * @internal
 * Removes node n from its list. The node must
 * be a member of the list, this is not verified.
 */
vbi3_inline node *
unlink_node			(node *			n)
{
	if (DLIST_CONSISTENCY) {
		node *n1 = n;

		while (n1->pred)
			n1 = n1->pred;
		list_verify ((list *) n1);
	}

	n->pred->succ = n->succ;
	n->succ->pred = n->pred;

	return n;
}

/**
 * @internal
 * Removes node n if member of list l.
 */
vbi3_inline node *
rem_node			(list *			l,
				 node *			n)
{
	if (is_member (l, n)) {
		unlink_node (n);
		return n;
	}

	return NULL;
}

/**
 * @internal
 * Returns number of nodes in list l.
 */
vbi3_inline unsigned int
list_length			(list *			l)
{
	unsigned int count = (unsigned int) -1;
	node *n;

	list_verify (l);

	for (n = l->head; n; n = n->succ)
		++count;

	return count;
}

#endif /* DLIST_H */