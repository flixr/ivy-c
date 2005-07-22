/*
 *
 *	Ivy, C interface
 *
 *	Copyright 1997-2000
 *	Centre d'Etudes de la Navigation Aerienne
 *
 *	Main functions
 *
 *	Authors: Francois-Regis Colin <fcolin@cena.fr>
 *		Stephane Chatty <chatty@cena.fr>
 *
 *	$Id$
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/time.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>


#include <fcntl.h>

#include "ivychannel.h"
#include "ivybind.h"
#include "ivysocket.h"
#include "list.h"
#include "ivy.h"

#define VERSION 4

#define MAX_MSG_FIELDS 200

#define MESSAGE_SEPARATOR '\001' /* TODO remove use Bin args tree */

#define DEFAULT_DOMAIN 127.255.255.255

/* stringification et concatenation du domaine et du port en 2 temps :
 * Obligatoire puisque la substitution de domain, et de bus n'est pas
 * effectuée si on stringifie directement dans la macro GenerateIvyBus */
#define str(bus) #bus
#define GenerateIvyBus(domain,bus) str(domain)":"str(bus)
static char* DefaultIvyBus = GenerateIvyBus(DEFAULT_DOMAIN,DEFAULT_BUS);

/* syntaxe des messages */
#define MSGTYPE 0
#define MSGID	1
#define ARG_0	2

typedef enum {

	Bye,				/* l'application emettrice se termine */
	AddRegexp,			/* expression reguliere d'un client */
	Msg,				/* message reel */
	Error,				/* error message */
	DelRegexp,			/* Remove expression reguliere */
	EndRegexp,			/* end of the regexp list */
	StartRegexp,		/* debut des expressions */
	DirectMsg,			/* message direct a destination de l'appli */
	Die,				/* demande de terminaison de l'appli */
	Ping = 9,			/* checks the presence of the other */
	Pong = 10,			/* checks the presence of the other */
	AddBinding = 11,	/* other methods for binding message based on hash table */
	DelBinding = 12,	/* other methods for binding message based on hash table */
	ApplicationId = 13,	/* on start send my ID and priority */

} MsgType;	

typedef struct _msg_snd *MsgSndPtr;

struct _msg_rcv {			/* requete d'emission d'un client */
	MsgRcvPtr next;
	int id;
	const char *regexp;		/* regexp du message a recevoir */
	MsgCallback callback;		/* callback a declanche a la reception */
	void *user_data;		/* stokage d'info client */
};

struct _msg_snd {			/* requete de reception d'un client */
	MsgSndPtr next;
	int id;
	char *str_regexp;		/* la regexp sous forme inhumaine */
	IvyBinding bind;
};

struct _clnt_lst {
	IvyClientPtr next;
	Client client;			/* la socket  client */
	MsgSndPtr msg_send;		/* liste des requetes recues */
	char *app_name;			/* nom de l'application */
	char *app_id;			/* identificateur unique de l'application (time-ip-port) */ 
	int priority;			/* client priority */
	unsigned short app_port;	/* port de l'application */
};

/* server  pour la socket application */
static Server server;

/* numero de port TCP en mode serveur */
static unsigned short ApplicationPort;

/* numero de port UDP */
static unsigned short SupervisionPort;

/* client pour la socket supervision */
static Client broadcast;

static const char *ApplicationName = 0;

/* classes de messages emis par l'application utilise pour le filtrage */
static int	messages_classes_count = 0;
static const char **messages_classes = 0;

/* callback appele sur reception d'un message direct */
static MsgDirectCallback direct_callback = 0;
static  void *direct_user_data = 0;

/* callback appele sur changement d'etat d'application */
static IvyApplicationCallback application_callback = 0;
static void *application_user_data = 0;

/* callback appele sur ajout suppression de regexp */
static IvyBindCallback application_bind_callback = 0;
static void *application_bind_data = 0;

/* callback appele sur demande de terminaison d'application */
static IvyDieCallback application_die_callback;
static void *application_die_user_data = 0;

