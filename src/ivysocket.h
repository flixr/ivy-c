/*
 *	Ivy, C interface
 *
 *	Copyright 1997-2000
 *	Centre d'Etudes de la Navigation Aerienne
 *
 *	Sockets
 *
 *	Authors: Francois-Regis Colin <fcolin@cena.fr>
 *
 *	$Id$
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifndef IVYSOCKET_H
#define IVYSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#ifdef __MINGW32__
#include <ws2tcpip.h>
#endif
#define HANDLE SOCKET
#define socklen_t int
#ifndef IN_MULTICAST
#define IN_MULTICAST(i)            (((long)(i) & 0xf0000000) == 0xe0000000)
#endif
#else
#define HANDLE int
#include <netinet/in.h>
#endif
#ifdef __INTERIX
#define socklen_t int
#endif

/* General Init */

/* utility fonction do make vsprintf without buffer limit */
extern int make_message(char ** buffer, int *size,  int offset, const char *fmt, va_list ap);
extern int make_message_var(char ** buffer, int *size,  int offset, const char *fmt, ...);

/* Forward def */
typedef struct _client *Client;
typedef char* (*SocketInterpretation) (Client client, void *data, char *ligne, unsigned int len);
typedef void* (*SocketCreate) (Client client);
typedef void (*SocketDelete) (Client client, void *data);

/* Server Part */
typedef struct _server *Server;
extern Server SocketServer(unsigned short port, 
	SocketCreate create,
	SocketDelete handle_delete,
	SocketInterpretation interpretation);
extern unsigned short SocketServerGetPort( Server server );
extern void SocketServerClose( Server server );

/* Client Part */
extern void SocketKeepAlive( Client client,int keepalive );
extern void SocketClose( Client client );
extern void SocketSend( Client client, char *fmt, ... );
extern void SocketSendRaw( Client client, char *buffer, int len );
extern char *SocketGetPeerHost( Client client );
extern void SocketSetData( Client client, void *data );
extern void *SocketGetData( Client client );
extern void SocketSendToAll( char *fmt, ... );
extern Client SocketConnect( char * host, unsigned short port,
			void *data, 
			SocketInterpretation interpretation,
			SocketDelete handle_delete
			);
extern Client SocketConnectAddr( struct in_addr * addr, unsigned short port, 
			void *data, 
			SocketInterpretation interpretation,
			SocketDelete handle_delete
			);
/* Socket UDP */
/* Creation d'une socket en mode non connecte */
/* et ecoute des messages */
extern Client SocketBroadcastCreate(
			unsigned short port, 
			void *data, 
			SocketInterpretation interpretation
			);
/* Socket Multicast */
extern int SocketAddMember( Client client, unsigned long host );

/* recuperation de l'emetteur du message */
extern struct in_addr * SocketGetRemoteAddr( Client client );
extern void SocketGetRemoteHost (Client client, char **host, unsigned short *port );
/* emmission d'un broadcast UDP */
extern void SocketSendBroadcast( Client client, unsigned long host, unsigned short port, char *fmt, ... );
extern void SocketSendBroadcastRaw( Client client, unsigned long host, unsigned short port, char *buffer, int len );

#ifdef __cplusplus
}
#endif

#endif

