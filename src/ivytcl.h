/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-1999
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop based on X Toolkit
 *
 *	Authors: François-Régis Colin <colin@cenatoulouse.dgac.fr>
 *		 Stéphane Chatty <chatty@cenatoulouse.dgac.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifndef IVYTCLLOOP_H
#define IVYTCLLOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <X11/Intrinsic.h>

/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#define HANDLE SOCKET
#else
#define HANDLE int
#endif

#include "ivychannel.h"

extern void IvyTclChannelInit(void);

extern Channel IvyTclChannelSetUp(
		HANDLE fd,
		void *data,
		ChannelHandleDelete handle_delete,
		ChannelHandleRead handle_read
);

extern void IvyTclChannelClose( Channel channel );

#ifdef __cplusplus
}
#endif

#endif