/* liste des messages a recevoir */
static MsgRcvPtr msg_recv = 0;

/* liste des clients connectes */
static IvyClientPtr clients = 0;

static const char *ready_message = 0;

static char *applicationUniqueId = 0;
static int applicationPriority = DEFAULT_PRIORITY;

/* get Current Time in milliseconds */
static long currentTime()
{
        unsigned long current;
#ifdef WIN32
        current = GetTickCount();
#else
        struct timeval stamp;
        gettimeofday( &stamp, NULL );
        current = stamp.tv_sec * 1000 + stamp.tv_usec/1000;
#endif
        return  current;
}

/*
 * function split string in multiple string using separator
 * empty args when consecutives separators
 * the input string is modified separator are replaced with \0
 * */
static int SplitArg( char *s, const char separator, char **argv )
{
	char *ptr = s;
	int argc = 0;
	while ( *ptr ) 
	{
		argv[argc++] = ptr;
		while( *ptr && *ptr != separator )
				ptr++;
		if ( *ptr == separator )
		{
		*ptr++ = '\0';
		if ( !*ptr ) /* check last arg empty */
			argv[argc++] = ptr;
		}
	}
	return argc;
}
/* returns < 0 if *p sorts lower than *q */
static int keycmp (IvyClientPtr p,  IvyClientPtr q)
{
      return p->priority - q->priority;
}

/* merge 2 lists under dummy head item */
static IvyClientPtr lmerge (IvyClientPtr p, IvyClientPtr q)
{
      IvyClientPtr r;
	   struct _clnt_lst head;

      for ( r = &head; p && q; )
      {
            if ( keycmp(p, q) < 0 )
            {
                  r = r->next = p;
                  p = p->next;
            }
            else
            {
                  r = r->next = q;
                  q = q->next;
            }
      }
      r->next = (p ? p : q);
      return head.next;
}

/* split list into 2 parts, sort each recursively, merge */
static IvyClientPtr lsort (IvyClientPtr p)
{
      IvyClientPtr q, r;

      if ( p )
      {
            q = p;
            for ( r = q->next; r && (r = r->next) != NULL; r = r->next )
                  q = q->next;
            r = q->next;
            q->next = NULL;
            if ( r )
                  p = lmerge(lsort(r), lsort(p));
      }
      return p;
}


static void SortClients()
{
	//TODO sort client list again priority!
	lsort( clients );
}
static void MsgSendTo( Client client, MsgType msgtype, int id, const char *message )
{
	char * packet;
	char * ptr;
	int len;
	int len_arg;
	len_arg = strlen( message );
	len = 3 * sizeof ( ushort  ) + len_arg;
	packet = malloc ( len ); /* TODO free packet */
	
#ifdef DEBUG
	printf( "Sending message type=%d id=%d '%s'\n",msgtype,id,message);
#endif
	ptr = packet;
	*((ushort *) ptr)++ = htons( (ushort)msgtype );
	*((ushort *) ptr)++ = htons( (ushort)id );
	*((ushort *) ptr)++ = htons( (ushort)len_arg );
	if ( len_arg )
	{
	strncpy( ptr, message , len_arg);
	}
	SocketSendBuf( client, packet, len ); //TODO dont do multiple buffer copy
	SocketFlush( client );
	free( packet );
}

static void IvyCleanup()
{
	IvyClientPtr clnt,next;

	/* destruction des connexions clients */
	IVY_LIST_EACH_SAFE( clients, clnt, next )
	{
		/* on dit au revoir */
		MsgSendTo( clnt->client, Bye, 0, "" );
		SocketClose( clnt->client );
		IVY_LIST_EMPTY( clnt->msg_send );
	}
	IVY_LIST_EMPTY( clients );

	/* destruction des sockets serveur et supervision */
	SocketServerClose( server );
	SocketClose( broadcast );
}

