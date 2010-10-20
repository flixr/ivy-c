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



#ifdef OPENMP
#include <omp.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

#ifdef WIN32
#ifndef __MINGW32__
typedef long ssize_t;
#endif
#define close closesocket
/*#define perror (a ) printf(a" error=%d\n",WSAGetLastError());*/
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif

#include "param.h"
#include "list.h"
#include "ivychannel.h"
#include "ivysocket.h"
#include "ivyloop.h"
#include "ivybuffer.h"
#include "ivyfifo.h"
#include "ivydebug.h"

struct _server {
	Server next;
	HANDLE fd;
	Channel channel;
	unsigned short port;
	int ipv6;
	void *(*create)(Client client);
	void (*handle_delete)(Client client, const void *data);
	void (*handle_decongestion)(Client client, const void *data);
	SocketInterpretation interpretation;
};

struct _client {
	Client next;
	HANDLE fd;
	Channel channel;
	unsigned short port;
	char app_uuid[128];
	int ipv6;
	struct sockaddr* from; // use ptr dynamic aloca against IPV6 ou IPV4
	SocketInterpretation interpretation;
	void (*handle_delete)(Client client, const void *data);
	void (*handle_decongestion)(Client client, const void *data);
	char terminator;	/* character delimiter of the message */ 
	/* Buffer de reception */
	long buffer_size;
	char *buffer;		/* dynamicaly reallocated */
	char *ptr;
	/* Buffer d'emission */
        IvyFifoBuffer *ifb;             /* le buffer circulaire en cas de congestion */
  	/* user data */
	const void *data;
#ifdef OPENMP
	omp_lock_t fdLock;
#endif
};
 

static Server servers_list = NULL;
static Client clients_list = NULL;

static int debug_send = 0;

#ifdef WIN32
WSADATA	WsaData;
#endif

// tricky things to initialise socket ADDR 
#define InitAddr(  ipv6, port ) \
	struct sockaddr_in sock; \
	struct sockaddr_in6 sock6; \
	struct sockaddr* sock_addr; \
	socklen_t sock_addr_len; \
	\
	if ( ipv6 ) \
	{ \
	memset( &sock6,0,sizeof(sock6) ); \
	sock6.sin6_family =  AF_INET6; \
	sock6.sin6_addr = in6addr_any; \
	sock6.sin6_port = htons (port); \
	sock_addr = (struct sockaddr*)&sock6; \
	sock_addr_len = sizeof( sock6 ); \
	} \
	else \
	{ \
	memset( &sock,0,sizeof(sock) ); \
	sock.sin_family =  AF_INET; \
	sock.sin_addr.s_addr = INADDR_ANY; \
	sock.sin_port = htons (port); \
	sock_addr = (struct sockaddr*)&sock; \
	sock_addr_len = sizeof( sock ); \
	}\

static struct sockaddr* DupAddr( struct sockaddr* addr, socklen_t len ) 
{
	struct sockaddr*  dupaddr = (struct sockaddr*) malloc( len );
	memcpy( addr, dupaddr , len );
	return dupaddr;
}
static SendState BufferizedSocketSendRaw (const Client client, const char *buffer, const int len );


void SocketInit()
{
	if ( getenv( "IVY_DEBUG_SEND" )) debug_send = 1;
	IvyChannelInit();
}

static void DeleteSocket(void *data)
{
	Client client = (Client )data;
	if (client->handle_delete )
		(*client->handle_delete) (client, client->data );
	shutdown (client->fd, 2 );
	close (client->fd );
#ifdef OPENMP
	omp_destroy_lock (&(client->fdLock));
#endif
	if (client->ifb != NULL) {
	  IvyFifoDelete (client->ifb);
	  client->ifb = NULL;
	}
	free( client->from );
	IVY_LIST_REMOVE (clients_list, client );
}


static void DeleteServerSocket(void *data)
{
        Server server = (Server )data;
#ifdef BUGGY_END
        if (server->handle_delete )
                (*server->handle_delete) (server, NULL );
#endif
        shutdown (server->fd, 2 );
        close (server->fd );
        IVY_LIST_REMOVE (servers_list, server);
}


