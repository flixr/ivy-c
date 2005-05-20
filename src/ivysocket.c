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

#ifdef WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>

#ifdef WIN32
#define close closesocket
/*#define perror (a ) printf(a" error=%d\n",WSAGetLastError());*/
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
#include "ivysocket.h"
#include "ivyloop.h"

#define BUFFER_SIZE 4096	/* taille buffer initiale on multiple pas deux a chaque realloc */

struct _server {
	Server next;
	HANDLE fd;
	Channel channel;
	unsigned short port;
	void *(*create)(Client client);
	void (*handle_delete)(Client client, void *data);
	SocketInterpretation interpretation;
};

struct _client {
	Client next;
	HANDLE fd;
	Channel channel;
	unsigned short port;
	struct sockaddr_in from;
	SocketInterpretation interpretation;
	void (*handle_delete)(Client client, void *data);
	FILE *socket_output;	/* Handle buffered output */
	char terminator;	/* character delimiter of the message */ 
	long buffer_size;
	char *buffer;		/* dynamicaly reallocated */
	char *ptr;
	void *data;
};

static Server servers_list = NULL;
static Client clients_list = NULL;

#ifdef WIN32
WSADATA	WsaData;
#endif

// fonction de formtage a la printf d'un buffer avec reallocation dynamique  
int make_message(char ** buffer, int *size,  const char *fmt, va_list ap)
{
    /* Guess we need no more than BUFFER_INIT_SIZE bytes. */
    int n;
	if ( *size == 0 || *buffer == NULL )
		{
		*size = BUFFER_SIZE;
		*buffer = malloc (BUFFER_SIZE);
		if ( *buffer == NULL )
		    return -1;
		}
    while (1) {
    /* Try to print in the allocated space. */
    n = vsnprintf (*buffer, *size, fmt, ap);
    /* If that worked, return the string size. */
    if (n > -1 && n < *size)
        return n;
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
        *size = n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
        *size *= 2;  /* twice the old size */
    if ((*buffer = realloc (*buffer, *size)) == NULL)
        return -1;
    }
}


void SocketInit()
{
	if (! channel_init )
	{
		fprintf (stderr, "Channel management functions not set, exiting.\n");
		exit(-1);
	}
	(*channel_init)();
}

static void DeleteSocket(void *data)
{
	Client client = (Client )data;
	if (client->handle_delete )
		(*client->handle_delete) (client, client->data );
	shutdown (client->fd, 2 );
	close (client->fd );
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
	socklen_t len;
	
	/* limitation taille buffer */
	nb_to_read = client->buffer_size - (client->ptr - client->buffer );
	if (nb_to_read == 0 ) {
		client->buffer_size *= 2; /* twice old size */
		client->buffer = realloc( client->buffer, client->buffer_size );
		if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		fprintf(stderr, "Buffer Limit reached realloc new size %ld\n", client->buffer_size );
		nb_to_read = client->buffer_size - (client->ptr - client->buffer );
	}
	len = sizeof (client->from );
	nb = recvfrom (fd, client->ptr, nb_to_read,0,(struct sockaddr *)&client->from,
		       &len);
	if (nb  < 0) {
		perror(" Read Socket ");
		(*channel_close) (client->channel );
		return;
	}
	if (nb == 0 ) {
		(*channel_close) (client->channel );
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
		memcpy (client->buffer, ptr, len  );
		client->ptr = client->buffer + len;
		}
		else
		{
		client->ptr = client->buffer;
		}
}

static void HandleServer(Channel channel, HANDLE fd, void *data)
{
	Server server = (Server ) data;
	Client client;
	HANDLE ns;
	socklen_t addrlen;
	struct sockaddr_in remote2;
#ifdef DEBUG
	printf( "Accepting Connection...\n");
#endif //DEBUG
	addrlen = sizeof (remote2 );
	if ((ns = accept (fd, (struct sockaddr *)&remote2, &addrlen)) <0)
		{
		perror ("*** accept ***");
		return;
		};
#ifdef DEBUG
	printf( "Accepting Connection ret\n");
#endif //DEBUG
	IVY_LIST_ADD (clients_list, client );
	if (!client )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		close (fd );
		exit(0);
		}
	client->buffer_size = BUFFER_SIZE;
	client->buffer = malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->from = remote2;
	client->fd = ns;
	client->channel = (*channel_setup) (ns, client,  DeleteSocket, HandleSocket );
	client->interpretation = server->interpretation;
	client->ptr = client->buffer;
	client->handle_delete = server->handle_delete;
	client->data = (*server->create) (client );
	client->socket_output = fdopen( client->fd, "w" );
	if (!client->socket_output )
		{
			perror("Socket Buffered output fdopen:");
			exit(0);
		} 
}