static int MsgCall (const char *message, MsgSndPtr msg,  Client client)
{
	static char *buffer = NULL; /* Use satic mem to eliminate multiple call to malloc /free */
	static int size = 0;		/* donc non reentrant !!!! */
	int offset = 0;
  	int arglen;
	const char *arg;
	int index;

	int rc = IvyBindExec( msg->bind, message );
	
	if (rc<1) return 0; /* no match */
#ifdef DEBUG
	printf( "Sending message id=%d '%s'\n",msg->id,message);
#endif
	// il faut essayer d'envoyer le message en une seule fois sur la socket
	// pour eviter au maximun de passer dans le select plusieur fois par message du protocole Ivy
	// pour eviter la latence ( PB de perfo detecte par ivyperf ping roudtrip )

#ifdef DEBUG
	printf( "Send matching args count %d\n",rc-1);
#endif
	index=0;
	while ( index<rc ) {

		IvyBindingGetMatch( msg->bind, message, index,  &arg, &arglen );
#ifdef DEBUG
		printf ("Send matching arg%d '%.*s'\n",index,arglen, arg);
#endif
		offset += make_message_var( &buffer, &size, offset, "%.*s%c",
				arglen, arg,
				MESSAGE_SEPARATOR
				);
		++index;
	}
	buffer[offset-1] = '\0';
	MsgSendTo( client, Msg, msg->id, buffer );
	return 1;
}


static int ClientCall (IvyClientPtr clnt, const char *message)
{
	MsgSndPtr msg;
	int match_count = 0;
	/* recherche dans la liste des requetes recues de ce client */
	IVY_LIST_EACH (clnt->msg_send, msg) {
		match_count+= MsgCall (message, msg, clnt->client);
	}
	return match_count;
}

static int CheckRegexp(char *exp)
{
	/* accepte tout par default */
	int i;
	int regexp_ok = 1;
	if ( *exp =='^' && messages_classes_count !=0 )
	{
		regexp_ok = 0;
		for ( i = 0 ; i < messages_classes_count; i++ )
		{
			if (strncmp( messages_classes[i], exp+1, strlen( messages_classes[i] )) == 0)
				return 1;
		}
 	}
	return regexp_ok;
}

static int CheckConnected( IvyClientPtr clnt )
{
	IvyClientPtr client;
	struct in_addr *addr1;
	struct in_addr *addr2;

	if ( clnt->app_port == 0 ) /* Old Ivy Protocol Dont check */
		return 0;
	/* recherche dans la liste des clients de la presence de clnt */
	IVY_LIST_EACH( clients, client )
	{
		/* client different mais port identique */
		if ( (client != clnt) && (clnt->app_port == client->app_port) )
			{
			/* et meme machine */
			addr1 = SocketGetRemoteAddr( client->client );
			addr2 = SocketGetRemoteAddr( clnt->client );
			if ( addr1->s_addr == addr2->s_addr )
				return 1;
			}
			
	}
	return 0;
}

