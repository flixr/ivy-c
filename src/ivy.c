/*
 *
 *	Ivy, C interface
 *
 *	Copyright 1997-1999 
 *	Centre d'Etudes de la Navigation Aerienne
 *
 *	Main functions
 *
 *	Authors: Francois-Regis Colin <fcolin@cenatoulouse.dgac.fr>
 *		Stephane Chatty <chatty@cenatoulouse.dgac.fr>
 *
 *	$Id$
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <regex.h>

#include <fcntl.h>

#include "ivychannel.h"
#include "ivysocket.h"
#include "list.h"
#include "ivy.h"

#define VERSION 3

#define MAX_MATCHING_ARGS 20

#define ARG_START "\002"
#define ARG_END "\003"

#define DEFAULT_DOMAIN 127.255.255.255

/* stringification et concatenation du domaine et du port en 2 temps :
 * Obligatoire puisque la substitution de domain, et de bus n'est pas
 * effectuée si on stringifie directement dans la macro GenerateIvyBus */
#define str(bus) #bus
#define GenerateIvyBus(domain,bus) str(domain)##":"str(bus)
static char* DefaultIvyBus = GenerateIvyBus(DEFAULT_DOMAIN,DEFAULT_BUS);

typedef enum {

	Bye,			/* l'application emettrice se termine */
	AddRegexp,		/* expression reguliere d'un client */
	Msg,			/* message reel */
	Error,			/* error message */
	DelRegexp,		/* Remove expression reguliere */
	EndRegexp,		/* end of the regexp list */
	StartRegexp,		/* debut des expressions */
	DirectMsg,		/* message direct a destination de l'appli */
	Die			/* demande de terminaison de l'appli */

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
	regex_t regexp;			/* la regexp sous forme machine */
};

