/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Simple lists in C
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.dgac.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
#if (__GNUC__ >= 3)
#define TYPEOF(p) typeof (p)
#else
#define  TYPEOF(p) void *
#endif

#define IVY_LIST_ITER( list, p, cond ) \
	p = list; \
	while ( p && (cond) ) p = p->next 



#define IVY_LIST_REMOVE( list, p ) \
	{ \
	TYPEOF(p)  toRemove; \
	if ( list == p ) \
		{ \
		list = p->next; \
		free(p);\
		} \
		else \
		{\
		toRemove = p;\
		IVY_LIST_ITER( list, p, ( p->next != toRemove ));\
		if ( p )\
			{\
			/* somme tricky swapping to use a untyped variable */\
			TYPEOF(p) suiv; \
			TYPEOF(p) prec = p;\
			p = toRemove;\
			suiv = p->next;\
			p = prec;\
			p->next = suiv;\
			free(toRemove);\
			}\
		} \
	}

#define IVY_LIST_ADD(list, p ) \
	if ( p = (TYPEOF(p)) (malloc( sizeof( *p ))))\
		{ \
		memset( p, 0 , sizeof( *p ));\
		p->next = list; \
		list = p; \
		} 

#define IVY_LIST_EACH( list, p ) \
	for ( p = list ; p ; p = p -> next )

#define IVY_LIST_EACH_SAFE( list, p, next )\
for ( p = list ; (next = p ? p->next: p ),p ; p = next )

#define IVY_LIST_EMPTY( list ) \
	{ \
	TYPEOF(list) p; \
	while( list ) \
		{ \
		p = list;\
		list = list->next; \
		free(p);\
		} \
	}
