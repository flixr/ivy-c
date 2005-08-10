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

#ifdef USE_PCRE_REGEX
#define OVECSIZE 60 /* must be multiple of 3, for regexp return */
#include <pcre.h>
#else
#define MAX_MSG_FIELDS 200
#include <regex.h>
#endif

#include "list.h"
#include "ivybind.h"

static int err_offset;

#ifdef USE_PCRE_REGEX
	static const char *err_buf;
#else
	static char err_buf[4096];
#endif

struct _binding {
	struct _binding *next;
	IvyBindingType type;
#ifdef USE_PCRE_REGEX
	pcre *regexp;
	pcre_extra *inspect;
	int nb_match;
	int ovector[OVECSIZE];
#else
	regex_t regexp;						/* la regexp sous forme machine */
	regmatch_t match[MAX_MSG_FIELDS+1];	/* resultat du match */
#endif
	};


/* classes de messages emis par l'application utilise pour le filtrage */
static int	messages_classes_count = 0;
static const char **messages_classes = 0;

static IvyBinding IvyBindingCompileSimple( IvyBindingType typ, const char * expression )
{
	//TODO return NULL
	err_offset = 0;
#ifdef USE_PCRE_REGEX
	err_buf = "Not Yiet Implemented";
#else
	strcpy( err_buf, "Not Yiet Implemented" );
#endif
	return NULL;
}
static IvyBinding IvyBindingCompileRegexp( IvyBindingType typ, const char * expression )
{
	IvyBinding bind=0;
#ifdef USE_PCRE_REGEX
	pcre *regexp;
	regexp = pcre_compile(expression, PCRE_OPT,&err_buf,&err_offset,NULL);
	if ( regexp != NULL )
		{
			bind = (IvyBinding)malloc( sizeof( struct _binding ));
			bind->regexp = regexp;
			bind->next = NULL;
			bind->type = IvyBindRegexp;
			bind->inspect = pcre_study(regexp,0,&err_buf);
			if (err_buf!=NULL)
				{
					printf("Error studying %s, message: %s\n",expression,err_buf);
				}
		}
		else
		{
		printf("Error compiling '%s', %s\n", expression, err_buf);
		}
#else
	regex_t regexp;
	int reg;
	reg = regcomp(&regexp, expression, REGCOMP_OPT|REG_EXTENDED);
	if ( reg == 0 )
		{
			bind = (IvyBinding)malloc( sizeof( struct _binding ));
			bind->regexp = regexp;
			bind->next = NULL;
		}
		else
		{
		regerror (reg, &regexp, err_buf, sizeof(err_buf) );
		err_offset = 0; // TODO unkown offset error
		printf("Error compiling '%s', %s\n", expression, err_buf);
		}
#endif
	return bind;
}
IvyBinding IvyBindingCompile( IvyBindingType typ, const char * expression )
{
	if ( typ == IvyBindRegexp )
		return IvyBindingCompileRegexp( typ, expression);
	else
		return IvyBindingCompileSimple( typ, expression);
}
void IvyBindingGetCompileError( int *offset, const char **errmessage )
{
	*offset = err_offset;
	*errmessage = err_buf;
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
int IvyBindingExecRegexp( IvyBinding bind, const char * message )
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
	bind->nb_match = nb_match;
	nb_match--; // firts arg wall string ???

#else
	memset( bind->match, -1, sizeof(bind->match )); /* work around bug !!!*/
	nb_match = regexec (&bind->regexp, message, MAX_MSG_FIELDS, bind->match, 0) 
	if (nb_match == REG_NOMATCH)
		return 0;
	for ( index = 1; index < MAX_MSG_FIELDS; index++ )
	{
		if ( bind->match[i].rm_so != -1 )
			nb_match++;
	}
#endif
	return nb_match;
}
int IvyBindingExecSimple( IvyBinding bind, const char * message )
{
	return 0;
}
int IvyBindingExec( IvyBinding bind, const char * message )
{
	if ( bind->type == IvyBindRegexp )
		return IvyBindingExecRegexp( bind, message);
	else
		return IvyBindingExecSimple( bind, message);
}
static IvyArgument IvyBindingMatchSimple( IvyBinding bind, const char *message)
{
	//TODO
	return NULL;
}
static IvyArgument IvyBindingMatchRegexp( IvyBinding bind, const char *message)
{
	int index=1;// firts arg wall string ???
	int arglen;
	const void* arg;
	IvyArgument args;
	args = IvyArgumentNew( 0,NULL ); 
	
#ifdef USE_PCRE_REGEX
	while ( index<bind->nb_match ) {
		arglen = bind->ovector[2*index+1]- bind->ovector[2*index];
		arg =   message + bind->ovector[2*index];
		index++;
#else  /* we don't USE_PCRE_REGEX */
	for ( index = 1; index < MAX_MSG_FIELDS; index++ )
	{
	regmatch_t* p;

	p = &bind->match[index];
	if ( p->rm_so != -1 ) {
			arglen = p->rm_eo - p->rm_so;
			arg = message + p->rm_so;
	} else { // ARG VIDE
			arglen = 0;
			arg = NULL;
	}
#endif // USE_PCRE_REGEX
	IvyAddChildValue( args, arglen, arg );
	}
	return args;
}
IvyArgument IvyBindingMatch( IvyBinding bind, const char *message)
{
	if ( bind->type == IvyBindRegexp )
		return IvyBindingMatchRegexp( bind, message);
	else 
		return IvyBindingMatchSimple( bind, message);
}

//filter Expression Bind 
void IvyBindingSetFilter( int argc, const char **argv)
{
	messages_classes_count = argc;
	messages_classes = argv;
}
int IvyBindingFilter(IvyBindingType typ, int len, const char *exp)
{
	 /* TODO check args limits !!!*/
	int i;
	/* accepte tout par default */
	int regexp_ok = 1;
	// TODO simplify test 3 conditions
	if ( typ == IvyBindRegexp )
	{
	if ( *exp =='^' && messages_classes_count !=0 )
	{
		regexp_ok = 0;
		for ( i = 0 ; i < messages_classes_count; i++ )
		{
			if (strncmp( messages_classes[i], exp+1, strlen( messages_classes[i] )) == 0)
				return 1;
		}
 	}
	}
	else
	{
	if ( messages_classes_count !=0 )
	{
		regexp_ok = 0;
		for ( i = 0 ; i < messages_classes_count; i++ )
		{
			if (strncmp( messages_classes[i], exp, strlen( messages_classes[i] )) == 0)
				return 1;
		}
 	}
	}
	return regexp_ok;
}