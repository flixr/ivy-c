/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-1999
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop based on Gtk Toolkit
 *
 *	Authors: François-Régis Colin <colin@tls.cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifdef WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef WIN32
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif


#include <gdk/gdk.h>

#include "ivychannel.h"
#include "ivygtkloop.h"

struct _channel {
	guint id_read;
	guint id_delete;
	gpointer data;
	ChannelHandleDelete handle_delete;
	ChannelHandleRead handle_read;
	};


static int channel_initialized = 0;


ChannelInit channel_init = IvyGtkChannelInit;
ChannelSetUp channel_setup = IvyGtkChannelSetUp;
ChannelClose channel_close = IvyGtkChannelClose;


void IvyGtkChannelInit(void)
{

	if ( channel_initialized ) return;

	/* pour eviter les plantages quand les autres applis font core-dump */
#ifndef WIN32
	signal( SIGPIPE, SIG_IGN);
#endif
	channel_initialized = 1;
}

void IvyGtkChannelClose( Channel channel )
{

	if ( channel->handle_delete )
		(*channel->handle_delete)( channel->data );
	gtk_input_remove( channel->id_read );
	gtk_input_remove( channel->id_delete );
}
 

static void IvyGtkHandleChannelRead(gpointer data, gint source, GdkInputCondition condition)
{
	Channel channel = (Channel)data;
#ifdef DEBUG
	printf("Handle Channel read %d\n",source );
#endif
	(*channel->handle_read)(channel,source,channel->data);
}

static void IvyGtkHandleChannelDelete(gpointer data, gint source, GdkInputCondition condition)
{
	Channel channel = (Channel)data;
#ifdef DEBUG
	printf("Handle Channel delete %d\n",source );
#endif
	(*channel->handle_delete)(channel->data);
}



Channel IvyGtkChannelSetUp(HANDLE fd, void *data,
				ChannelHandleDelete handle_delete,
				ChannelHandleRead handle_read
				)						
{
	Channel channel;

	channel = (Channel)malloc( sizeof(struct _channel) );
	if ( !channel )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		exit(0);
		}

	channel->handle_delete = handle_delete;
	channel->handle_read = handle_read;
	channel->data = data;

	channel->id_read = gdk_input_add( fd, GDK_INPUT_READ, IvyGtkHandleChannelRead, channel);
	channel->id_delete = gdk_input_add( fd, GDK_INPUT_EXCEPTION, IvyGtkHandleChannelDelete, channel);

	return channel;
}


void
IvyStop ()
{
  /* To be implemented */
}

