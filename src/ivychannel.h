/*
 *
 * Ivy, C interface
 *
 * Copyright 1997-1998 
 * Centre d'Etudes de la Navigation Aerienne
 *
 * Basic I/O handling
 *
 * $Id$
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
#define HANDLE SOCKET
#else
#define HANDLE int
#endif

typedef struct _channel *Channel;
/* callback declenche par la gestion de boucle  sur evenement exception sur le canal */
typedef void (*ChannelHandleDelete)( void *data );
/* callback declenche par la gestion de boucle sur donnees pretes sur le canal */
typedef void (*ChannelHandleRead)( Channel channel, HANDLE fd, void *data);

/* fonction appele par le bus pour initialisation */
typedef void (*ChannelInit)(void);
						
/* fonction appele par le bus pour mise en place des callback sur le canal */
typedef Channel (*ChannelSetUp)(
						HANDLE fd,
						void *data,
						ChannelHandleDelete handle_delete,
						ChannelHandleRead handle_read
						);
/* fonction appele par le bus pour fermeture du canal */
typedef void (*ChannelClose)( Channel channel );

/* mise en place des fonction de gestion des canaux */
void BusSetChannelManagement( ChannelInit init_chan, ChannelSetUp setup_chan, ChannelClose close_chan );

#ifdef __cplusplus
}
#endif

#endif