static char* Receive( Client client, void *data, char *message, unsigned int len )
{
	IvyClientPtr clnt;
	int id;
	ushort len_args;
	MsgSndPtr snd;
	MsgRcvPtr rcv;
	int argc = 0;
	char *argv[MAX_MSG_FIELDS];
	int kind_of_msg = Bye;
	IvyBinding bind;
	char *ptr_end;
	char *args;

	ptr_end = message;

	if ( len < 6 ) return NULL;  /* incomplete message */
	kind_of_msg = ntohs( *((ushort *) ptr_end)++ );
	id = ntohs( *((ushort *) ptr_end)++ ); 
	len_args = ntohs( *((ushort *) ptr_end)++ );

	if ( len_args )
	{
	if ( len < (6 + len_args) ) return NULL; /* incomplete message */
	args = malloc( len_args ); /* TODO keep the buffer to free it */
	strncpy( args , ptr_end, len_args );
	args[ len_args ] = '\0';
	ptr_end += len_args;
	}

#ifdef DEBUG
	printf("Receive Message type=%d id=%d arg=%s\n",  kind_of_msg, id, args);
#endif //DEBUG

	clnt = (IvyClientPtr)data;
	switch( kind_of_msg )
		{
		case Bye:
			
#ifdef DEBUG
			printf("Quitting  Bye %s\n",  args);
#endif //DEBUG

			SocketClose( client );
			break;
		case Error:
			printf ("Received error %d %s\n",  id, args);
			break;
		case AddRegexp:

#ifdef DEBUG
			printf("Regexp  id=%d exp='%s'\n",  id, args);
#endif //DEBUG
			if ( !CheckRegexp( args ) )
				{
#ifdef DEBUG
				printf("Warning: regexp '%s' illegal, removing from %s\n",args,ApplicationName);
#endif //DEBUG
				return ptr_end;
				}
			bind = IvyBindingCompile( args );
			if ( bind != NULL )
				{
				IVY_LIST_ADD( clnt->msg_send, snd )
				if ( snd )
					{
					snd->id = id;
					snd->str_regexp = strdup( args );
					snd->bind = bind;	
					if ( application_bind_callback )
					  {
					    (*application_bind_callback)( clnt, application_bind_data, IvyAddBind, snd->str_regexp );
					  }
					}
				}
			else
			{
			int offset;
			const char *errbuf;
			IvyBindingGetCompileError( &offset, &errbuf );
			MsgSendTo( client, Error, offset, errbuf );
			}

			break;
		case DelRegexp:
#ifdef DEBUG
			printf("Regexp Delete id=%d\n",  id);
#endif //DEBUG

			IVY_LIST_ITER( clnt->msg_send, snd, ( id != snd->id ));
			if ( snd )
				{
				  if ( application_bind_callback )
				    {
				      (*application_bind_callback)( clnt, application_bind_data,  IvyRemoveBind, snd->str_regexp );
				    }
				free( snd->str_regexp );
				IvyBindingFree( snd->bind );

				IVY_LIST_REMOVE( clnt->msg_send, snd  );
				}
			break;
		case StartRegexp:
#ifdef DEBUG
			printf("Regexp Start id=%d Application='%s'\n",  id, args);
#endif //DEBUG
			clnt->app_name = strdup( args );
			clnt->app_port = id;
			if ( CheckConnected( clnt ) )
			{			
#ifdef DEBUG
			printf("Quitting  already connected %s\n",  args);
#endif //DEBUG
			IvySendError( clnt, 0, "Application already connected" );
			SocketClose( client );
			}
			break;
		case EndRegexp:
			
#ifdef DEBUG
			printf("Regexp End id=%d\n",  id);
#endif //DEBUG
			if ( application_callback )
				{
				(*application_callback)( clnt, application_user_data, IvyApplicationConnected );
				}
			if ( ready_message )
				{
				int count;
				count = ClientCall( clnt, ready_message );
				
#ifdef DEBUG
				printf(" Sendind ready message %d\n", count);
#endif //DEBUG

				}
			break;
		case Msg:
			
#ifdef DEBUG
		printf("Message id=%d msg='%s'\n", id, args);
#endif //DEBUG

			argc = SplitArg( args, MESSAGE_SEPARATOR, argv);	
	
			IVY_LIST_EACH( msg_recv, rcv )
				{
				if ( id == rcv->id )
					{
#ifdef DEBUG
					printf("Calling  id=%d argc=%d for %s\n", id, argc,rcv->regexp);
#endif
					if ( rcv->callback ) (*rcv->callback)( clnt, rcv->user_data, argc, argv );
					return ptr_end;
					}
				}
			printf("Callback Message id=%d not found!!!'\n", id);
			break;
		case DirectMsg:
			
#ifdef DEBUG
			printf("Direct Message id=%d msg='%s'\n", id, args);
#endif //DEBUG

			if ( direct_callback)
				(*direct_callback)( clnt, direct_user_data, id, args );
			break;

		case Die:
			
#ifdef DEBUG
			printf("Die Message\n");
#endif //DEBUG

			if ( application_die_callback)
				(*application_die_callback)( clnt, application_die_user_data, id );
			IvyCleanup();
			exit(0);
			break;
		case ApplicationId:
#ifdef DEBUG
			printf("ApplicationId  priority=%d appid='%s'\n",  id, args);
#endif //DEBUG
			clnt->app_id = strdup( args );
			if ( id != clnt->priority )
			{
			clnt->priority = id;
			SortClients();
			}
			break;
		default:
			printf("Receive unhandled message %d\n",  kind_of_msg);
			break;
		}
	return ptr_end;
}