Server SocketServer(unsigned short port, 
	void*(*create)(Client client),
	void(*handle_delete)(Client client, void *data),
	void(*interpretation) (Client client, void *data, char *ligne))
{
	Server server;
	HANDLE fd;
	int one=1;
	struct sockaddr_in local;
	socklen_t addrlen;
		

	if ((fd = socket (AF_INET, SOCK_STREAM, 0)) < 0){
		perror ("***open socket ***");
		exit(0);
		};

	
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY; 
	local.sin_port = htons (port);

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
	
	if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0)
		{
		perror ("*** bind ***");
		exit(0);
		}

	addrlen = sizeof (local );
	if (getsockname(fd,(struct sockaddr *)&local, &addrlen) < 0)
		{
		perror ("***get socket name ***");
		exit(0);
		} 
	
	if (listen (fd, 128) < 0){
		perror ("*** listen ***");
		exit(0);
		};
	

	IVY_LIST_ADD (servers_list, server );
	if (!server )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		exit(0);
		}
	server->fd = fd;
	server->channel =	(*channel_setup) (fd, server, DeleteServerSocket, HandleServer );
	server->create = create;
	server->handle_delete = handle_delete;
	server->interpretation = interpretation;
	server->port = ntohs(local.sin_port);
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
	(*channel_close) (server->channel );
}

char *SocketGetPeerHost (Client client )
{
	int err;
	struct sockaddr_in name;
	struct hostent *host;
	socklen_t len = sizeof(name);

	if (!client)
		return "undefined";

	err = getpeername (client->fd, (struct sockaddr *)&name, &len );
	if (err < 0 ) return "can't get peer";
	host = gethostbyaddr ((char *)&name.sin_addr.s_addr,sizeof(name.sin_addr.s_addr),name.sin_family);
	if (host == NULL ) return "can't translate addr";
	return host->h_name;
}

struct in_addr * SocketGetRemoteAddr (Client client )
{
	return client ? &client->from.sin_addr : 0;
}

void SocketGetRemoteHost (Client client, char **host, unsigned short *port )
{
	struct hostent *hostp;

	if (!client)
		return;

	/* extract hostname and port from last message received */
	hostp = gethostbyaddr ((char *)&client->from.sin_addr.s_addr,
			sizeof(client->from.sin_addr.s_addr),client->from.sin_family);
	if (hostp == NULL ) *host = "unknown";
		else *host = hostp->h_name;
	*port = ntohs (client->from.sin_port );
}

void SocketClose (Client client )
{
	if (client)
		(*channel_close) (client->channel );
}

void SocketSendRaw (Client client, char *buffer, int len )
{
	int err;

	if (!client)
		return;

	err = send (client->fd, buffer, len, 0 );
	if (err != len )
		perror ("*** send ***");
}

void SocketSetData (Client client, void *data )
{
	if (client)
		client->data = data;
}

void SocketSend (Client client, char *fmt, ... )
{
	static char *buffer = NULL; /* Use satic mem to eliminate multiple call to malloc /free */
	static int size = 0;		/* donc non reentrant !!!! */
	va_list ap;
	int len;
	if (!client)
		return;
	va_start (ap, fmt );
	len = make_message (&buffer,&size, fmt, ap );
	SocketSendRaw (client, buffer, len );
	va_end (ap );
}
void SocketSendBuffered (Client client, char *fmt, ... )
{
	va_list ap;
	if (!client)
		return;
	va_start (ap, fmt );
	vfprintf (client->socket_output, fmt, ap );
	va_end (ap );
}
void SocketFlush ( Client client )
{
	if (!client)
		return;
	fflush( client->socket_output );
}

void *SocketGetData (Client client )
{
	return client ? client->data : 0;
}

