#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#define close closesocket
#define perror( a ) printf(a" error=%d\n",WSAGetLastError());
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
#ifdef XTMAINLOOP
#include <X11/Intrinsic.h>
#endif
#include "list.h"
#include "bussocket.h"
#include "timer.h"

#define MAX_BUFFER 2048

struct _channel {
	Channel next;
	HANDLE fd;
#ifdef XTMAINLOOP
	XtInputId id;
#endif
	void *data;
	int tobedeleted;
	void (*handle_delete)( void *data );
	void (*handle_read)( Channel channel, HANDLE fd, void *data);
	};

typedef struct _server *Server;

struct _server {
	Server next;
	Channel channel;
	void *(*create)(Client client);
	void (*handle_delete)(Client client, void *data);
	SocketInterpretation interpretation;
	};

struct _client {
	Client next;
	Channel channel;
	unsigned short port;
	struct sockaddr_in from;
	SocketInterpretation interpretation;
	void (*handle_delete)(Client client, void *data);
	char buffer[MAX_BUFFER+2];
	char *ptr;
	void *data;
	};

static Channel channels_list = NULL;
static Server servers_list = NULL;
static Client clients_list = NULL;
static int channel_initialized = 0;

#ifdef XTMAINLOOP
static XtAppContext    app;
#else
static fd_set open_fds;
static int MainLoop = 1;
#endif



#ifdef WIN32
WSADATA					WsaData;
#endif

void ChannelClose( Channel channel )
{
#ifdef XTMAINLOOP
	if ( channel->handle_delete )
		(*channel->handle_delete)( channel->data );
	close(channel->fd);
	XtRemoveInput( channel->id );
	LIST_REMOVE( channels_list, channel );
#else
	channel->tobedeleted = 1;
#endif
}
#ifdef XTMAINLOOP
static void HandleChannel( XtPointer closure, int* source, XtInputId* id )
{
	Channel channel = (Channel)closure;
#ifdef DEBUG
	printf("Handle Channel read %d\n",*source );
#endif
	(*channel->handle_read)(channel,channel->fd,channel->data);
}
#else
static void ChannelDelete( Channel channel )
{
	if ( channel->handle_delete )
		(*channel->handle_delete)( channel->data );
	close(channel->fd);

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
			ChannelDelete( channel );
			}
		}
}
#endif
Channel ChannelSetUp(HANDLE fd, void *data,
				void (*handle_delete)( void *data ),
				void (*handle_read)( Channel channel, HANDLE fd, void *data)
				)						
{
	Channel channel;

	LIST_ADD( channels_list, channel );
	if ( !channel )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		close( fd );
		exit(0);
		}
	channel->fd = fd;
	channel->tobedeleted = 0;
	channel->handle_delete = handle_delete;
	channel->handle_read = handle_read;
	channel->data = data;
#ifdef XTMAINLOOP
	channel->id = XtAppAddInput( app, fd, (XtPointer)XtInputReadMask, HandleChannel, channel);
#else
	FD_SET( channel->fd, &open_fds );