static void HandleSocket (Channel channel, HANDLE fd, void *data)
{
	Client client = (Client)data;
	char *ptr;
	char *ptr_nl;
	long nb_to_read = 0;
	long nb;
	long nb_occuped;
	socklen_t len;
	
	/* limitation taille buffer */
	nb_occuped = client->ptr - client->buffer;
	nb_to_read = client->buffer_size - nb_occuped;
	if (nb_to_read == 0 ) {
		client->buffer_size *= 2; /* twice old size */
		client->buffer = realloc( client->buffer, client->buffer_size );
		if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		fprintf(stderr, "Buffer Limit reached realloc new size %ld\n", client->buffer_size );
		nb_to_read = client->buffer_size - nb_occuped;
		client->ptr = client->buffer + nb_occuped; 
	}
	len = client->ipv6 ? sizeof (struct sockaddr_in6 ) : sizeof (struct sockaddr_in ) ;
	nb = recvfrom (fd, client->ptr, nb_to_read,0,client->from, &len);
	if (nb  < 0) {
		perror(" Read Socket ");
		IvyChannelRemove (client->channel );
		return;
	}
	if (nb == 0 ) {
		IvyChannelRemove (client->channel );
		return;
	}
	client->ptr += nb;
	ptr = client->buffer;
	while ((ptr_nl = memchr (ptr, client->terminator,  client->ptr - ptr )))
		{
		*ptr_nl ='\0';
		if (client->interpretation )
			(*client->interpretation) (client, client->data, ptr );
			else fprintf (stderr,"Socket No interpretation function ???\n");
		ptr = ++ptr_nl;
		}
	if (ptr < client->ptr )
		{ /* recopie ligne incomplete au debut du buffer */
		len = client->ptr - ptr;
		memmove (client->buffer, ptr, len  );
		client->ptr = client->buffer + len;
		}
		else
		{
		client->ptr = client->buffer;
		}
}



static void HandleCongestionWrite (Channel channel, HANDLE fd, void *data)
{
  Client client = (Client)data;
  
  if (IvyFifoSendSocket (client->ifb, fd) == 0) {
    // Not congestionned anymore
    IvyChannelClearWritableEvent (channel);
    //    printf ("DBG> Socket *DE*congestionnee\n");
    IvyFifoDelete (client->ifb);
    client->ifb = NULL;
    if (client->handle_decongestion )
      (*client->handle_decongestion) (client, client->data );

  }
}


static void HandleServer(Channel channel, HANDLE fd, void *data)
{
	Server server = (Server ) data;
	Client client;
	HANDLE ns;
#ifdef WIN32
	u_long iMode = 1; /* non blocking Mode */
#else
	long   socketFlag;
#endif
	InitAddr( server->ipv6, 0 );
	TRACE( "Accepting Connection...\n");

	if ((ns = accept (fd, sock_addr, &sock_addr_len)) <0)
		{
		perror ("*** accept ***");
		return;
		};

	TRACE( "Accepting Connection ret\n");

	IVY_LIST_ADD_START (clients_list, client );
	
	client->buffer_size = IVY_BUFFER_SIZE;
	client->buffer = malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->ipv6 = server->ipv6;
	client->from = DupAddr( sock_addr, sock_addr_len );
	client->fd = ns;
	client->ifb = NULL;
	strcpy (client->app_uuid, "init by HandleServer");

#ifdef WIN32
	if ( ioctlsocket(client->fd,FIONBIO, &iMode ) )
		fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
#else
	socketFlag = fcntl (client->fd, F_GETFL);
	if (fcntl (client->fd, F_SETFL, socketFlag|O_NONBLOCK)) {
	  fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
	}
#endif
	if (setsockopt(client->fd,            /* socket affected */
		       IPPROTO_TCP,     /* set option at TCP level */
		       TCP_NODELAY,     /* name of option */
		       (char *) &TCP_NO_DELAY_ACTIVATED,  /* the cast is historical */
 		       sizeof(TCP_NO_DELAY_ACTIVATED)) < 0)    /* length of option value */
	  {
#ifdef WIN32
	    fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
	    perror ("*** set socket option  TCP_NODELAY***");
	    exit(0);
	  } 




	client->channel = IvyChannelAdd (ns, client,  DeleteSocket, HandleSocket,
					 HandleCongestionWrite);
	client->interpretation = server->interpretation;
	client->ptr = client->buffer;
	client->handle_delete = server->handle_delete;
	client->handle_decongestion = server->handle_decongestion;
	client->data = (*server->create) (client );
#ifdef OPENMP
	omp_init_lock (&(client->fdLock));
#endif


	IVY_LIST_ADD_END (clients_list, client );
	
}

