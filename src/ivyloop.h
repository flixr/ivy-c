/*
 *
 * Ivy, C interface
 *
 * Copyright 1997-1999
 * Centre d'Etudes de la Navigation Aerienne
 *
 * Main loop based on select
 *
 * $Id$
 *
 */

#ifndef _IVYLOOP_H
#define _IVYLOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ivychannel.h"

/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#define HANDLE SOCKET
#else
#define HANDLE int
#endif

extern void IvyChannelInit(void);
extern void IvyChannelStop(void);
extern void IvyMainLoop(void(*hook)(void) );

extern Channel IvyChannelSetUp(
			HANDLE fd,
			void *data,
			ChannelHandleDelete handle_delete,
			ChannelHandleRead handle_read
);

extern void IvyChannelClose( Channel channel );


#ifdef __cplusplus
}
#endif

#endif