static IvyClientPtr SendService( Client client )
{
	IvyClientPtr clnt;
	MsgRcvPtr msg;
	IVY_LIST_ADD( clients, clnt )
	if ( clnt )
		{
		SocketKeepAlive ( client, 1 );
		clnt->msg_send = 0;
		clnt->client = client;
		clnt->app_name = strdup("Unknown");
		clnt->app_port = 0;
		clnt->priority = DEFAULT_PRIORITY;
		MsgSendTo( client, ApplicationId, applicationPriority, applicationUniqueId );
		MsgSendTo( client, StartRegexp, ApplicationPort, ApplicationName);
		IVY_LIST_EACH(msg_recv, msg )
			{
			MsgSendTo( client, AddRegexp,msg->id,msg->regexp);
			}
		MsgSendTo( client, EndRegexp, 0, "");
		}
	return clnt;
}

static void ClientDelete( Client client, void *data )
{
	IvyClientPtr clnt;
	MsgSndPtr msg;
#ifdef DEBUG
	char *remotehost;
	unsigned short remoteport;
#endif
	clnt = (IvyClientPtr)data;
	if ( application_callback )
				{
				(*application_callback)( clnt, application_user_data, IvyApplicationDisconnected );
				}
	
#ifdef DEBUG
	/* probably bogus call, but this is for debug only anyway */
	SocketGetRemoteHost( client, &remotehost, &remoteport );
	printf("Deconnexion de %s:%hu\n", remotehost, remoteport );
#endif //DEBUG

	if ( clnt->app_name ) free( clnt->app_name );
	IVY_LIST_EACH( clnt->msg_send, msg)
	{
		/*regfree(msg->regexp);*/
		free( msg->str_regexp);
	}
	IVY_LIST_EMPTY( clnt->msg_send );
	IVY_LIST_REMOVE( clients, clnt );
}