Server SocketServer(int ipv6, unsigned short port, 
	void*(*create)(Client client),
	void(*handle_delete)(Client client, const void *data),
        void(*handle_decongestion)(Client client, const void *data),
	void(*interpretation) (Client client, const void *data, char *ligne))
{
	Server server;
	HANDLE fd;
	int one=1;
	InitAddr(  ipv6, port );
		

	if ((fd = socket (ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) < 0){
		perror ("***open socket ***");
		exit(0);
		};

	
	if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one)) < 0)
	  {
#ifdef WIN32
	    fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
	    perror ("*** set socket option SO_REUSEADDR ***");
	    exit(0);
	  } 

#ifdef SO_REUSEPORT

	if (setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (char *)&one, sizeof (one)) < 0)
	  {
	    perror ("*** set socket option REUSEPORT ***");
	    exit(0);
	  }
#endif
	
	

	if (bind(fd, sock_addr, sock_addr_len) < 0)
		{
		perror ("*** bind ***");
		exit(0);
		}

	if (getsockname(fd,sock_addr, &sock_addr_len) < 0)
		{
		perror ("***get socket name ***");
		exit(0);
		} 
	
	if (listen (fd, 128) < 0){
		perror ("*** listen ***");
		exit(0);
		};
	

	IVY_LIST_ADD_START (servers_list, server );
	server->fd = fd;
	server->channel = IvyChannelAdd (fd, server, DeleteServerSocket, 
					 HandleServer, NULL);
	server->ipv6 = ipv6;
	server->create = create;
	server->handle_delete = handle_delete;
	server->handle_decongestion = handle_decongestion;
	server->interpretation = interpretation;
	server->port = ntohs(ipv6 ? ((struct sockaddr_in6 *)(sock_addr))->sin6_port : ((struct sockaddr_in *)(sock_addr))->sin_port);
	IVY_LIST_ADD_END (servers_list, server );
	
	return server;
}

unsigned short SocketServerGetPort (Server server )
{
	return server ? server->port : 0;
}

void SocketServerClose (Server server )
{
	if (!server)
		return;
	IvyChannelRemove (server->channel );
}

char *SocketGetPeerHost (Client client )
{
	int err;
	struct hostent *host;
	
	if (!client)
		return "undefined";
	if ( client->ipv6 )
	{
		struct sockaddr_in6 name;
		socklen_t len = sizeof(name);
		err = getpeername (client->fd, (struct sockaddr *)&name, &len );
		if (err < 0 ) return "can't get peer";
		host = gethostbyaddr (&name.sin6_addr,sizeof(name.sin6_addr),name.sin6_family);
	}
	else
	{
		struct sockaddr_in name;
		socklen_t len = sizeof(name);
		err = getpeername (client->fd, (struct sockaddr *)&name, &len );
		if (err < 0 ) return "can't get peer";
		host = gethostbyaddr (&name.sin_addr,sizeof(name.sin_addr),name.sin_family);
	
	}
	if (host == NULL ) 
	{
		fprintf(stderr, "SocketGetPeerHost :: gethostbyaddr %s\n", hstrerror( h_errno) );
		return "can't translate addr";
	}
	return host->h_name;
}

unsigned short int SocketGetLocalPort ( Client client )
{
	int err;
	struct sockaddr_in name;
	socklen_t len = sizeof(name);

	if (!client)
		return 0;

	err = getsockname (client->fd, (struct sockaddr *)&name, &len );
	if (err < 0 ) return 0;
	return name.sin_port;;
}
unsigned short int SocketGetRemotePort ( Client client )
{
	if (!client)
		return 0;
	if ( client->ipv6 )
		return ((struct sockaddr_in6*)(client->from ))->sin6_port;
	return ((struct sockaddr_in*)(client->from ))->sin_port;
}

struct sockaddr * SocketGetRemoteAddr (Client client )
{
	return client ? client->from : 0;
}

