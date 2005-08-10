/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2005
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
#ifndef IVY_ARGUMENT_H
#define IVY_ARGUMENT_H
/* Module de gestion de la syntaxe des messages Ivy */

typedef struct _argument *IvyArgument;

IvyArgument IvyArgumentNew( int len, const void * value );
void IvyArgumentFree( IvyArgument arg );
int IvyArgumentGetChildCount( IvyArgument arg );
void IvyArgumentGetValue( IvyArgument arg, int * len, const void **val );
IvyArgument IvyArgumentGetChildrens( IvyArgument arg );
IvyArgument IvyArgumentGetNextChild( IvyArgument arg );
void IvyAddChild( IvyArgument arg, IvyArgument child );
IvyArgument IvyAddChildValue( IvyArgument arg, int childvaluelen, const void* childvalue );

IvyArgument IvyArgumentDeserialize( int buf_len, void* buffer, int * buf_adv);
int IvyArgumentSerialize(IvyArgument arg, int *buf_len, void **buffer, int offset);

#endif