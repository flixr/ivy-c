/*
 *
 * Ivy, C interface
 *
 * Copyright 1997-1998 
 * Centre d'Etudes de la Navigation Aerienne
 *
 * Main loop based on X Toolkit
 *
 * $Id$
 *
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


#include <X11/Intrinsic.h>

#include "ivychannel.h"
#include "ivyxtloop.h"

struct _channel {
	XtInputId id_read;
	XtInputId id_delete;
	void *data;
	ChannelHandleDelete handle_delete;
	ChannelHandleRead handle_read;
	};


static int channel_initialized = 0;


static XtAppContext    app = NULL;


void BusXtChannelClose( Channel channel )
{

	if ( channel->handle_delete )
		(*channel->handle_delete)( channel->data );
	XtRemoveInput( channel->id_read );
	XtRemoveInput( channel->id_delete );
}

static void BusXtHandleChannelRead( XtPointer closure, int* source, XtInputId* id )
{
	Channel channel = (Channel)closure;
#ifdef DEBUG
	printf("Handle Channel read %d\n",*source );
#endif
	(*channel->handle_read)(channel,*source,channel->data);
}
static void BusXtHandleChannelDelete( XtPointer closure, int* source, XtInputId* id )
{
	Channel channel = (Channel)closure;
#ifdef DEBUG
	printf("Handle Channel delete %d\n",*source );
#endif
	(*channel->handle_delete)(channel->data);
}
Channel BusXtChannelSetUp(HANDLE fd, void *data,
				ChannelHandleDelete handle_delete,
				ChannelHandleRead handle_read
				)						
{
	Channel channel;

	channel = XtNew( struct _channel );
	if ( !channel )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		exit(0);
		}

	channel->handle_delete = handle_delete;
	channel->handle_read = handle_read;
	channel->data = data;

	channel->id_read = XtAppAddInput( app, fd, (XtPointer)XtInputReadMask, BusXtHandleChannelRead, channel);
	channel->id_delete = XtAppAddInput( app, fd, (XtPointer)XtInputExceptMask, BusXtHandleChannelDelete, channel);

	return channel;
}


void BusXtChannelAppContext( XtAppContext cntx )
{
	app = cntx;
}

void BusXtChannelInit(void)
{

	if ( channel_initialized ) return;

	/* pour eviter les plantages quand les autres applis font core-dump */
#ifndef WIN32
	signal( SIGPIPE, SIG_IGN);
#endif
	/* verifie si init correct */
	if ( !app )
	{
		fprintf( stderr, "You Must call BusXtChannelAppContext to Use XtMainLoop !!!\n");
		exit(-1);
	}
	channel_initialized = 1;
}