struct _clnt_lst {
	IvyClientPtr next;
	Client client;			/* la socket  client */
	MsgSndPtr msg_send;		/* liste des requetes recues */
	char *app_name;			/* nom de l'application */
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
static *direct_user_data = 0;

/* callback appele sur changement d'etat d'application */
static IvyApplicationCallback application_callback;
static *application_user_data = 0;

/* callback appele sur demande de terminaison d'application */
static IvyDieCallback application_die_callback;
static *application_die_user_data = 0;

/* liste des messages a recevoir */
static MsgRcvPtr msg_recv = 0;


/* liste des clients connectes */
static IvyClientPtr clients = 0;

static const char *ready_message = 0;

static void MsgSendTo( Client client, MsgType msgtype, int id, const char *message )
{
	SocketSend( client, "%d %d" ARG_START "%s\n", msgtype, id, message);
}

static void IvyCleanup()
{
	IvyClientPtr clnt,next;

	/* destruction des connexions clients */
	LIST_EACH_SAFE( clients, clnt, next )
	{
		/* on dit au revoir */
		MsgSendTo( clnt->client, Bye, 0, "" );
		SocketClose( clnt->client );
		LIST_EMPTY( clnt->msg_send );
	}
	LIST_EMPTY( clients );

	/* destruction des sockets serveur et supervision */
	SocketServerClose( server );
	SocketClose( broadcast );
}


static int
MsgCall (const char *message, MsgSndPtr msg,  Client client)
{
	regmatch_t match[MAX_MATCHING_ARGS+1];
#ifdef GNU_REGEXP
	regmatch_t* p;
#else
	unsigned int i;
#endif
	memset( match, -1, sizeof(match )); /* work around bug !!!*/
	if (regexec (&msg->regexp, message, MAX_MATCHING_ARGS, match, 0) != 0)
		return 0;

#ifdef DEBUG
	printf( "Sending message id=%d '%s'\n",msg->id,message);
#endif
	SocketSend( client, "%d %d" ARG_START ,Msg, msg->id);

#ifdef DEBUG
	printf( "Send matching args count %d\n",msg->regexp.re_nsub);
#endif //DEBUG

#ifdef GNU_REGEXP
	p = &match[1];
	while ( p->rm_so != -1 ) {
		SocketSend( client, "%.*s" ARG_END , p->rm_eo - p->rm_so, 
			    message + p->rm_so); 
		++p;
	}
#else
	for ( i = 1; i < msg->regexp.re_nsub+1; i ++ ) {
		if ( match[i].rm_so != -1 ) {
#ifdef DEBUG
			printf ("Send matching arg%d %d %d\n",i,match[i].rm_so , match[i].rm_eo);
			printf ("Send matching arg%d %.*s\n",i,match[i].rm_eo - match[i].rm_so, 
				message + match[i].rm_so);
#endif
			SocketSend (client, "%.*s" ARG_END ,match[i].rm_eo - match[i].rm_so, 
				message + match[i].rm_so);
		} else {
			SocketSend (client, ARG_END);
#ifdef DEBUG
			printf( "Send matching arg%d VIDE\n",i);
#endif //DEBUG
		}
	}
#endif

	SocketSend (client, "\n");
	return 1;
}
	
static int
ClientCall (IvyClientPtr clnt, const char *message)
{
	MsgSndPtr msg;
	int match_count = 0;
	/* recherche dans la liste des requetes recues de ce client */
	LIST_EACH (clnt->msg_send, msg) {
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

	if ( clnt->app_port == 0 )
		return 0;
	/* recherche dans la liste des clients de la presence de clnt */
	LIST_EACH( clients, client )
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

static void Receive( Client client, void *data, char *line )
{
	IvyClientPtr clnt;
	int err,id,reg;
	MsgSndPtr snd;
	MsgRcvPtr rcv;
	int argc = 0;
	char *argv[MAX_MATCHING_ARGS];
	char *arg;
	int kind_of_msg = Bye;
	regex_t regexp;

	err = sscanf( line ,"%d %d", &kind_of_msg, &id );
	arg = strstr( line , ARG_START );
	if ( (err != 2) || (arg == 0)  )
		{
		printf("Quitting bad format  %s\n",  line);
		MsgSendTo( client, Error, Error, "bad format request expected 'type id ...'" );
		MsgSendTo( client, Bye, 0, "" );
		SocketClose( client );
		return;
		}
	arg++;
	clnt = (IvyClientPtr)data;
	switch( kind_of_msg )
		{
		case Bye:
			
#ifdef DEBUG
			printf("Quitting  %s\n",  line);
#endif //DEBUG

			SocketClose( client );
			break;
		case Error:
			printf ("Received error %d %s\n",  id, arg);
			break;
		case AddRegexp:

#ifdef DEBUG
			printf("Regexp  id=%d exp='%s'\n",  id, arg);
#endif //DEBUG
			if ( !CheckRegexp( arg ) )
				{
#ifdef DEBUG
				printf("Warning: regexp '%s' illegal, removing from %s\n",arg,ApplicationName);
#endif //DEBUG
				return;
				}
			reg = regcomp(&regexp, arg, REG_ICASE|REG_EXTENDED);
			if ( reg == 0 )
				{
				LIST_ADD( clnt->msg_send, snd )
				if ( snd )
					{
					snd->id = id;
					snd->str_regexp = strdup( arg );
					snd->regexp = regexp;
					}
				}
			else
			{
			char errbuf[4096];
			regerror (reg, &regexp, errbuf, 4096);
			printf("Error compiling '%s', %s\n", arg, errbuf);
			MsgSendTo( client, Error, reg, errbuf );
			}
			break;
		case DelRegexp:

#ifdef DEBUG
			printf("Regexp Delete id=%d\n",  id);
#endif //DEBUG

			LIST_ITER( clnt->msg_send, snd, ( id != snd->id ));
			if ( snd )
				{
				free( snd->str_regexp );
				LIST_REMOVE( clnt->msg_send, snd  );
				}
			break;
		case StartRegexp:
			
#ifdef DEBUG
			printf("Regexp Start id=%d Application='%s'\n",  id, arg);
#endif //DEBUG
			clnt->app_name = strdup( arg );
			clnt->app_port = id;
			if ( CheckConnected( clnt ) )
			{			
#ifdef DEBUG
			printf("Quitting  already connected %s\n",  line);
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
		printf("Message id=%d msg='%s'\n", id, arg);
#endif //DEBUG

			LIST_EACH( msg_recv, rcv )
				{
				if ( id == rcv->id )
					{
					arg = strtok( arg, ARG_END);	
					while ( arg )
						{
						argv[argc++] = arg;
						arg = strtok( 0, ARG_END );
						}
#ifdef DEBUG
					printf("Calling  id=%d argc=%d for %s\n", id, argc,rcv->regexp);
#endif
					if ( rcv->callback ) (*rcv->callback)( clnt, rcv->user_data, argc, argv );
					return;
					}
				}
			printf("Callback Message id=%d not found!!!'\n", id);
			break;
		case DirectMsg:
			
#ifdef DEBUG
			printf("Direct Message id=%d msg='%s'\n", id, arg);
#endif //DEBUG

			if ( direct_callback)
				(*direct_callback)( clnt, direct_user_data, id, arg );
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

		default:
			printf("Receive unhandled message %s\n",  line);
			break;
		}
		
}

static IvyClientPtr SendService( Client client )
{
	IvyClientPtr clnt;
	MsgRcvPtr msg;
	LIST_ADD( clients, clnt )
	if ( clnt )
		{
		clnt->msg_send = 0;
		clnt->client = client;
		clnt->app_name = strdup("Unknown");
		clnt->app_port = 0;
		MsgSendTo( client, StartRegexp, ApplicationPort, ApplicationName);
		LIST_EACH(msg_recv, msg )
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
	LIST_EACH( clnt->msg_send, msg)
	{
		/*regfree(msg->regexp);*/
		free( msg->str_regexp);
	}
	LIST_EMPTY( clnt->msg_send );
	LIST_REMOVE( clients, clnt );
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

static void BroadcastReceive( Client client, void *data, char *line )
{	
	Client app;
	int err;
	int version;
	unsigned short serviceport;
#ifdef DEBUG
	unsigned short remoteport;
	char *remotehost = 0;
#endif

	err = sscanf (line,"%d %hu", &version, &serviceport);
	if ( err != 2 ) {
		/* ignore the message */
		unsigned short remoteport;
		char *remotehost;
		SocketGetRemoteHost (client, &remotehost, &remoteport );
		printf (" Bad supervision message, expected 'version port' from %s:%d\n",
				remotehost, remoteport);
		return;
	}
	if ( version != VERSION ) {
		/* ignore the message */
		unsigned short remoteport;
		char *remotehost = 0;
		SocketGetRemoteHost (client, &remotehost, &remoteport );
		fprintf (stderr, "Bad Ivy version, expected %d and got %d from %s:%d\n",
			VERSION, version, remotehost, remoteport);
		return;
	}
	/* check if we received our own message. SHOULD ALSO TEST THE HOST */
	if (serviceport == ApplicationPort) return;
	
#ifdef DEBUG
	SocketGetRemoteHost (client, &remotehost, &remoteport );
	printf(" Broadcast de %s:%hu port %hu\n", remotehost, remoteport, serviceport );
#endif //DEBUG

	/* connect to the service and send the regexp */
	app = SocketConnectAddr(SocketGetRemoteAddr(client), serviceport, 0, Receive, ClientDelete );
	if (app) {
		IvyClientPtr clnt;
		clnt = SendService( app );
		SocketSetData( app, clnt);
	}
}


void IvyInit (const char *appname, const char *ready, 
			 IvyApplicationCallback callback, void *data,
			 IvyDieCallback die_callback, void *die_data
			 )
{
	SocketInit();

	ApplicationName = appname;
	application_callback = callback;
	application_user_data = data;
	application_die_callback = die_callback;
	application_die_user_data = die_data;
	ready_message = ready;
}

void IvyClasses( int argc, const char **argv)
{
	messages_classes_count = argc;
	messages_classes = argv;
}


void IvyStart (const char* bus)
{
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
	if (!p)
		p = getenv ("IVYBUS");
	if (!p) 
		p = DefaultIvyBus;

	/* then, let's get a port number */
	q = strchr (p, ':');
	if (q && (port = atoi (q+1)))
		SupervisionPort = port;
	else
		SupervisionPort = DEFAULT_BUS;

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
				printf ("Broadcasting on network %lx, port %d\n", mask, SupervisionPort);
				SocketSendBroadcast (broadcast, mask, SupervisionPort, "%d %hu\n", VERSION, ApplicationPort); 
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
UnbindMsg (MsgRcvPtr msg)
{
	IvyClientPtr clnt;
	/* Send to already connected clients */
	LIST_EACH (clients, clnt ) {
		MsgSendTo( clnt->client, DelRegexp,msg->id, "");
	}
}

/* demande de reception d'un message */
static MsgRcvPtr
_BindMsg (MsgCallback callback, void *user_data, const char *regexp )
{
	static int recv_id = 0;
	IvyClientPtr clnt;
	MsgRcvPtr msg;
	/* add Msg to the query list */
	LIST_ADD( msg_recv, msg );
	if (msg)	{
		msg->id = recv_id++;
		msg->regexp = strdup(regexp);
		msg->callback = callback;
		msg->user_data = user_data;
	}
	/* Send to already connected clients */
	/* recherche dans la liste des requetes recues de mes clients */
	LIST_EACH( clients, clnt ) {
		MsgSendTo( clnt->client, AddRegexp,msg->id,msg->regexp);
	}
	return msg;
}

MsgRcvPtr
IvyBindMsg (MsgCallback callback, void *user_data, const char *fmt_regex, ... )
{
	char buffer[4096];
	va_list ap;
	
	va_start (ap, fmt_regex );
	vsprintf (buffer, fmt_regex, ap );
	va_end  (ap );
	return _BindMsg (callback, user_data, buffer );
}

static int
_SendMsg (const char *message)
{
	IvyClientPtr clnt;
	int match_count = 0;

	/* recherche dans la liste des requetes recues de mes clients */
	LIST_EACH (clients, clnt) {
		match_count += ClientCall (clnt, message);
	}
#ifdef DEBUG
	if ( match_count == 0 ) printf( "Warning no recipient for %s\n",message);
#endif
	return match_count;
}

int IvySendMsg(const char *fmt, ...)
{
	char buffer[4096];
	va_list ap;
	
	va_start( ap, fmt );
	vsprintf( buffer, fmt, ap );
	va_end ( ap );
	return _SendMsg( buffer );
}

void IvySendError( IvyClientPtr app, int id, const char *fmt, ... )
{
	char buffer[4096];
	va_list ap;
	
	va_start( ap, fmt );
	vsprintf( buffer, fmt, ap );
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

IvyClientPtr IvyGetApplication( char *name )
{
	IvyClientPtr app = 0;
	LIST_ITER( clients, app, strcmp(name, app->app_name) != 0 );
	return app;
}

char *IvyGetApplicationList()
{
	static char applist[4096];
	IvyClientPtr app;
	applist[0] = '\0';
	LIST_EACH( clients, app )
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
	LIST_EACH( app->msg_send, msg )
	{
	messagelist[msgCount++]= msg->str_regexp;
	}
	return messagelist;
}
