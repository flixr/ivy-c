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

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h> 

#ifndef USE_PCRE_REGEX
#include <regex.h>
#else
#define OVECSIZE 60 /* must be multiple of 3, for regexp return */
#include <pcre.h>
#endif

#include "list.h"
#include "ivybind.h"

#ifndef USE_PCRE_REGEX
	static int erroroffset;
	static char errbuf[4096];
#else
	static const char *errbuf;
	static int erroffset;
#endif

struct _binding {
	struct _binding *next;
#ifndef USE_PCRE_REGEX
	regex_t regexp;						/* la regexp sous forme machine */
	regmatch_t match[MAX_MSG_FIELDS+1];	/* resultat du match */
#else
	pcre *regexp;
	pcre_extra *inspect;
	int ovector[OVECSIZE];
#endif
	};

IvyBinding IvyBindingCompile( const char * expression )
{
	IvyBinding bind=0;
#ifdef USE_PCRE_REGEX
	pcre *regexp;
	regexp = pcre_compile(expression, PCRE_OPT,&errbuf,&erroffset,NULL);
	if ( regexp != NULL )
		{
			bind = (IvyBinding)malloc( sizeof( struct _binding ));
			bind->regexp = regexp;
			bind->inspect = pcre_study(regexp,0,&errbuf);
			if (errbuf!=NULL)
				{
					printf("Error studying %s, message: %s\n",expression,errbuf);
				}
		}
		else
		{
		printf("Error compiling '%s', %s\n", expression, errbuf);
		}
#else
	regex_t regexp;
	int reg;
	reg = regcomp(&regexp, expression, REGCOMP_OPT|REG_EXTENDED);
	if ( reg == 0 )
		{
			bind = (IvyBinding)malloc( sizeof( struct _binding ));
			bind->regexp = regexp;
		}
		else
		{
		regerror (reg, &regexp, errbuf, sizeof(errbuf) );
		erroroffset = 0; // TODO unkown offset error
		printf("Error compiling '%s', %s\n", expression, errbuf);
		}
#endif
	return bind;
}
void IvyBindingGetCompileError( int *offset, const char **errmessage )
{
#ifndef USE_PCRE_REGEX
	*offset = erroroffset;
	*errmessage = errbuf;
#else
	*offset = erroffset;
	*errmessage = errbuf;
#endif
}
void IvyBindingFree( IvyBinding bind )
{
#ifdef USE_PCRE_REGEX
				if (bind->inspect!=NULL) pcre_free(bind->inspect);
				pcre_free(bind->regexp);
#else
#endif
	free ( bind );
}
int IvyBindingExec( IvyBinding bind, const char * message )
{
	int nb_match = 0;
#ifdef USE_PCRE_REGEX
	
	nb_match = pcre_exec(
					bind->regexp,
					bind->inspect,
					message,
					strlen(message),
					0, /* debut */
					0, /* no other regexp option */
					bind->ovector,
					OVECSIZE);
	if (nb_match<1) return 0; /* no match */
	nb_match--; // firts arg wall string ???

#else
	memset( bind->match, -1, sizeof(bind->match )); /* work around bug !!!*/
	nb_match = regexec (&bind->regexp, message, MAX_MSG_FIELDS, bind->match, 0) 
	if (nb_match == REG_NOMATCH)
		return 0;
	/* TODO Possible BUG if empty match in middle of regexp */
	for ( index = 0; index < MAX_MSG_FIELDS; index++ )
	{
		if ( bind->match[i].rm_so != -1 )
			nb_match++;
	}
#endif
	return nb_match;
}
void IvyBindingGetMatch( IvyBinding bind, const char *message, int index, const char **arg, int *arglen )
{
	index++; // firts arg wall string ???
#ifdef USE_PCRE_REGEX
		*arglen = bind->ovector[2*index+1]- bind->ovector[2*index];
		*arg =   message + bind->ovector[2*index];
#else  /* we don't USE_PCRE_REGEX */

	regmatch_t* p;

	p = &bind->match[index];
	if ( p->rm_so != -1 ) {
			*arglen = p->rm_eo - p->rm_so;
			*arg = message + p->rm_so;
	} else { // ARG VIDE
			*arglen = 0;
			*arg = message;
	}

#endif
}