#endif
	return channel;
}
#ifndef XTMAINLOOP
static void ChannelHandleRead(fd_set *current)
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
static void ChannelHandleExcpt(fd_set *current)
{
	Channel channel,next;
	LIST_EACH_SAFE( channels_list, channel, next )
		{
		if (FD_ISSET( channel->fd, current ) )
			{
			ChannelClose( channel );
			}
		}
}
#endif
static void DeleteSocket(void *data)
{
	Client client = ( Client )data;
	if ( client->handle_delete )
		(*client->handle_delete)( client, client->data );
	shutdown( client->channel->fd, 2 );
	LIST_REMOVE( clients_list, client );
}
static void HandleSocket( Channel channel, HANDLE fd, void *data)
{
	Client client = (Client)data;
	char *ptr;
	char *ptr_nl;
	long nb_to_read = 0;
	long nb;
	int len;

	/* limitation taille buffer */
	nb_to_read = MAX_BUFFER - ( client->ptr - client->buffer );
	if( nb_to_read == 0 ) {
		fprintf(stderr, "Erreur message trop long sans LF\n");
		client->ptr  = client->buffer;
		return;
		};
	len = sizeof( client->from );
	nb = recvfrom( fd, client->ptr, nb_to_read,0,(struct sockaddr *)&client->from,&len);
	if (nb  < 0) {
		perror(" Read Socket ");
		ChannelClose( client->channel );
		return;
		};
	if ( nb == 0 )
		{
		ChannelClose( client->channel );
		return;
		}
	
	client->ptr += nb;
	*(client->ptr) = '\0';
	ptr = client->buffer;
	while( (ptr_nl = strchr( ptr, '\n' )))
		{
		*ptr_nl = '\0';
		if ( client->interpretation )
			(*client->interpretation)( client, client->data, ptr );
			else fprintf( stderr,"Socket No interpretation function ???!\n");
		ptr = ++ptr_nl;
		}
	if ( *ptr != '\0' )
		{ /* recopie ligne incomplete au debut du buffer */
		strcpy( client->buffer, ptr );
		client->ptr = client->buffer + strlen(client->buffer);
		}
		else
		{
		client->ptr = client->buffer;
		}
}
static void HandleServer(Channel channel, HANDLE fd, void *data)
{
	Server server = ( Server ) data;
	Client client;
	HANDLE ns;
	int addrlen;
	struct sockaddr_in remote2;

	addrlen = sizeof( remote2 );
	if ((ns = accept( fd, (struct sockaddr *)&remote2, &addrlen)) <0)
		{
		perror ( "*** accept ***");
		return;
		};
	LIST_ADD( clients_list, client );
	if ( !client )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		close( fd );
		exit(0);
		}
	client->from = remote2;
	client->channel = ChannelSetUp( ns, client,  DeleteSocket, HandleSocket );
	client->interpretation = server->interpretation;
	client->ptr = client->buffer;
	client->handle_delete = server->handle_delete;
	client->data = (*server->create)( client );
	
}
int SocketServer(unsigned short port, 
	void*(*create)(Client client),
	void(*handle_delete)(Client client, void *data),
	void(*interpretation)( Client client, void *data, char *ligne))
{
	Server server;
	HANDLE fd;
	int one=1;
	struct sockaddr_in local;
	int addrlen;
		

	if ((fd = socket( AF_INET, SOCK_STREAM, 0)) < 0){
		perror( "***open socket ***");
		exit(0);
		};

	
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY; 
	local.sin_port = htons (port);

	if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one)) < 0)
		{
		perror( "*** set socket option SO_REUSEADDR ***");
		exit(0);
		} 

#ifdef SO_REUSEPORT

	if (setsockopt( fd, SOL_SOCKET, SO_REUSEPORT, (char *)&one, sizeof( one)) < 0)
		{
		perror( "*** set socket option REUSEPORT ***");
		exit(0);
		}
#endif
	
	if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0)
		{
		perror( "*** bind ***");
		exit(0);
		}

	addrlen = sizeof( local );
	if (getsockname(fd,(struct sockaddr *)&local, &addrlen) < 0)
		{
		perror( "***get socket name ***");
		exit(0);
		} 
	
	if (listen( fd, 128) < 0){
		perror( "*** listen ***");
		exit(0);
		};
	

	LIST_ADD( servers_list, server );
	if ( !server )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		exit(0);
		}
	server->channel =	ChannelSetUp( fd, server, DeleteSocket, HandleServer );
	server->create = create;
	server->handle_delete = handle_delete;
	server->interpretation = interpretation;
	return ntohs(local.sin_port);
}
char *SocketGetPeerHost( Client client )
{
	int err;
	struct sockaddr_in name;
	struct hostent *host;
	int len = sizeof(name);
	err = getpeername( client->channel->fd, (struct sockaddr *)&name, &len );
	if ( err < 0 ) return "can't get peer";
	host = gethostbyaddr( (char *)&name.sin_addr.s_addr,sizeof(name.sin_addr.s_addr),name.sin_family);
	if ( host == NULL ) return "can't translate addr";
	return host->h_name;
}
struct in_addr * SocketGetRemoteAddr( Client client )
{
	return &client->from.sin_addr;
}
void SocketGetRemote( Client client, char **host, unsigned short *port )
{
	struct hostent *hostp;
	/* extract hostname and port from last message received */
	hostp = gethostbyaddr( (char *)&client->from.sin_addr.s_addr,
			sizeof(client->from.sin_addr.s_addr),client->from.sin_family);
	if ( hostp == NULL ) *host = "unknown";
		else *host = hostp->h_name;
	*port = ntohs( client->from.sin_port );
}
void SocketClose( Client client )
{
	ChannelClose( client->channel );
}

