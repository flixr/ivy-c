/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 *	Argument message comtent 
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
/* Module de gestion de la syntaxe des messages Ivy */

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h> 
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#endif
#include "list.h"
#include "ivyargument.h"



struct _argument {
	/* childrens */
	struct _argument *next;
	struct _argument *childrens;
	/* arg value */
	int value_len;
	const void *value;
	};

IvyArgument IvyArgumentNew( int len, const void * value )
{
	IvyArgument arg = malloc( sizeof( *arg  ) );
	arg->value_len = len;
	arg->value = value;
	arg->next = 0;
	arg->childrens = 0;
	return arg;
}
void IvyArgumentFree( IvyArgument arg )
{
	free( arg );
}
void IvyArgumentGetValue( IvyArgument arg, int * len, const void **val )
{
	*len = arg->value_len;
	*val = arg->value;
}
IvyArgument IvyArgumentGetChildrens( IvyArgument arg )
{
	return arg->childrens;
}
IvyArgument IvyArgumentGetNextChild( IvyArgument arg )
{
	return arg->next;
}
IvyArgument IvyAddChildValue( IvyArgument arg, int childvaluelen, const void* childvalue )
{
	/* ADD Child to the beginning of the list */
	IvyArgument child;
	IVY_LIST_ADD( arg->childrens, child )
	if ( child )
		{
			child->value_len = childvaluelen;
			child->value = childvalue;
		}
	return child;
}
void IvyAddChild( IvyArgument arg, IvyArgument child )
{
	/* ADD Child to the beginning of the list */
	child->next = arg->childrens;
	arg->childrens = child;
}
int IvyArgumentGetChildCount( IvyArgument arg )
{
	IvyArgument p;
	int count = 0;
	IVY_LIST_EACH( arg->childrens, p )
	{
		count++;
	}
	return count;
}
IvyArgument IvyArgumentDeserialize( int buf_len, void* buffer, int * buf_adv )
{
	int i;
	void *ptr_end = buffer;
	int adv;
	unsigned short value_len;
	unsigned short nbchild;
	IvyArgument arg;
	IvyArgument child;

	adv = 0;
	arg = IvyArgumentNew( 0, 0);
	/* reading value */
	value_len = ntohs( *((unsigned short *) ptr_end)++ );
	if ( value_len )
	{
	arg->value_len = value_len;
	arg->value = ptr_end;
	((unsigned char *)ptr_end) += value_len;
	}
	/* reading child */
	nbchild = ntohs( *((unsigned short *) ptr_end)++ );

	if ( nbchild )
	{
	for ( i= 0; i < nbchild; i++ )
	{
		child = IvyArgumentDeserialize( buf_len - ((unsigned char *)ptr_end - (unsigned char *)buffer)   , ptr_end, &adv );
		IvyAddChild( arg, child );
		((unsigned char *)ptr_end) += adv;
	}
	}
	*buf_adv += (unsigned char *)ptr_end - (unsigned char *)buffer; 
#ifdef DEBUG
	printf( "IvyArgumentDeserialize value='%.*s' nbchild=%d size=%d\n",arg->value_len, (char*)arg->value,  nbchild, *buf_adv);
#endif
	return arg;
}
int IvyArgumentSerialize(IvyArgument arg, int *buf_len, void **buffer, int offset)
{
	int nb_child;
	IvyArgument child;
	void *ptr;
	
	/* check buffer space */
	if ( (*buf_len - offset) < (2 * sizeof( unsigned short ) + arg->value_len) )
	{
		*buf_len += 4096;
		*buffer = realloc( *buffer, *buf_len );
	}
	ptr =  (unsigned char*)(*buffer) + offset; 
	/* writing value */
	*((unsigned short*)ptr)++  = htons( (unsigned short)arg->value_len );
	if ( arg->value_len )
	{
	memcpy( ptr, arg->value, arg->value_len );
	(unsigned char*)ptr += arg->value_len;
	}
	/* writing child */
	nb_child = IvyArgumentGetChildCount( arg );
	*((unsigned short*)ptr)++  = htons( (unsigned short)nb_child );
	IVY_LIST_EACH ( arg->childrens, child )
	{
		(unsigned char*)ptr += IvyArgumentSerialize( child, buf_len, buffer, (unsigned char*)ptr - (unsigned char*)*buffer );
	}
#ifdef DEBUG
	printf( "IvyArgumentSerialize value='%.*s' nbchild=%d buff size=%d\n",arg->value_len, (char*)arg->value,  nb_child, (unsigned char*)ptr - (unsigned char*)*buffer - offset);
#endif
	return (unsigned char*)ptr - (unsigned char*)*buffer - offset;
}

