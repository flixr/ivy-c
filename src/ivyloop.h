/*
 *	Ivy, C interface
 *
 *      Copyright (C) 1997-1999
 *      Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop handling around select
 *
 *      Authors: François-Régis Colin <colin@cenatoulouse.dgac.fr>
 *		 Stéphane Chatty <chatty@cenatoulouse.dgac.fr>
 *
 *	$Id$
 * 
 *      Please refer to file version.h for the
 *      copyright notice regarding this software
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



extern void IvyMainLoop(void(*hook)(void) );


extern void IvyChannelInit(void);
extern void IvyChannelClose( Channel channel );
extern Channel IvyChannelSetUp(
			HANDLE fd,
			void *data,
			ChannelHandleDelete handle_delete,
			ChannelHandleRead handle_read);



#ifdef __cplusplus
}
#endif

#endif