void SocketSendRaw( Client client, char *buffer, int len )
{
	int err;
	err = send( client->channel->fd, buffer, len, 0 );
	if ( err != len )
		perror( "*** send ***");
}
void SocketSetData( Client client, void *data )
{
	client->data = data;
}
void SocketSend( Client client, char *fmt, ... )
{
	char buffer[4096];
	va_list ap;
	int len;
	va_start( ap, fmt );
	len = vsprintf( buffer, fmt, ap );
	SocketSendRaw( client, buffer, len );
	va_end ( ap );
}
void *SocketGetData( Client client )
{
	return client->data;
}
void SocketBroadcast(  char *fmt, ... )
{
	Client client;
	char buffer[4096];
	va_list ap;
	int len;
	
	va_start( ap, fmt );
	len = vsprintf( buffer, fmt, ap );
	va_end ( ap );
	LIST_EACH( clients_list, client )
		{
		SocketSendRaw( client, buffer, len );
		}
}
/*
Ouverture d'un canal TCP/IP en mode client
*/
Client SocketConnect( char * host, unsigned short port, 
			void *data, 
			SocketInterpretation interpretation,
			void (*handle_delete)(Client client, void *data)
			)
{
struct hostent *rhost;


if ((rhost = gethostbyname( host )) == NULL){
     	fprintf(stderr, "Erreur %s Calculateur inconnu !\n",host);
      return NULL;
	};
return SocketConnectAddr( (struct in_addr*)(rhost->h_addr), port, data, interpretation, handle_delete);
}
Client SocketConnectAddr( struct in_addr * addr, unsigned short port, 
			void *data, 
			SocketInterpretation interpretation,
			void (*handle_delete)(Client client, void *data)
			)
{
HANDLE handle;
Client client;
struct sockaddr_in remote;

remote.sin_family = AF_INET;
remote.sin_addr = *addr;
remote.sin_port = htons (port);

if ((handle = socket( AF_INET, SOCK_STREAM, 0)) < 0){
	perror( "*** client socket ***");
	return NULL;
	};

if ( connect( handle,  (struct sockaddr *)&remote, sizeof(remote) ) < 0){
	perror( "*** client connect ***");
	return NULL;
	};

LIST_ADD( clients_list, client );
if ( !client )
	{
	fprintf(stderr,"NOK Memory Alloc Error\n");
	close( handle );
	exit(0);
	}
	

client->channel = ChannelSetUp( handle, client,  DeleteSocket, HandleSocket );
client->interpretation = interpretation;
client->ptr = client->buffer;
client->data = data;
client->handle_delete = handle_delete;
client->from.sin_family = AF_INET;
client->from.sin_addr = *addr;
client->from.sin_port = htons (port);

return client;
}
int SocketWaitForReply( Client client, char *buffer, int size, int delai)
{
	fd_set rdset;
	struct timeval timeout;
	struct timeval *timeoutptr = &timeout;
	int ready;
	char *ptr;
	char *ptr_nl;
	long nb_to_read = 0;
	long nb;
	HANDLE fd;

	fd = client->channel->fd;
	ptr = buffer;
	timeout.tv_sec = delai;
	timeout.tv_usec = 0;
   	do {
		/* limitation taille buffer */
		nb_to_read = size - ( ptr - buffer );
		if( nb_to_read == 0 )
			{
			fprintf(stderr, "Erreur message trop long sans LF\n");
			ptr  = buffer;
			return -1;
			}
		FD_ZERO( &rdset );
		FD_SET( fd, &rdset );
		ready = select(fd+1, &rdset, 0,  0, timeoutptr);
		if ( ready < 0 )
			{
			perror("select");
			return -1;
			}
		if ( ready == 0 )
			{
			return -2;
			}
		if ((nb = recv( fd , ptr, nb_to_read, 0 )) < 0)
			{
			perror(" Read Socket ");
			return -1;
			}
		if ( nb == 0 )
			return 0;

		ptr += nb;
		*ptr = '\0';
		ptr_nl = strchr( buffer, '\n' );
	} while ( !ptr_nl );
	*ptr_nl = '\0';
	return (ptr_nl - buffer);
}
void ChannelInit(void)
{
#ifdef WIN32
	int error;
#else 
	signal( SIGPIPE, SIG_IGN);
#endif
	if ( channel_initialized ) return;
#ifndef XTMAINLOOP
	FD_ZERO( &open_fds );
#endif
#ifdef WIN32
	error = WSAStartup( 0x0101, &WsaData );
        if ( error == SOCKET_ERROR ) {
            printf( "WSAStartup failed.\n" );
        }
#endif
	channel_initialized = 1;
}