void SocketGetRemoteHost (Client client, char **host, unsigned short *port )
{
	struct hostent *hostp;

	if (!client)
		return;

	/* extract hostname and port from last message received */
	if ( client->ipv6 )
	{
		struct sockaddr_in6* sock_addr = (struct sockaddr_in6*)(client->from);
		hostp = gethostbyaddr ((char *)&sock_addr->sin6_addr,
			sizeof(sock_addr->sin6_addr),sock_addr->sin6_family);
		*port = ntohs (sock_addr->sin6_port );
	}
	else
	{
		struct sockaddr_in* sock_addr = (struct sockaddr_in*)(client->from);
		hostp = gethostbyaddr ((char *)&sock_addr->sin_addr,
			sizeof(sock_addr->sin_addr),sock_addr->sin_family);
		*port = ntohs (sock_addr->sin_port );
	}
	
	if (hostp == NULL )
	{
		fprintf(stderr, "SocketGetRemoteHost :: gethostbyaddr %s\n", hstrerror( h_errno) );
		*host = "unknown";
	}
		else *host = hostp->h_name;
	
}

void SocketClose (Client client )
{
	if (client)
		IvyChannelRemove (client->channel );
}

SendState SocketSendRaw (const Client client, const char *buffer, const int len )
{
  SendState state;
  
  if (!client)
    return SendParamError;
  
#ifdef OPENMP
  omp_set_lock (&(client->fdLock));
#endif
  
  state = BufferizedSocketSendRaw (client, buffer, len);

#ifdef OPENMP
  omp_unset_lock (&(client->fdLock));
#endif
  
  return state;
}


