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

#define BUFFER_SIZE 4096	/* taille buffer initiale on multiple pas deux a chaque realloc */

struct _server {
	Server next;
	HANDLE fd;
	Channel channel;
	unsigned short port;
	SocketCreate create;
	SocketDelete handle_delete;
	SocketInterpretation interpretation;
};

struct _client {
	Client next;
	HANDLE fd;
	Channel channel;
	unsigned short port;
	struct sockaddr_in from;
	SocketInterpretation interpretation;
	SocketDelete handle_delete;
	/* input buffer */
	int in_buffer_size;
	char *in_buffer;		/* dynamicaly reallocated */
	char *in_ptr;
	/* output buffer */
	int out_buffer_size;
	char *out_buffer;		/* dynamicaly reallocated */
	char *out_ptr;
	
	void *data;
};

static Server servers_list = NULL;
static Client clients_list = NULL;

#ifdef WIN32
WSADATA	WsaData;
#endif

// fonction de formtage a la printf d'un buffer avec reallocation dynamique  
int make_message(char ** buffer, int *size,  int offset, const char *fmt, va_list ap)
{
    /* Guess we need no more than BUFFER_INIT_SIZE bytes. */
    long n;
	if ( *size == 0 || *buffer == NULL )
		{
		*size = BUFFER_SIZE;
		*buffer = malloc (BUFFER_SIZE);
		if ( *buffer == NULL )
		    return -1;
		}
    while (1) {
    /* Try to print in the allocated space. */
#ifdef WIN32
	n = _vsnprintf (*buffer + offset, *size - offset, fmt, ap);
#else
    n = vsnprintf (*buffer + offset, *size - offset, fmt, ap);
#endif
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
int make_message_var(char ** buffer, int *size,  int offset, const char *fmt, ... )
{
	va_list ap;
	long len;
	va_start (ap, fmt );
	len = make_message (buffer,size, offset, fmt, ap );
	va_end (ap );
	return len;
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
	char *ptr_end;
	long nb_to_read = 0;
	long nb;
	socklen_t len;
	
	/* limitation taille buffer */
	nb_to_read = client->in_buffer_size - (client->in_ptr - client->in_buffer );
	if (nb_to_read == 0 ) {
		client->in_buffer_size *= 2; /* twice old size */
		client->in_buffer = realloc( client->in_buffer, client->in_buffer_size );
		if (!client->in_buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		fprintf(stderr, "Buffer Limit reached realloc new size %d\n", client->in_buffer_size );
		nb_to_read = client->in_buffer_size - (client->in_ptr - client->in_buffer );
	}
	len = sizeof (client->from );
	nb = recvfrom (fd, client->in_ptr, nb_to_read,0,(struct sockaddr *)&client->from,
		       &len);
	if (nb  < 0) {
		perror(" Read Socket ");
		IvyChannelClose(client->channel );
		return;
	}
	if (nb == 0 ) {
		IvyChannelClose(client->channel );
		return;
	}
	client->in_ptr += nb;
	if (! client->interpretation )
	{
		client->in_ptr = client->in_buffer;
		fprintf (stderr,"Socket No interpretation function ??? skipping data\n");
		return;
	}
	ptr = client->in_buffer;
	
	while ( (client->in_ptr > ptr )&&(ptr_end = (*client->interpretation) (client, client->data, ptr, client->in_ptr - ptr )))
		{
		ptr = ptr_end;
		}
	if (ptr < client->in_ptr )
		{ /* recopie message incomplet au debut du buffer */
		len = client->in_ptr - ptr;
		memcpy (client->in_buffer, ptr, len  );
		client->in_ptr = client->in_buffer + len;
		}
		else
		{
		client->in_ptr = client->in_buffer;
		}
}
static Client CreateClient(int handle)
{
	Client client;
	IVY_LIST_ADD (clients_list, client );
	if (!client )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		close ( handle );
		exit(0);
		}
	client->in_buffer_size = BUFFER_SIZE;
	client->in_buffer = malloc( client->in_buffer_size );
	if (!client->in_buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
	client->in_ptr = client->in_buffer;
	client->out_buffer_size = BUFFER_SIZE;
	client->out_buffer = malloc( client->out_buffer_size );
	if (!client->in_buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
	client->out_ptr = client->out_buffer;
	client->fd = handle;
	client->channel = IvyChannelOpen (client->fd, client,  DeleteSocket, HandleSocket );

	return client;
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
	client = CreateClient(ns);
	client->from = remote2;
	client->interpretation = server->interpretation;
	client->handle_delete = server->handle_delete;
	client->data = (*server->create) (client );
}


Server SocketServer(unsigned short port, 
	SocketCreate create,
	SocketDelete handle_delete,
	SocketInterpretation interpretation )
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
	server->channel = IvyChannelOpen(fd, server, DeleteServerSocket, HandleServer );
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
	IvyChannelClose (server->channel );
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
		IvyChannelClose (client->channel );
}
void SocketSetData (Client client, void *data )
{
	if (client)
		client->data = data;
}

void *SocketGetData (Client client )
{
	return client ? client->data : 0;
}
void SocketSendBuf (Client client, char *buffer, int len )
{
	unsigned long usedspace;
	if (!client)
		return;
	usedspace = client->out_ptr - client->out_buffer;
	if ( len >= client->out_buffer_size - usedspace )
	{
		/* not enought space */
		client->out_buffer_size += len - usedspace +1;
		client->out_buffer = realloc (client->out_buffer, client->out_buffer_size);
	}
	memcpy ( client->out_ptr, buffer, len );
	client->out_ptr += len;
}


void SocketSendFmt (Client client, char *fmt, ... )
{
	va_list ap;
	if (!client)
		return;
	va_start (ap, fmt );
	client->out_ptr += make_message (&client->out_buffer, &client->out_buffer_size, client->out_ptr - client->out_buffer, fmt, ap );
	va_end (ap );
}
void SocketFlush (Client client)
{
	int err;
	unsigned long len;

	if (!client)
		return;
	len =  client->out_ptr - client->out_buffer;
	err = send (client->fd, client->out_buffer, len, 0 );
	if (err != len )
		perror ("*** send ***");
	client->out_ptr = client->out_buffer;
}

/*
Ouverture d'un canal TCP/IP en mode client
*/
Client SocketConnect (char * host, unsigned short port, 
			void *data, 
			SocketInterpretation interpretation,
			SocketDelete handle_delete
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
			  SocketDelete handle_delete
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

	client = CreateClient(handle);
	client->interpretation = interpretation;
	client->handle_delete = handle_delete;
	client->data = data;
	client->from.sin_family = AF_INET;
	client->from.sin_addr = *addr;
	client->from.sin_port = htons (port);
	return client;
}

/* Socket UDP */

Client SocketBroadcastCreate (
				unsigned short port, 
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

	client = CreateClient(handle);
	client->interpretation = interpretation;
	client->data = data;
	return client;
}

void SocketSendBroadcast (Client client, unsigned long host, unsigned short port, char *fmt, ... )
{
	va_list ap;
	int len;

	if (!client)
		return;

	va_start (ap, fmt );
	len = make_message (&client->out_buffer, &client->out_buffer_size, 0, fmt, ap );
	SocketSendBroadcastRaw( client, host, port, client->out_buffer, len );
	va_end (ap );
}
void SocketSendBroadcastRaw (Client client, unsigned long host, unsigned short port, char *buffer, int len )
{
	struct sockaddr_in remote;
	int err;

	if (!client)
		return;

	/* Send UDP packet to the dest */
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = htonl (host );
	remote.sin_port = htons(port);
	err = sendto (client->fd, 
			buffer, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if (err != len) {
		perror ("*** send ***");
	}
}
void SocketKeepAlive( Client client,int keepalive )
{
	int alive = keepalive;
	if (setsockopt(client->fd,SOL_SOCKET,SO_KEEPALIVE,(char*)&alive,sizeof(alive)) < 0)
		{
#ifdef WIN32
		fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
		perror ("*** set socket option SO_KEEPALIVE ***");
		exit(0);
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