#ifdef XTMAINLOOP

void SetSocketAppContext( XtAppContext cntx )
{
	app = cntx;
}
	
void ChannelMainLoop(void(*hook)(void))
{
	printf("Compiled for Use of XtMainLoop not ChannelMainLoop\n");
	exit(-1);
}
#else
void ChannelStop(void)
{
	MainLoop = 0;
}
void ChannelMainLoop(void(*hook)(void))
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
		ChannelHandleExcpt(&exset);
		ChannelHandleRead(&rdset);
		continue;
		}
	}
}
#endif
/* Socket UDP */

Client SocketBroadcastCreate( unsigned short port, 
			void *data, 
			SocketInterpretation interpretation
			)
{
HANDLE handle;
Client client;
struct sockaddr_in local;
int on = 1;

local.sin_family = AF_INET;
local.sin_addr.s_addr = INADDR_ANY;
local.sin_port = htons (port);

if ((handle = socket( AF_INET, SOCK_DGRAM, 0)) < 0){
	perror( "*** dgram socket ***");
	return NULL;
	};

/* wee need to used multiple client on the same host */
if (setsockopt( handle, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof( on)) < 0)
		{
		perror( "*** set socket option REUSEADDR ***");
		return NULL;
		};
#ifdef SO_REUSEPORT

if (setsockopt( fd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof( on)) < 0)
		{
		perror( "*** set socket option REUSEPORT ***");
		return NULL;
		}
#endif
/* wee need to broadcast */
if (setsockopt( handle, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof( on)) < 0)
			{
			perror( "*** BROADCAST ***");
			return NULL;
			};

if (bind(handle, (struct sockaddr *)&local, sizeof(local)) < 0)
	{
	perror( "*** test BIND ***");
	return NULL;
	};

LIST_ADD( clients_list, client );
if ( !client )
	{
	fprintf(stderr,"NOK Memory Alloc Error\n");
	close( handle );
	exit(0);
	}
	

client->channel = ChannelSetUp( handle, client,  DeleteSocket, HandleSocket );
client->interpretation = interpretation;
client->ptr = client->buffer;
client->data = data;

return client;
}

void SocketSendBroadcast( Client client, unsigned long host, unsigned short port, char *fmt, ... )
{
	struct sockaddr_in remote;
	char buffer[4096];
	va_list ap;
	int err,len;
	
	va_start( ap, fmt );
	len = vsprintf( buffer, fmt, ap );
	/* Send UDP packet to the dest */
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = htonl( host );
	remote.sin_port = htons(port);
	err = sendto( client->channel->fd, 
			buffer, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if ( err != len )
		{
		perror( "*** send ***");
		}	va_end ( ap );
}
