#ifndef _BUSLOOP_H
#define _BUSLOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "buschannel.h"

/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#define HANDLE SOCKET
#else
#define HANDLE int
#endif

extern void BusLoopChannelInit(void);
extern void BusLoopChannelStop(void);
extern void BusLoopChannelMainLoop(void(*hook)(void) );

extern Channel BusLoopChannelSetUp(
						HANDLE fd,
						void *data,
						ChannelHandleDelete handle_delete,
						ChannelHandleRead handle_read
						);

extern void BusLoopChannelClose( Channel channel );


#ifdef __cplusplus
}
#endif

#endif
