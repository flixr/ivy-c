/*
 *
 * Ivy, C interface
 *
 * Copyright 1997-1998 
 * Centre d'Etudes de la Navigation Aerienne
 *
 * Main loop handling around select
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


#include "list.h"
#include "ivychannel.h"
#include "ivyloop.h"
#include "timer.h"

struct _channel {
	Channel next;
	HANDLE fd;
	void *data;
	int tobedeleted;
	ChannelHandleDelete handle_delete;
	ChannelHandleRead handle_read;
	};



static Channel channels_list = NULL;

static int channel_initialized = 0;

static fd_set open_fds;
static int MainLoop = 1;

#ifdef WIN32
WSADATA					WsaData;
#endif

void BusLoopChannelClose( Channel channel )
{
	channel->tobedeleted = 1;
}

static void BusLoopChannelDelete( Channel channel )
{
	if ( channel->handle_delete )
		(*channel->handle_delete)( channel->data );

	FD_CLR(channel->fd, &open_fds);
	LIST_REMOVE( channels_list, channel );
}
static void ChannelDefferedDelete()
{
	Channel channel,next;
	LIST_EACH_SAFE( channels_list, channel,next)
		{
		if ( channel->tobedeleted  )
			{
			BusLoopChannelDelete( channel );
			}
		}
}

Channel BusLoopChannelSetUp(HANDLE fd, void *data, 
				ChannelHandleDelete handle_delete,
				ChannelHandleRead handle_read
				)						
{
	Channel channel;

	LIST_ADD( channels_list, channel );
	if ( !channel )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		exit(0);
		}
	channel->fd = fd;
	channel->tobedeleted = 0;
	channel->handle_delete = handle_delete;
	channel->handle_read = handle_read;
	channel->data = data;

	FD_SET( channel->fd, &open_fds );

	return channel;
}

static void BusLoopChannelHandleRead(fd_set *current)
{
	Channel channel,next;
	
	LIST_EACH_SAFE( channels_list, channel, next )
		{
		if ( FD_ISSET( channel->fd, current ) )
			{
			(*channel->handle_read)(channel,channel->fd,channel->data);
			}
		}
}

static void BusLoopChannelHandleExcpt(fd_set *current)
{
	Channel channel,next;
	LIST_EACH_SAFE( channels_list, channel, next )
		{
		if (FD_ISSET( channel->fd, current ) )
			{
			(*channel->handle_delete)(channel->data);
//			BusLoopChannelClose( channel );
			}
		}
}

void BusLoopChannelInit(void)
{
#ifdef WIN32
	int error;
#else 
	/* pour eviter les plantages quand les autres applis font core-dump */
	signal( SIGPIPE, SIG_IGN);
#endif
	if ( channel_initialized ) return;

	FD_ZERO( &open_fds );

#ifdef WIN32
	error = WSAStartup( 0x0101, &WsaData );
        if ( error == SOCKET_ERROR ) {
            printf( "WSAStartup failed.\n" );
        }
#endif
	channel_initialized = 1;
}


void BusLoopChannelStop(void)
{
	MainLoop = 0;
}

void BusLoopChannelMainLoop(void(*hook)(void))
{

fd_set rdset;
fd_set exset;
int ready;



   while (MainLoop) {
	ChannelDefferedDelete();
   	if ( hook ) (*hook)();
    rdset = open_fds;
    exset = open_fds;
	ready = select(64, &rdset, 0,  &exset, TimerGetSmallestTimeout());
	if ( ready < 0 && ( errno != EINTR ))
		{
		perror("select");
		return;
		}
	TimerScan();
    if ( ready > 0 )
		{
		BusLoopChannelHandleExcpt(&exset);
		BusLoopChannelHandleRead(&rdset);
		continue;
		}
	}
}

