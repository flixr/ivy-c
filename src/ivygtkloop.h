/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-1999
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop based on GTK Toolkit
 *
 *	Authors: François-Régis Colin <colin@tls.cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifndef IVYGTKLOOP_H
#define IVYGTKLOOP_H

#ifdef __cplusplus
extern "C" {
#endif


/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#define HANDLE SOCKET
#else
#define HANDLE int
#endif

#include "ivychannel.h"

extern void IvyGtkChannelInit(void);

extern Channel IvyGtkChannelSetUp(
		HANDLE fd,
		void *data,
		ChannelHandleDelete handle_delete,
		ChannelHandleRead handle_read
);

extern void IvyGtkChannelClose( Channel channel );


#ifdef __cplusplus
}
#endif

#endif

