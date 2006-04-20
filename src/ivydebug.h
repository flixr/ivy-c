#ifndef IVY_DEBUG_H
#define IVY_DEBUG_H

#ifdef WIN32
#include <crtdbg.h>
#else
#ifdef DEBUG
#define TRACE(format, args...)  \
 fprintf (stderr, format , ## args)

#define TRACE_IF( cond, format, args...)  \
	if ( cond ) fprintf (stderr, format , ## args)

#else
#define TRACE(format, args...) /**/
#define TRACE_IF( cond, format, args...)  /**/
#endif
#endif

#endif