void SocketBroadcast ( char *fmt, ... )
{
	Client client;
	static char *buffer = NULL; /* Use satic mem to eliminate multiple call to malloc /free */
	static int size = 0;		/* donc non reentrant !!!! */
	va_list ap;
	int len;
	
	va_start (ap, fmt );
	len = make_message (&buffer, &size, fmt, ap );
	va_end (ap );
	IVY_LIST_EACH (clients_list, client )
		{
		SocketSendRaw (client, buffer, len );
		}
}

/*
Ouverture d'un canal TCP/IP en mode client
*/
Client SocketConnect (char * host, unsigned short port, 
			void *data, 
			SocketInterpretation interpretation,
			void (*handle_delete)(Client client, void *data)
			)
{
	struct hostent *rhost;

	if ((rhost = gethostbyname (host )) == NULL) {
		fprintf(stderr, "Erreur %s Calculateur inconnu !\n",host);
		 return NULL;
	}
	return SocketConnectAddr ((struct in_addr*)(rhost->h_addr), port, data, interpretation, handle_delete);
}

Client SocketConnectAddr (struct in_addr * addr, unsigned short port, 
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

	if ((handle = socket (AF_INET, SOCK_STREAM, 0)) < 0){
		perror ("*** client socket ***");
		return NULL;
	};

	if (connect (handle, (struct sockaddr *)&remote, sizeof(remote) ) < 0){
		perror ("*** client connect ***");
		return NULL;
	};

	IVY_LIST_ADD (clients_list, client );
	if (!client ) {
			fprintf(stderr,"NOK Memory Alloc Error\n");
			close (handle );
			exit(0);
	}
	
	client->buffer_size = BUFFER_SIZE;
	client->buffer = malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->fd = handle;
	client->channel = (*channel_setup) (handle, client,  DeleteSocket, HandleSocket );
	client->interpretation = interpretation;
	client->ptr = client->buffer;
	client->data = data;
	client->handle_delete = handle_delete;
	client->from.sin_family = AF_INET;
	client->from.sin_addr = *addr;
	client->from.sin_port = htons (port);
	client->socket_output = fdopen( client->fd, "w" );
	if (!client->socket_output )
		{
			perror("Socket Buffered output fdopen:");
			exit(0);
		} 
	return client;
}
// TODO factoriser avec HandleRead !!!!
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

Client SocketBroadcastCreate (unsigned short port, 
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

	if ((handle = socket (AF_INET, SOCK_DGRAM, 0)) < 0){
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

	if (bind(handle, (struct sockaddr *)&local, sizeof(local)) < 0)
		{
			perror ("*** test BIND ***");
			return NULL;
		};

	IVY_LIST_ADD(clients_list, client );
	if (!client ) {
		fprintf(stderr,"NOK Memory Alloc Error\n");
		close (handle );
		exit(0);
	}
	
	client->buffer_size = BUFFER_SIZE;
	client->buffer = malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->fd = handle;
	client->channel = (*channel_setup) (handle, client,  DeleteSocket, HandleSocket );
	client->interpretation = interpretation;
	client->ptr = client->buffer;
	client->data = data;
	client->socket_output = fdopen( client->fd, "w" );
	if (!client->socket_output )
		{
			perror("Socket Buffered output fdopen:");
			exit(0);
		} 
	return client;
}

void SocketSendBroadcast (Client client, unsigned long host, unsigned short port, char *fmt, ... )
{
	struct sockaddr_in remote;
	static char *buffer = NULL; /* Use satic mem to eliminate multiple call to malloc /free */
	static int size = 0;		/* donc non reentrant !!!! */
	va_list ap;
	int err,len;

	if (!client)
		return;

	va_start (ap, fmt );
	len = make_message (&buffer, &size, fmt, ap );
	/* Send UDP packet to the dest */
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = htonl (host );
	remote.sin_port = htons(port);
	err = sendto (client->fd, 
			buffer, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if (err != len) {
		perror ("*** send ***");
	}	va_end (ap );
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
	unsigned char ttl = 64 ; // Arbitrary TTL value.
	/* wee need to broadcast */

	imr.imr_multiaddr.s_addr = htonl( host );
	imr.imr_interface.s_addr = INADDR_ANY; 
	if(setsockopt(client->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&imr,sizeof(struct ip_mreq)) == -1 )
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

