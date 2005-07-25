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


#include "list.h"
#include "ivyargument.h"



struct _argument {
	/* childrens */
	struct _argument *next;
	/* arg value */
	char *value;
	};

IvyArgument IvyArgumentNew( const char * value )
{
	IvyArgument arg = malloc( sizeof( struct _argument  ) );
	arg->value = strdup( value );
	arg->next = 0;
	return arg;
}
void IvyArgumentFree( IvyArgument arg )
{
	free( arg->value );
	free( arg );
}
const char * IvyArgumentGetValue( IvyArgument arg )
{
	return arg->value;
}

IvyArgument IvyArgumentGetNextChild( IvyArgument arg )
{
	return arg->next;
}
IvyArgument IvyAddChild( IvyArgument arg, const char* childvalue )
{
	IvyArgument child;
	IVY_LIST_ADD( arg->next, child )
	if ( child )
		{
			child->value = strdup( childvalue );
			child->next = 0;
		}
	return child;
}
IvyArgument IvyArgumentDeserialize( int fd )
{
	return 0;
}
void IvyArgumentSerialize(IvyArgument arg, int fd )
{

}