static void *ClientCreate( Client client )
{

#ifdef DEBUG
	char *remotehost;
	unsigned short remoteport;
	SocketGetRemoteHost( client, &remotehost, &remoteport );
	printf("Connexion de %s:%hu\n", remotehost, remoteport );
#endif //DEBUG
	return SendService (client);
}
/* Hello packet Send */
static void IvySendHello(unsigned long mask)
{
	char *packet;
	char *ptr;
	int lenAppId;
	int lenAppName;
	int len;
	lenAppId = strlen( applicationUniqueId );
	lenAppName = strlen( ApplicationName );
	len = 4*sizeof(ushort) +  lenAppId + lenAppName;
	packet = malloc( len );
	ptr = packet;
	*((ushort *) ptr)++ = htons( VERSION );
	*((ushort *) ptr)++ = htons( ApplicationPort );
	*((ushort *) ptr)++ = htons( lenAppId );
	strncpy( ptr, applicationUniqueId , lenAppId);
	ptr += lenAppId;
	*((ushort *) ptr)++ = htons( lenAppName );
	strncpy( ptr, ApplicationName , lenAppName);
	
	SocketSendBroadcastRaw (broadcast, mask, SupervisionPort, packet,len ); 
	free( packet );
}
/* Hello packet Receive */
static char* BroadcastReceive( Client client, void *data, char *message, unsigned int len)
{	
	Client app;
	unsigned short version;
	unsigned short serviceport;
	char appname[1024];
	char appid[1024];
	unsigned short len_appid;
	unsigned short len_appname;
#ifdef DEBUG
	unsigned short remoteport;
	char *remotehost = 0;
#endif

	char *ptr_end;
	ptr_end = message;
	
	if ( len < 6 ) return NULL;  /* incomplete message */
	
	version = ntohs( *((ushort *) ptr_end)++ );
	serviceport = ntohs( *((ushort *) ptr_end)++ ); 
	len_appid = ntohs( *((ushort *) ptr_end)++ );
	if ( len < (6 +len_appid) ) return NULL;  /* incomplete message */
	
	strncpy( appid , ptr_end, len_appid );
	appid[ len_appid ] = '\0';
	ptr_end += len_appid;
	len_appname = ntohs( *((ushort *) ptr_end)++ );
	if ( len < (6 +len_appid + len_appname) ) return NULL;  /* incomplete message */
	
	strncpy( appname , ptr_end, len_appname );
	appname[ len_appname ] = '\0';
	ptr_end += len_appname;

	if ( version != VERSION ) {
		/* ignore the message */
		unsigned short remoteport;
		char *remotehost = 0;
		SocketGetRemoteHost (client, &remotehost, &remoteport );
		fprintf (stderr, "Bad Ivy version, expected %d and got %d from %s:%d\n",
			VERSION, version, remotehost, remoteport);
		return ptr_end;
	}
	/* check if we received our own message */
	if (strcmp( appid,applicationUniqueId )== 0) 
		return ptr_end;
	
#ifdef DEBUG
	SocketGetRemoteHost (client, &remotehost, &remoteport );
	printf(" Broadcast de %s:%hu port %hu %s %s\n", remotehost, remoteport, serviceport, appname, appid );
#endif //DEBUG

	/* connect to the service and send the regexp */
	app = SocketConnectAddr(SocketGetRemoteAddr(client), serviceport, 0, Receive, ClientDelete );
	if (app) {
		IvyClientPtr clnt;
		clnt = SendService( app );
		SocketSetData( app, clnt);
	}
	return ptr_end;
}

void IvyInit (const char *appname, const char *ready, 
			 IvyApplicationCallback callback, void *data,
			 IvyDieCallback die_callback, void *die_data
			 )
{
	IvyChannelInit();

	ApplicationName = appname;
	application_callback = callback;
	application_user_data = data;
	application_die_callback = die_callback;
	application_die_user_data = die_data;
	ready_message = ready;
}

void IvyStop()
{
	IvyChannelStop();
}

void IvySetApplicationPriority( int priority )
{
	IvyClientPtr clnt;
	applicationPriority = priority;
	/* Send to already connected clients */
	IVY_LIST_EACH (clients, clnt ) {
		MsgSendTo( clnt->client, ApplicationId, applicationPriority, applicationUniqueId);
	}
}

void IvySetBindCallback(IvyBindCallback bind_callback, void *bind_data)
{
  application_bind_callback=bind_callback;
  application_bind_data=bind_data;
}

void IvyClasses( int argc, const char **argv)
{
	messages_classes_count = argc;
	messages_classes = argv;
}

