#ifndef _BUSXTLOOP_H
#define _BUSXTLOOP_H

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

#include "buschannel.h"

extern void BusXtChannelInit(void);

extern Channel BusXtChannelSetUp(
						HANDLE fd,
						void *data,
						ChannelHandleDelete handle_delete,
						ChannelHandleRead handle_read
						);

extern void BusXtChannelClose( Channel channel );

extern void BusXtChannelAppContext( XtAppContext cntx );

#ifdef __cplusplus
}
#endif

#endif