static SendState BufferizedSocketSendRaw (const Client client, const char *buffer, const int len )
{
  ssize_t reallySent;
  SendState state;

  if (client->ifb != NULL) {
    // Socket en congestion : on rajoute juste le flux dans le buffer, 
    // quand la socket sera dispo en ecriture, le select appellera la callback
    // pour vider ce buffer
    IvyFifoWrite (client->ifb, buffer, len);
    state = IvyFifoIsFull (client->ifb) ? SendStateFifoFull : SendStillCongestion;
  } else {
    // on tente d'ecrire direct dans la socket
    reallySent =  send (client->fd, buffer, len, 0);
    if (reallySent == len) 
	{
      state = SendOk; // PAS CONGESTIONNEE
    } else if (reallySent == -1) 
	{
#ifdef WIN32
	if ( WSAGetLastError() == WSAEWOULDBLOCK) {
#else
      if (errno == EWOULDBLOCK) {
#endif
	// Aucun octet n'a été envoyé, mais le send ne rend pas 0
	// car 0 peut être une longueur passée au send, donc dans ce cas
	// send renvoie -1 et met errno a EWOULDBLOCK
	client->ifb = IvyFifoNew ();
	IvyFifoWrite (client->ifb, buffer, len);
	// on ajoute un fdset pour que le select appelle une callback pour vider
	// le buffer quand la socket sera ?? nouveau libre
	IvyChannelAddWritableEvent (client->channel);
	state = SendStateChangeToCongestion;
      } else {
	state = SendError; // ERREUR
      }
    } else {
      // socket congestionnée
      // on initialise une fifo pour accumuler les données
      client->ifb = IvyFifoNew ();
      IvyFifoWrite (client->ifb, &(buffer[reallySent]), len-reallySent);
      // on ajoute un fdset pour que le select appelle une callback pour vider
      // le buffer quand la socket sera à nouveau libre
      IvyChannelAddWritableEvent (client->channel);
      state = SendStateChangeToCongestion;
    }
  }

#ifdef DEBUG
  // DBG BEGIN DEBUG
  /* SendOk, SendStillCongestion, SendStateChangeToCongestion,
          SendStateChangeToDecongestion, SendStateFifoFull, SendError,
	  SendParamError
  */
  {
    static SendState DBG_state = SendOk;
    char *litState="";
    if (state != DBG_state) {
      switch (state) {
      case SendOk : litState = "SendOk";
	break;
      case  SendStillCongestion: litState = "SendStillCongestion";
	break;
      case SendStateChangeToCongestion : litState = "SendStateChangeToCongestion";
	break;
      case  SendStateChangeToDecongestion: litState = "SendStateChangeToDecongestion";
	break;
      case  SendStateFifoFull: litState = "SendStateFifoFull";
	break;
      case  SendError: litState = "SendError";
	break;
      case  SendParamError: litState = "SendParamError";
	break;
      }
      printf ("DBG>> BufferizedSocketSendRaw, state changed to '%s'\n", litState);
      DBG_state = state;
    }
  }
  // DBG END DEBUG
#endif

  return (state);
}



SendState SocketSendRawWithId( const Client client, const char *id, const char *buffer, const int len )
{
  SendState s1, s2;
  
#ifdef OPENMP
  omp_set_lock (&(client->fdLock));
#endif
  
  s1 = BufferizedSocketSendRaw (client, id, strlen (id));

  s2 = BufferizedSocketSendRaw (client, buffer, len);
  
#ifdef OPENMP
  omp_unset_lock (&(client->fdLock));
#endif
  
  if (s1 == SendStateChangeToCongestion) {
    // si le passage en congestion s'est fait sur l'envoi de l'id
    s2 = s1;
  }

  return (s2);
}


void SocketSetData (Client client, const void *data )
{
  if (client) {
    client->data = data;
  }
}

SendState SocketSend (Client client, char *fmt, ... )
{
  SendState state;
  static IvyBuffer buffer = {NULL, 0, 0 }; /* Use static mem to eliminate multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif

  va_list ap;
  int len;
  va_start (ap, fmt );
  buffer.offset = 0;
  len = make_message (&buffer, fmt, ap );
  state = SocketSendRaw (client, buffer.data, len );
  va_end (ap );
  return state;
}

const void *SocketGetData (Client client )
{
	return client ? client->data : 0;
}

void SocketBroadcast ( char *fmt, ... )
{
	Client client;
	static IvyBuffer buffer = {NULL, 0, 0 }; /* Use static mem to eliminate 
						    multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif
	va_list ap;
	int len;
	
	va_start (ap, fmt );
	len = make_message (&buffer, fmt, ap );
	va_end (ap );
	IVY_LIST_EACH (clients_list, client )
		{
		SocketSendRaw (client, buffer.data, len );
		}
}

/*
Ouverture d'un canal TCP/IP en mode client
*/
//Client SocketConnect (int ipv6, char * host, unsigned short port, 
//			void *data, 
//			SocketInterpretation interpretation,
//		        void (*handle_delete)(Client client, const void *data),
//		        void(*handle_decongestion)(Client client, const void *data)
//			)
//{
//	struct hostent *rhost;
//
//	if ((rhost = gethostbyname (host )) == NULL) {
//		fprintf(stderr, "Erreur %s Calculateur inconnu !\n",host);
//		 return NULL;
//	}
//	return SocketConnectAddr (ipv6, rhost->h_addr, port, data, 
//				  interpretation, handle_delete, handle_decongestion);
//}

Client SocketConnectAddr (int ipv6, struct sockaddr * addr, unsigned short port, 
			  void *data, 
			  SocketInterpretation interpretation,
			  void (*handle_delete)(Client client, const void *data),
		          void(*handle_decongestion)(Client client, const void *data)
			  )
{
	HANDLE handle;
	Client client;
#ifdef WIN32
	u_long iMode = 1; /* non blocking Mode */
#else
	long   socketFlag;
#endif
	InitAddr( ipv6, port );
	if ( ipv6 )
	{
		sock6.sin6_addr =  ((struct sockaddr_in6*)addr)->sin6_addr;
		sock6.sin6_scope_id =  ((struct sockaddr_in6*)addr)->sin6_scope_id;
		sock6.sin6_flowinfo =  ((struct sockaddr_in6*)addr)->sin6_flowinfo;
	}
	else
	{
		sock.sin_addr = ((struct sockaddr_in*)addr)->sin_addr;
	}
	if ((handle = socket ( ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) < 0){
		perror ("*** client socket ***");
		return NULL;
	};

	if (connect (handle, sock_addr, sock_addr_len ) < 0){
		perror ("*** client connect ***");
		return NULL;
	};
#ifdef WIN32
	if ( ioctlsocket(handle,FIONBIO, &iMode ) )
		fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
#else
	socketFlag = fcntl (handle, F_GETFL);
	if (fcntl (handle, F_SETFL, socketFlag|O_NONBLOCK)) {
	  fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
	}
#endif
	if (setsockopt(handle,            /* socket affected */
		       IPPROTO_TCP,     /* set option at TCP level */
		       TCP_NODELAY,     /* name of option */
		       (char *) &TCP_NO_DELAY_ACTIVATED,  /* the cast is historical */
 		       sizeof(TCP_NO_DELAY_ACTIVATED)) < 0)    /* length of option value */
	  {
#ifdef WIN32
	    fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
	    perror ("*** set socket option  TCP_NODELAY***");
	    exit(0);
	  } 


	IVY_LIST_ADD_START(clients_list, client );
	
	client->buffer_size = IVY_BUFFER_SIZE;
	client->buffer = malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->fd = handle;
	client->ipv6 = ipv6;
	client->channel = IvyChannelAdd (handle, client,  DeleteSocket, 
					 HandleSocket, HandleCongestionWrite );
	client->interpretation = interpretation;
	client->ptr = client->buffer;
	client->data = data;
	client->handle_delete = handle_delete;
	client->handle_decongestion = handle_decongestion;
	client->from = DupAddr( sock_addr, sock_addr_len );
	client->ifb = NULL;
	strcpy (client->app_uuid, "init by SocketConnectAddr");


#ifdef OPENMP
	omp_init_lock (&(client->fdLock));
#endif
	IVY_LIST_ADD_END(clients_list, client );
	

	return client;
}
/* TODO factoriser avec HandleRead !!!! */
int SocketWaitForReply (Client client, char *buffer, int size, int delai)
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

	fd = client->fd;
	ptr = buffer;
	timeout.tv_sec = delai;
	timeout.tv_usec = 0;
   	do {
		/* limitation taille buffer */
		nb_to_read = size - (ptr - buffer );
		if (nb_to_read == 0 )
			{
			fprintf(stderr, "Erreur message trop long sans LF\n");
			ptr  = buffer;
			return -1;
			}
		FD_ZERO (&rdset );
		FD_SET (fd, &rdset );
		ready = select(fd+1, &rdset, 0,  0, timeoutptr);
		if (ready < 0 )
			{
			perror("select");
			return -1;
			}
		if (ready == 0 )
			{
			return -2;
			}
		if ((nb = recv (fd , ptr, nb_to_read, 0 )) < 0)
			{
			perror(" Read Socket ");
			return -1;
			}
		if (nb == 0 )
			return 0;

		ptr += nb;
		*ptr = '\0';
		ptr_nl = strchr (buffer, client->terminator );
	} while (!ptr_nl );
	*ptr_nl = '\0';
	return (ptr_nl - buffer);
}

/* Socket UDP */

Client SocketBroadcastCreate (int ipv6, unsigned short port, 
				void *data, 
				SocketInterpretation interpretation
			)
{
	HANDLE handle;
	Client client;
	int on = 1;
	InitAddr(  ipv6, port );

	if ((handle = socket ( ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		perror ("*** dgram socket ***");
		return NULL;
	};

	/* wee need to used multiple client on the same host */
	if (setsockopt (handle, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0)
		{
			perror ("*** set socket option REUSEADDR ***");
			return NULL;
		};
#ifdef SO_REUSEPORT

	if (setsockopt (handle, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof (on)) < 0)
		{
			perror ("*** set socket option REUSEPORT ***");
			return NULL;
		}
#endif
	/* wee need to broadcast */
	if (setsockopt (handle, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof (on)) < 0)
		{
			perror ("*** BROADCAST ***");
			return NULL;
		};

	if (bind(handle, sock_addr, sock_addr_len ) < 0)
		{
			perror ("*** BIND ***");
			return NULL;
		};

	IVY_LIST_ADD_START(clients_list, client );
	
	client->buffer_size = IVY_BUFFER_SIZE;
	client->buffer = malloc( client->buffer_size );
	if (!client->buffer )
		{
		perror("HandleSocket Buffer Memory Alloc Error: ");
		exit(0);
		}
	client->terminator = '\n';
	client->fd = handle;
	client->ipv6 = ipv6;
	client->from = DupAddr( sock_addr, sock_addr_len );
	client->channel = IvyChannelAdd (handle, client,  DeleteSocket, 
					 HandleSocket, HandleCongestionWrite);
	client->interpretation = interpretation;
	client->ptr = client->buffer;
	client->data = data;
	client->ifb = NULL;
	strcpy (client->app_uuid, "init by SocketBroadcastCreate");

#ifdef OPENMP
	omp_init_lock (&(client->fdLock));
#endif
	IVY_LIST_ADD_END(clients_list, client );
	
	return client;
}
/* TODO unifier les deux fonctions */
void SocketSendBroadcast (Client client, unsigned long host, unsigned short port, char *fmt, ... )
{
	struct sockaddr_in remote;
	static IvyBuffer buffer = { NULL, 0, 0 }; /* Use satic mem to eliminate multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif
	va_list ap;
	int err,len;

	if (!client)
		return;

	va_start (ap, fmt );
	buffer.offset = 0;
	len = make_message (&buffer, fmt, ap );
	va_end (ap );
	/* Send UDP packet to the dest */
	memset( &remote,0,sizeof(remote) );
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = htonl (host );
	remote.sin_port = htons(port);
	err = sendto (client->fd, 
			buffer.data, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if (err != len) {
		perror ("*** send ***");
	}	
	
}

void SocketSendBroadcast6 (Client client, struct in6_addr* host, unsigned short port, char *fmt, ... )
{
	struct sockaddr_in6 remote;
	static IvyBuffer buffer = { NULL, 0, 0 }; /* Use satic mem to eliminate multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif
	va_list ap;
	int err,len;

	if (!client)
		return;

	va_start (ap, fmt );
	buffer.offset = 0;
	len = make_message (&buffer, fmt, ap );
	va_end (ap );
	/* Send UDP packet to the dest */
	memset( &remote,0,sizeof(remote) );
	remote.sin6_family = AF_INET6;
	remote.sin6_addr = *host;
	remote.sin6_port = htons(port);
	err = sendto (client->fd, 
			buffer.data, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if (err != len) {
		perror ("*** send ***");
	}	
	
}


/* Socket Multicast */

int SocketAddMember(Client client, unsigned long host )
{
	struct ip_mreq imr;
/*
Multicast datagrams with initial TTL 0 are restricted to the same host. 
Multicast datagrams with initial TTL 1 are restricted to the same subnet. 
Multicast datagrams with initial TTL 32 are restricted to the same site. 
Multicast datagrams with initial TTL 64 are restricted to the same region. 
Multicast datagrams with initial TTL 128 are restricted to the same continent. 
Multicast datagrams with initial TTL 255 are unrestricted in scope. 
*/
	unsigned char ttl = 64 ; /* Arbitrary TTL value. */
	/* wee need to broadcast */

	imr.imr_multiaddr.s_addr = htonl( host );
	imr.imr_interface.s_addr = INADDR_ANY; 
	if(setsockopt(client->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&imr,sizeof(imr)) == -1 )
		{
      	perror("setsockopt() Cannot join group");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}
				
  	if(setsockopt(client->fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0 )
		{
      	perror("setsockopt() Cannot set TTL");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}

	return 1;
}

int SocketAddMember6(Client client, struct in6_addr* host )
{
	struct ipv6_mreq imr;
/*
Multicast datagrams with initial TTL 0 are restricted to the same host. 
Multicast datagrams with initial TTL 1 are restricted to the same subnet. 
Multicast datagrams with initial TTL 32 are restricted to the same site. 
Multicast datagrams with initial TTL 64 are restricted to the same region. 
Multicast datagrams with initial TTL 128 are restricted to the same continent. 
Multicast datagrams with initial TTL 255 are unrestricted in scope. 
*/
	//unsigned char ttl = 64 ; /* Arbitrary TTL value. */
	/* wee need to broadcast */

	imr.ipv6mr_multiaddr = *host;
	imr.ipv6mr_interface = 0;  /// TODO: Attention ca fait quoi ca ???
	if(setsockopt(client->fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *)&imr,sizeof(imr)) == -1 )
		{
      	perror("setsockopt() Cannot join group");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}
				
  	/*
	if(setsockopt(client->fd, IPPROTO_IPV6, IPV6_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0 )
		{
      	perror("setsockopt() Cannot set TTL");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}
	*/

	return 1;
}

extern void SocketSetUuid (Client client, const char *uuid)
{
  strncpy (client->app_uuid, uuid, sizeof (client->app_uuid));
}

const char* SocketGetUuid (const Client client)
{
  return client->app_uuid;
}

extern int  SocketCmpUuid (const Client c1, const Client c2)
{
  return strncmp (c1->app_uuid, c2->app_uuid, sizeof (c1->app_uuid));
}