void IvyStart (const char* bus)
{

	struct in_addr baddr;
	unsigned long mask = 0xffffffff; 
	unsigned char elem = 0;
	int numdigit = 0;
	int numelem = 0;
	int error = 0;
	const char* p = bus;	/* used for decoding address list */
	const char* q;		/* used for decoding port number */
	int port;

	
	/*
	 * Initialize TCP port
	 */
	server = SocketServer (ANYPORT, ClientCreate, ClientDelete, Receive);
	ApplicationPort = SocketServerGetPort (server);

	/*
	 * Find network list as well as broadcast port
	 * (we accept things like 123.231,123.123:2000 or 123.231 or :2000),
	 * Initialize UDP port
	 * Send a broadcast handshake on every network
	 */

	/* first, let's find something to parse */
	if (!p || !*p)
		p = getenv ("IVYBUS");
	if (!p || !*p) 
		p = DefaultIvyBus;

	/* then, let's get a port number */
	q = strchr (p, ':');
	if (q && (port = atoi (q+1)))
		SupervisionPort = port;
	else
		SupervisionPort = DEFAULT_BUS;

	/* generate application UniqueID (timeStamp-Ipaddress-port*/
	applicationUniqueId = malloc(1024);
	sprintf( applicationUniqueId , "%lu-%s-%d", currentTime(),"123456" , ApplicationPort);

	/*
	 * Now we have a port number it's time to initialize the UDP port
	 */
	broadcast =  SocketBroadcastCreate (SupervisionPort, 0, BroadcastReceive );

		
	/* then, if we only have a port number, resort to default value for network */
	if (p == q)
		p = DefaultIvyBus;

	/* and finally, parse network list and send broadcast handshakes.
	   This is painful but inet_aton is sloppy.
	   If someone knows other builtin routines that do that... */
	for (;;) {
		/* address elements are up to 3 digits... */
		if (!error && isdigit (*p)) {
			if (numdigit < 3 && numelem < 4) {
				elem = 10 * elem +  *p -'0';
			} else {
				error = 1;
			}

		/* ... terminated by a point, a comma or a colon, or the end of string */
		} else if (!error && (*p == '.' || *p == ',' || *p == ':' || *p == '\0')) {
			mask = (mask ^ (0xff << (8*(3-numelem)))) | (elem << (8*(3-numelem)));

			/* after a point, expect next address element */
			if (*p == '.') {
				numelem++;

			/* addresses are terminated by a comma or end of string */
			} else {

				baddr.s_addr = htonl(mask);
				printf ("Broadcasting on network %s, port %d\n", inet_ntoa(baddr), SupervisionPort);
				// test mask value agaisnt CLASS D
				if ( IN_MULTICAST( mask ) )
					SocketAddMember (broadcast , mask );

				IvySendHello( mask );
				numelem = 0;
				mask = 0xffffffff;
			}
			numdigit = 0;
			elem = 0;

		/* recover from bad addresses at next comma or colon or at end of string */
		} else if (*p == ',' || *p == ':' || *p == '\0') {
			fprintf (stderr, "bad broadcast address\n");
			elem = 0;
			numelem = 0;
			numdigit = 0;
			mask = 0xffffffff;
			error = 0;

		/* ignore spaces */
		} else if (*p == ' ') {

		  /* everything else is illegal */
		} else {
			error = 1;
		}

		/* end of string or colon */
		if (*p == '\0' || *p == ':')
			break;
		++p;
	}

#ifdef DEBUG
	fprintf (stderr,"Listening on TCP:%hu\n",ApplicationPort);
#endif

}

/* desabonnements */
void
IvyUnbindMsg (MsgRcvPtr msg)
{
	IvyClientPtr clnt;
	/* Send to already connected clients */
	IVY_LIST_EACH (clients, clnt ) {
		MsgSendTo( clnt->client, DelRegexp,msg->id, "");
	}
	IVY_LIST_REMOVE( msg_recv, msg  );
}

/* demande de reception d'un message */

MsgRcvPtr
IvyBindMsg (MsgCallback callback, void *user_data, const char *fmt_regex, ... )
{
	static char *buffer = NULL;
	static int size = 0;
	va_list ap;
	static int recv_id = 0;
	IvyClientPtr clnt;
	MsgRcvPtr msg;

	va_start (ap, fmt_regex );
	make_message( &buffer, &size, 0, fmt_regex, ap );
	va_end  (ap );

	/* add Msg to the query list */
	IVY_LIST_ADD( msg_recv, msg );
	if (msg)	{
		msg->id = recv_id++;
		msg->regexp = strdup(buffer);
		msg->callback = callback;
		msg->user_data = user_data;
	}
	/* Send to already connected clients */
	/* recherche dans la liste des requetes recues de mes clients */
	IVY_LIST_EACH( clients, clnt ) {
		MsgSendTo( clnt->client, AddRegexp,msg->id,msg->regexp);
	}
	return msg;
}

