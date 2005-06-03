/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 *	Bind syntax for extracting message comtent 
 *  using regexp or other 
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

/* Module de gestion de la syntaxe des messages Ivy */

typedef struct _binding *IvyBinding;

IvyBinding IvyBindingCompile( const char * expression );
void IvyBindingGetCompileError( int *erroffset, const char **errmessage );
void IvyBindingFree( IvyBinding bind );
int IvyBindExec( IvyBinding bind, const char * message );
void IvyBindingGetMatch( IvyBinding bind, const char *message, int index, const char **arg, int *arglen );