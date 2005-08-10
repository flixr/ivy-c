/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Basic I/O handling
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 *
 */
#ifndef _IVYCHANNEL_H
#define _IVYCHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif
	
/* general Handle */

#ifdef WIN32
#include <windows.h>
#define IVY_HANDLE SOCKET
#else
#define IVY_HANDLE int
#endif

typedef struct _channel *Channel;
/* callback declenche par la gestion de boucle  sur evenement exception sur le canal */
typedef void (*ChannelHandleDelete)( void *data );
/* callback declenche par la gestion de boucle sur donnees pretes sur le canal */
typedef void (*ChannelHandleRead)( Channel channel, IVY_HANDLE fd, void *data);

/* fonction appele par le bus pour initialisation */
extern void IvyChannelInit(void);
/* fonction appele par le bus pour terminaison */
extern void IvyChannelStop(void);
						
/* fonction appele par le bus pour mise en place des callback sur le canal */
extern Channel IvyChannelAdd(
	IVY_HANDLE fd,
	void *data,
	ChannelHandleDelete handle_delete,
	ChannelHandleRead handle_read
);

/* fonction appele par le bus pour fermeture du canal */
extern void IvyChannelRemove( Channel channel );

#ifdef __cplusplus
}
#endif

#endif

