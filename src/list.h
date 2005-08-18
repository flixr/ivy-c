/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Simple lists in C
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
#ifdef WIN32
#define TYPEOF(p) void*
#else
#if (__GNUC__ >= 2)
#define TYPEOF(p) typeof (p)
#else
#define  TYPEOF(p) void *
#endif
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
        if ((p = (TYPEOF(p)) (malloc( sizeof( *p ))))) \
		{ \
		memset( p, 0 , sizeof( *p ));\
		p->next = list; \
		list = p; \
		}

#define IVY_LIST_ADD_END(list, p ) \
        if ((p = (TYPEOF(p)) (malloc( sizeof( *p ))))) \
		{ \
		TYPEOF(p) list_end; \
		memset( p, 0 , sizeof( *p ));\
		p->next = 0; \
		if ( list ) \
		{\
		list_end = list; \
		while ( list_end && list_end->next ) \
			list_end = list_end->next;\
		list_end->next = p; \
		} \
		else list = p; \
		}

#define IVY_LIST_EACH( list, p ) \
	for ( p = list ; p ; p = p -> next )

#define IVY_LIST_EACH_SAFE( list, p, p_next )\
for ( p = list ; (p_next = p ? p->next: p ),p ; p = p_next )

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
