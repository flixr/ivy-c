/*
 *
 * $Id$
 */
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

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


typedef enum {

	Bye,			/* l'application emettrice se termine */
	AddRegexp,		/* expression reguliere d'un client */
	Msg,			/* message reel */
	Error,			/* error message */
	DelRegexp,		/* Remove expression reguliere */
	EndRegexp,		/* end of the regexp list */
	StartRegexp,	/* debut des expressions */
	DirectMsg,		/* message direct a destination de l'appli */
	Die				/* demande de terminaison de l'appli */

}MsgType;	


typedef struct _msg_snd *MsgSndPtr;

struct _msg_rcv {				/* requete d'emission d'un client */
	MsgRcvPtr next;
	int id;
	const char *regexp;			/* regexp du message a recevoir */
	MsgCallback callback;		/* callback a declanche a la reception */
	void *user_data;			/* stokage d'info client */
};


struct _msg_snd {				/* requete de reception d'un client */
	MsgSndPtr next;
	int id;
	char *str_regexp;			/* la regexp sous forme inhumaine */
	regex_t regexp;				/* la regexp sous forme machine */
};

struct _clnt_lst {
	BusClientPtr next;
	Client client;				/* la socket  client */
	MsgSndPtr msg_send;			/* liste des requetes recues */
	char *app_name;				/* nom de l'application */
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

static const char *ApplicationName = NULL;

/* classes de messages emis par l'application utilise pour le filtrage */
static int	messages_classes_count = 0;
static const char **messages_classes = NULL;

/* callback appele sur reception d'un message direct */
static MsgDirectCallback direct_callback = NULL;
static *direct_user_data = NULL;

/* callback appele sur changement d'etat d'application */
static BusApplicationCallback application_callback;
static *application_user_data = NULL;

/* callback appele sur demande de terminaison d'application */
static BusDieCallback application_die_callback;
static *application_die_user_data = NULL;

/* liste des messages a recevoir */
static MsgRcvPtr msg_recv = NULL;


/* liste des clients connectes */
static BusClientPtr clients = NULL;

static const char *ready_message = NULL;

static void MsgSendTo( Client client,MsgType msgtype, int id, const char *message )
{
SocketSend( client, "%d %d" ARG_START "%s\n",msgtype,id,message);
}
static void BusCleanup()
{
BusClientPtr clnt,next;

	/* destruction des connexion  clients */
	LIST_EACH_SAFE( clients, clnt, next )
	{
		/* on dit au revoir */
	MsgSendTo( clnt->client, Bye, 0, "" );
	SocketClose( clnt->client );
	LIST_EMPTY( clnt->msg_send );
	}
	LIST_EMPTY( clients );
	/* destruction des socket serveur et supervision */
	SocketServerClose( server );
	SocketClose( broadcast );
}
static int MsgCall( const char *message, MsgSndPtr msg,  Client client )
{
	regmatch_t match[MAX_MATCHING_ARGS+1];
#ifdef GNU_REGEXP
	regmatch_t* p;
#else
	unsigned int i;
#endif
	memset( match, -1, sizeof(match )); /* work around bug !!!*/
	if (regexec(&msg->regexp, message, MAX_MATCHING_ARGS, match, 0)==0) {
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
	for ( i = 1; i < msg->regexp.re_nsub+1; i ++ )
		{
			if ( match[i].rm_so != -1 )
			{
#ifdef DEBUG
			printf( "Send matching arg%d %d %d\n",i,match[i].rm_so , match[i].rm_eo);
			printf( "Send matching arg%d %.*s\n",i,match[i].rm_eo - match[i].rm_so, 
				message + match[i].rm_so);
#endif
			SocketSend( client, "%.*s" ARG_END ,match[i].rm_eo - match[i].rm_so, 
				message + match[i].rm_so);
			}
			else 
			{
			SocketSend( client, ARG_END );
#ifdef DEBUG
				printf( "Send matching arg%d VIDE\n",i);
#endif //DEBUG
			}
		}
#endif

	SocketSend( client, "\n");
	return 1;
	}
	return 0;
}
	
int ClientCall( BusClientPtr clnt, const char *message )
{
MsgSndPtr msg;
int match_count = 0;
	/* recherche dans la liste des requetes recues de ce client */
	LIST_EACH( clnt->msg_send, msg )
	{
	match_count+= MsgCall( message, msg, clnt->client );
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
static int CheckConnected( BusClientPtr clnt )
{
BusClientPtr client;
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
	BusClientPtr clnt;
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
	if ( (err != 2) || (arg == NULL)  )
		{
		printf("Quitting bad format  %s\n",  line);
		MsgSendTo( client, Error, Error, "bad format request expected 'type id ...'" );
		MsgSendTo( client, Bye, 0, "" );
		SocketClose( client );
		return;
		}
	arg++;
	clnt = (BusClientPtr)data;
	switch( kind_of_msg )
		{
		case Bye:
			
#ifdef DEBUG
			printf("Quitting  %s\n",  line);
#endif //DEBUG

			SocketClose( client );
			break;
		case Error:
			printf("Receive error %d  %s\n",  id, arg);
			break;
		case AddRegexp:

#ifdef DEBUG
			printf("Regexp  id=%d exp='%s'\n",  id, arg);
#endif //DEBUG
			if ( !CheckRegexp( arg ) )
				{
#ifdef DEBUG
				printf("Warning exp='%s' can't match removing from %s\n",arg,ApplicationName);
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
			printf("Error compiling '%s', %s\n",arg,errbuf);
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
			printf("Regexp Start id=%d App='%s'\n",  id, arg);
#endif //DEBUG
			clnt->app_name = strdup( arg );
			clnt->app_port = id;
			if ( CheckConnected( clnt ) )
			{			
#ifdef DEBUG
			printf("Quitting  already connected %s\n",  line);
#endif //DEBUG
			SendError( clnt, 0, "Application already connected" );
			SocketClose( client );
			}
			break;
		case EndRegexp:
			
#ifdef DEBUG
			printf("Regexp End id=%d\n",  id);
#endif //DEBUG
			if ( application_callback )
				{
				(*application_callback)( clnt, application_user_data, BusApplicationConnected );
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
						arg = strtok( NULL, ARG_END );
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
			BusCleanup();
			exit(0);
			break;

		default:
			printf("Receive unhandled message %s\n",  line);
			break;
		}
		
}

static BusClientPtr SendService( Client client )
{
	BusClientPtr clnt;
	MsgRcvPtr msg;
	LIST_ADD( clients, clnt )
	if ( clnt )
		{
		clnt->msg_send = NULL;
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
	BusClientPtr clnt;
	MsgSndPtr msg;
	char *remotehost;
	unsigned short remoteport;
	clnt = (BusClientPtr)data;
	if ( application_callback )
				{
				(*application_callback)( clnt, application_user_data, BusApplicationDisconnected );
				}
	SocketGetRemote( client, &remotehost, &remoteport );
	
#ifdef DEBUG
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
	char *remotehost;
	unsigned short remoteport;
	SocketGetRemote( client, &remotehost, &remoteport );

#ifdef DEBUG
	printf("Connexion de %s:%hu\n", remotehost, remoteport );
#endif //DEBUG

	return SendService( client );
}

static void BroadcastReceive( Client client, void *data, char *line )
{	
	Client app;
	char *remotehost;
	int err;
	int version;
	unsigned short remoteport;
	unsigned short serviceport;
	SocketGetRemote( client, &remotehost, &remoteport );

	err = sscanf(line,"%d %hu",&version, &serviceport);
	if ( err != 2 )
		{
		/* ignore the message */
		printf(" Bad Supervision message expected 'version port' from %s:%d\n",remotehost, remoteport);
		return;
		}
	if ( version != VERSION )
		{
		/* ignore the message */
		printf(" Bad Bus verion number expected %d receive %d from %s:%d\n", VERSION,version,remotehost, remoteport);
		return;
		}
	/* check if we receive our own message 
	should test also the host */
	if ( serviceport == ApplicationPort ) return;
	
#ifdef DEBUG
	printf(" Broadcast de %s:%hu port %hu\n", remotehost, remoteport, serviceport );	
#endif //DEBUG

	/* connect to the service and send the regexp */
	app = SocketConnectAddr(SocketGetRemoteAddr(client), serviceport, NULL, Receive, ClientDelete );
	if ( app )
		{
		BusClientPtr clnt;
		clnt = SendService( app );
		SocketSetData( app, clnt);
		}
}
void BusInit(const char *AppName, unsigned short busnumber, const char *ready, 
			 BusApplicationCallback callback, void *data,
			 BusDieCallback die_callback, void *die_data
			 )
{
	SocketInit();

	ApplicationName = AppName;
	SupervisionPort = busnumber;
	application_callback = callback;
	application_user_data = data;
	application_die_callback = die_callback;
	application_die_user_data = die_data;
	ready_message = ready;
	server = SocketServer( ANYPORT, ClientCreate, ClientDelete, Receive );
	ApplicationPort = SocketServerGetPort(server);
	broadcast =  SocketBroadcastCreate( SupervisionPort, NULL, BroadcastReceive );

}

void BusClasses( int argc, const char **argv)
{
	messages_classes_count = argc;
	messages_classes = argv;
}

void BusStart()
{
	
	SocketSendBroadcast( broadcast, 143 << 24 | 196 << 16 | 1 << 8 | 255, SupervisionPort, "%d %hu\n", VERSION, ApplicationPort);
	SocketSendBroadcast( broadcast, 143 << 24 | 196 << 16 | 2 << 8 | 255, SupervisionPort, "%d %hu\n", VERSION, ApplicationPort);
	SocketSendBroadcast( broadcast, 143 << 24 | 196 << 16 | 53 << 8 | 255, SupervisionPort, "%d %hu\n", VERSION, ApplicationPort);

	fprintf(stderr,"Server Ready  TCP:%hu\n",ApplicationPort);
}

/* desabonnements */
void UnbindMsg( MsgRcvPtr msg )
{
BusClientPtr clnt;
	/* Send to already connected clients */
	LIST_EACH( clients, clnt )
	{
	MsgSendTo( clnt->client, DelRegexp,msg->id, "");
	}
}

/* demande de reception d'un message */
static MsgRcvPtr _BindMsg( MsgCallback callback, void *user_data, const char *regexp )
{
	static int recv_id = 0;
	BusClientPtr clnt;
	MsgRcvPtr msg;
	/* add Msg to the query list */
	LIST_ADD( msg_recv, msg );
	if ( msg )
		{
		msg->id = recv_id++;
		msg->regexp = strdup(regexp);
		msg->callback = callback;
		msg->user_data = user_data;
		}
	/* Send to already connected clients */
	/* recherche dans la liste des requetes recues de mes clients */
	LIST_EACH( clients, clnt )
	{
	MsgSendTo( clnt->client, AddRegexp,msg->id,msg->regexp);
	}
	return msg;
}
MsgRcvPtr BindMsg( MsgCallback callback, void *user_data, const char *fmt_regex, ... )
{
	char buffer[4096];
	va_list ap;
	
	va_start( ap, fmt_regex );
	vsprintf( buffer, fmt_regex, ap );
	va_end ( ap );
	return _BindMsg( callback, user_data, buffer );
}
static int _SendMsg( const char *message )
{
BusClientPtr clnt;
int match_count = 0;

	/* recherche dans la liste des requetes recues de mes clients */
	LIST_EACH( clients, clnt )
	{
	match_count+= ClientCall( clnt, message );
	}
#ifdef DEBUG
	if ( match_count == 0 ) printf( "Warning no recipient for %s\n",message);
#endif
	return match_count;
}
int SendMsg(const char *fmt, ...)
{
	char buffer[4096];
	va_list ap;
	
	va_start( ap, fmt );
	vsprintf( buffer, fmt, ap );
	va_end ( ap );
	return _SendMsg( buffer );
}
void SendError( BusClientPtr app, int id, const char *fmt, ... )
{
	char buffer[4096];
	va_list ap;
	
	va_start( ap, fmt );
	vsprintf( buffer, fmt, ap );
	va_end ( ap );
	MsgSendTo( app->client, Error, id, buffer);
}
void BindDirectMsg( MsgDirectCallback callback, void *user_data)
{
direct_callback = callback;
direct_user_data = user_data;
}
void SendDirectMsg( BusClientPtr app, int id, char *msg )
{
	MsgSendTo( app->client, DirectMsg, id, msg);
}

void SendDieMsg( BusClientPtr app )
{
	MsgSendTo( app->client, Die, 0, "" );
}

char *GetApplicationName( BusClientPtr app )
{
	if ( app && app->app_name ) 
		return app->app_name;
	else return "Unknown";
}

char *GetApplicationHost( BusClientPtr app )
{
	if ( app && app->client ) 
		return SocketGetPeerHost( app->client );
	else return NULL;
}

void BusDefaultApplicationCallback( BusClientPtr app, void *user_data, BusApplicationEvent event)
{
	switch ( event )  {
	case BusApplicationConnected:
		printf("Application: %s ready on %s\n",GetApplicationName( app ), GetApplicationHost(app));
		break;
	case BusApplicationDisconnected:
		printf("Application: %s bye on %s\n",GetApplicationName( app ), GetApplicationHost(app));
		break;
	default:
		printf("Application: %s unkown event %d\n",GetApplicationName( app ), event);
		break;
	}
}

BusClientPtr GetApplication( char *name )
{
	BusClientPtr app = NULL;
	LIST_ITER( clients, app, strcmp(name, app->app_name) != 0 );
	return app;
}

char *GetApplicationList()
{
	static char applist[4096];
	BusClientPtr app;
	applist[0] = '\0';
	LIST_EACH( clients, app )
		{
		strcat( applist, app->app_name );
		strcat( applist, " " );
		}
	return applist;
}

char **GetApplicationMessages( BusClientPtr app )
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