int IvySendMsg(const char *fmt, ...)
{
	IvyClientPtr clnt;
	int match_count = 0;
	static char *buffer = NULL; /* Use satic mem to eliminate multiple call to malloc /free */
	static int size = 0;		/* donc non reentrant !!!! */
	va_list ap;
	
	va_start( ap, fmt );
	make_message( &buffer, &size, 0, fmt, ap );
	va_end ( ap );

	/* recherche dans la liste des requetes recues de mes clients */
	IVY_LIST_EACH (clients, clnt) {
		match_count += ClientCall (clnt, buffer);
	}
#ifdef DEBUG
	if ( match_count == 0 ) printf( "Warning no recipient for %s\n",buffer);
#endif
	return match_count;
}

void IvySendError( IvyClientPtr app, int id, const char *fmt, ... )
{
	static char *buffer = NULL; /* Use satic mem to eliminate multiple call to malloc /free */
	static int size = 0;		/* donc non reentrant !!!! */
	va_list ap;
	
	va_start( ap, fmt );
	make_message( &buffer, &size, 0, fmt, ap );
	va_end ( ap );
	MsgSendTo( app->client, Error, id, buffer);
}

void IvyBindDirectMsg( MsgDirectCallback callback, void *user_data)
{
	direct_callback = callback;
	direct_user_data = user_data;
}

void IvySendDirectMsg( IvyClientPtr app, int id, char *msg )
{
	MsgSendTo( app->client, DirectMsg, id, msg);
}

void IvySendDieMsg( IvyClientPtr app )
{
	MsgSendTo( app->client, Die, 0, "" );
}

char *IvyGetApplicationName( IvyClientPtr app )
{
	if ( app && app->app_name ) 
		return app->app_name;
	else return "Unknown";
}

char *IvyGetApplicationHost( IvyClientPtr app )
{
	if ( app && app->client ) 
		return SocketGetPeerHost (app->client );
	else return 0;
}
char *IvyGetApplicationId( IvyClientPtr app )
{
	if ( app && app->app_id ) 
		return app->app_id;
	else return 0;
}
void IvyDefaultApplicationCallback( IvyClientPtr app, void *user_data, IvyApplicationEvent event)
{
	switch ( event )  {
	case IvyApplicationConnected:
		printf("Application: %s ready on %s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app));
		break;
	case IvyApplicationDisconnected:
		printf("Application: %s bye on %s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app));
		break;
	default:
		printf("Application: %s unkown event %d\n",IvyGetApplicationName( app ), event);
		break;
	}
}
void IvyDefaultBindCallback( IvyClientPtr app, void *user_data, IvyBindEvent event, char* regexp )
{
	switch ( event )  {
	case IvyAddBind:
		printf("Application: %s on %s add regexp %s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app), regexp);
		break;
	case IvyRemoveBind:
		printf("Application: %s on %s remove regexp %s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app), regexp);
		break;
	default:
		printf("Application: %s unkown event %d\n",IvyGetApplicationName( app ), event);
		break;
	}
}

IvyClientPtr IvyGetApplication( char *name )
{
	IvyClientPtr app = 0;
	IVY_LIST_ITER( clients, app, strcmp(name, app->app_name) != 0 );
	return app;
}

char *IvyGetApplicationList()
{
	static char applist[4096];
	IvyClientPtr app;
	applist[0] = '\0';
	IVY_LIST_EACH( clients, app )
		{
		strcat( applist, app->app_name );
		strcat( applist, " " );
		}
	return applist;
}

char **IvyGetApplicationMessages( IvyClientPtr app )
{
	static char *messagelist[200];
	MsgSndPtr msg;
	int msgCount= 0;
	memset( messagelist, 0 , sizeof( messagelist ));
	/* recherche dans la liste des requetes recues de ce client */
	IVY_LIST_EACH( app->msg_send, msg )
	{
	messagelist[msgCount++]= msg->str_regexp;
	}
	return messagelist;
}
