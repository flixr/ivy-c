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

typedef struct _argument *IvyArgument;

IvyArgument IvyArgumentNew( const char * expression );
void IvyArgumentFree( IvyArgument arg );
const char * IvyArgumentGetValue( IvyArgument arg );
IvyArgument IvyArgumentGetNextChild( IvyArgument arg );
IvyArgument IvyAddChild( IvyArgument arg, const char* childvalue );
IvyArgument IvyArgumentDeserialize( int fd );
void IvyArgumentSerialize(IvyArgument arg, int fd );

