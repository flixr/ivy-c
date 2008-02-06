/*
 *
 *	Ivy, C interface
 *
 *	Copyright 1997-2008
 *	Centre d'Etudes de la Navigation Aerienne
 *
 *	Main functions
 *
 *	Authors: Francois-Regis Colin,Stephane Chatty, Alexandre Bustico
 *
 *	$Id$
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

/*
  TODO :  ° version non bloquante
	  ° outil de chasse aux memleak
	  ° mesures de perfo
	  ° compil rejeu en monothread et omp et tests
	  ° compil sur mac et windows pour portabilité
 */

#ifdef OPENMP
#include <omp.h>
#endif

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>



#include <fcntl.h>

#include "uthash.h"
#include "intervalRegexp.h"
#include "ivychannel.h"
#include "ivysocket.h"
#include "list.h"
#include "ivybuffer.h"
#include "ivydebug.h"
#include "ivybind.h"
#include "ivy.h"

#define VERSION 3

#define MAX_MATCHING_ARGS 40

#define ARG_START "\002"
#define ARG_END "\003"

#define DEFAULT_DOMAIN 127.255.255.255

/* stringification et concatenation du domaine et du port en 2 temps :
 * Obligatoire puisque la substitution de domain, et de bus n'est pas
 * effectuée si on stringifie directement dans la macro GenerateIvyBus */
#define str(bus) #bus
#define GenerateIvyBus(domain,bus) str(domain)":"str(bus)
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
	Die,			/* demande de terminaison de l'appli */
	Ping,			/* message de controle ivy */
	Pong			/* ivy doit renvoyer ce message à la reception d'un ping */
} MsgType;	


typedef struct _global_reg_lst	*GlobRegPtr;
typedef struct _msg_snd_dict	*MsgSndDictPtr;


struct _msg_rcv {			/* requete d'emission d'un client */
	MsgRcvPtr next;
	int id;
	const char *regexp;		/* regexp du message a recevoir */
	MsgCallback callback;		/* callback a declancher a la reception */
	void *user_data;		/* stokage d'info client */
};



/* liste de regexps source */
struct _global_reg_lst {		/* liste des regexp source */
	GlobRegPtr next;
	char *str_regexp;		/* la regexp sous forme source */
  	int id;                         /* son id, differente pour chaque client */
};


/* pour le dictionnaire clef=regexp, valeur = cette struct */  
struct _msg_snd_dict {			/* requete de reception d'un client */
        UT_hash_handle hh;		/* makes this structure hashable */
        char *regexp_src;		/* clef du dictionnaire (hash uthash) */
        IvyClientPtr clientList;        /* liste des clients */
	IvyBinding binding;		/* la regexp sous forme machine */
};

/* liste de clients, champ de la struct _msg_snd_dict qui est valeur du dictionnaire */
/* typedef IvyClientPtr */
struct _clnt_lst_dict {
	IvyClientPtr next;
	Client client;			/* la socket  client */
	char *app_name;			/* nom de l'application */
	unsigned short app_port;	/* port de l'application */
	int id;                         /* l'id n'est pas liée uniquement
					   a la regexp, mais au couple
					   regexp, client */
        GlobRegPtr srcRegList;          /* liste de regexp source */
#ifdef OPENMP
       int endRegexpReceived;
#endif // OPENMP
};

/* flag pour le debug en cas de Filter de regexp */
int debug_filter = 0;

/* flag pour le debug en cas de Filter de regexp */
int debug_binary_msg = 0;

/* server  pour la socket application */
static Server server;

/* numero de port TCP en mode serveur */
static unsigned short ApplicationPort;

/* numero de port UDP */
static unsigned short SupervisionPort;

/* client pour la socket supervision */
static Client broadcast;

static const char *ApplicationName = NULL;
static const char *ApplicationID = NULL;


/* callback appele sur reception d'un message direct */
static MsgDirectCallback direct_callback = NULL;
static  void *direct_user_data = NULL;

/* callback appele sur changement d'etat d'application */
static IvyApplicationCallback application_callback;
static void *application_user_data = NULL;

/* callback appele sur ajout suppression de regexp */
static IvyBindCallback application_bind_callback;
static void *application_bind_data = NULL;

/* callback appele sur demande de terminaison d'application */
static IvyDieCallback application_die_callback;
static void *application_die_user_data = NULL;

/* liste des messages a recevoir */
static MsgRcvPtr msg_recv = NULL;

/* liste des clients connectes */
static IvyClientPtr allClients = NULL;

/* dictionnaire clef : regexp, valeur : liste de clients IvyClientPtr */
static MsgSndDictPtr messSndByRegexp = NULL;

static const char *ready_message = NULL;
static void substituteInterval (IvyBuffer *src);

static int RegexpCall (const MsgSndDictPtr msg, const char * const message);
static int RegexpCallUnique (const MsgSndDictPtr msg, const char * const message, 
			     const Client clientUnique);

static void freeClient ( IvyClientPtr client);
static void delOneClient (const Client client);

static void delRegexpForOneClientFromDictionary (const char *regexp, const IvyClientPtr client);
static void delAllRegexpsFromDictionary ();
static void addRegexpToDictionary (const char* regexp, IvyClientPtr client, int id,
				   IvyBinding bind);
static void changeRegexpInDictionary (const char* regexp, IvyClientPtr client, int id, 
				      IvyBinding bind);

static char delRegexpForOneClient (const char *regexp, const IvyClientPtr client, int id);
static void addRegexp (const char* regexp, IvyClientPtr client, int id,
				   IvyBinding bind);
static void changeRegexp (const char* regexp, IvyClientPtr client, int id, IvyBinding bind);
static void addOrChangeRegexp (const char* regexp, IvyClientPtr client, int id, IvyBinding bind);
static int IvyCheckBuffer( const char* buffer );

#ifdef OPENMP
static void regenerateRegPtrArrayCache ();
static void addRegToPtrArrayCache (MsgSndDictPtr newReg);
static struct  {
  MsgSndDictPtr *msgPtrArray;
  int size;
  int numPtr;
} ompDictCache  = {NULL, 0, 0}; 
#endif 


/*
 * function like strok but do not eat consecutive separator
 * */
static char * nextArg( char *s, const char *separator )
{
	static char *start = NULL;
	static char *end = NULL;
	if ( s ) 
	{
		end = s;
	}
	start = end;

	while ( *end && *end != *separator )
		end++;
	if ( *end == *separator ) *end++ = '\0';  
	if ( end == start ) return NULL;
	return start;
}
static int MsgSendTo( Client client, MsgType msgtype, int id, const char *message )
{
	return SocketSend( client, "%d %d" ARG_START "%s\n", msgtype, id, message);
}

static void IvyCleanup()
{
	IvyClientPtr clnt,next;
	GlobRegPtr   regLst;
	

	/* destruction des connexions clients */
	IVY_LIST_EACH_SAFE( allClients, clnt, next )
	{
		/* on dit au revoir */
		MsgSendTo( clnt->client, Bye, 0, "" );
		SocketClose( clnt->client );
		IVY_LIST_EACH (clnt->srcRegList, regLst) {
		  if (regLst->str_regexp != NULL) {
		    free (regLst->str_regexp);
		    regLst->str_regexp = NULL;
		  }
		}
		IVY_LIST_EMPTY( clnt->srcRegList );
		IVY_LIST_REMOVE (allClients, clnt);
	}
	IVY_LIST_EMPTY( allClients );
	delAllRegexpsFromDictionary ();

	/* destruction des sockets serveur et supervision */
	SocketServerClose( server );
	SocketClose( broadcast );
}


static int
ClientCall (IvyClientPtr clnt, const char *message)
{
  int match_count = 0;

  /*   pour toutes les regexp */
  MsgSndDictPtr msgSendDict;
  
  for (msgSendDict=messSndByRegexp; msgSendDict != NULL; 
       msgSendDict=msgSendDict->hh.next) {
    match_count += RegexpCallUnique (msgSendDict, message, clnt->client);
  }
  
  TRACE_IF( match_count == 0, "Warning no recipient for %s\n",message);
  /* si le message n'est pas emit et qu'il y a des filtres alors WARNING */
  if ( match_count == 0 && debug_filter )  {
    IvyBindindFilterCheck( message );
  }
  return match_count;
}



static int
RegexpCall (const MsgSndDictPtr msg, const char * const message)
{
  static IvyBuffer bufferArg = {NULL, 0, 0}; 
  char   bufferId[16]; 
  int match_count ;
  int waiting ;
  int indx;
  int arglen;
  const char *arg;
  IvyClientPtr clnt;
  int rc;

#ifdef OPENMP
#pragma omp threadprivate (bufferArg)
  /* d'après la doc openmp : 
     Variables with automatic storage duration which are declared in a scope inside the
     construct are private.
  Il n'y aurait donc rien à faire pour s'assurer que les variables automatiques soient
  privées au thread */
#endif // OPENMP

  match_count = 0;
  waiting = 0;
  rc= IvyBindingExec(msg->binding, message );
	
  if (rc<1) return 0; /* no match */
	
  bufferArg.offset = 0;
  //  bufferArg.size = bufferId.size = 0;
  //  bufferArg.data = bufferId.data = NULL;

  /* il faut essayer d'envoyer le message en une seule fois sur la socket */
  /* pour eviter au maximun de passer dans le select plusieur fois par message du protocole Ivy */
  /* pour eviter la latence ( PB de perfo detecte par ivyperf ping roudtrip ) */

  TRACE( "Send matching args count %d\n",rc);
	
  for(  indx=1; indx < rc ; indx++ )
    {
      IvyBindingMatch (msg->binding, message, indx, &arglen, & arg );
      make_message_var( &bufferArg,  "%.*s" ARG_END , arglen, arg );
    }
  make_message_var( &bufferArg, "\n");

  IVY_LIST_EACH(msg->clientList, clnt ) {

    sprintf (bufferId, "%d %d" ARG_START ,Msg, clnt->id);
    waiting = SocketSendRawWithId(clnt->client, bufferId, bufferArg.data , bufferArg.offset);
    match_count++;
    if ( waiting )
      fprintf(stderr, "Ivy: Slow client : %s\n", clnt->app_name );
  }
  return match_count;
}

static int
RegexpCallUnique (const MsgSndDictPtr msg, const char * const message, const 
		  Client clientUnique)
{
  static IvyBuffer bufferArg = {NULL, 0, 0}; 
  char   bufferId[16];
  int match_count ;
  int waiting ;
  int indx;
  int arglen;
  const char *arg;
  IvyClientPtr clnt;
  int rc;

  match_count = 0;
  waiting = 0;
  rc= IvyBindingExec(msg->binding, message );
	
  if (rc<1) return 0; /* no match */
	
  bufferArg.offset = 0;
  //  bufferArg.size = bufferId.
  //  bufferArg.size = bufferId.size = 0;
  //  bufferArg.data = bufferId.data = NULL;

  /* il faut essayer d'envoyer le message en une seule fois sur la socket */
  /* pour eviter au maximun de passer dans le select plusieur fois par message du protocole Ivy */
  /* pour eviter la latence ( PB de perfo detecte par ivyperf ping roudtrip ) */

  TRACE( "Send matching args count %d\n",rc);
	
  for(  indx=1; indx < rc ; indx++ )
    {
      IvyBindingMatch (msg->binding, message, indx, &arglen, & arg );
      make_message_var( &bufferArg,  "%.*s" ARG_END , arglen, arg );
    }
  make_message_var( &bufferArg, "\n");

  IVY_LIST_EACH(msg->clientList, clnt ) {
    if (clientUnique != clnt->client)
      continue;
    sprintf (bufferId, "%d %d" ARG_START ,Msg, clnt->id);
    waiting = SocketSendRawWithId(clnt->client, bufferId, bufferArg.data , bufferArg.offset);
    match_count++;
    if ( waiting )
      fprintf(stderr, "Ivy: Slow client : %s\n", clnt->app_name );
  }
  return match_count;
}




static int CheckConnected( IvyClientPtr clnt )
{
	IvyClientPtr client;
	struct in_addr *addr1;
	struct in_addr *addr2;

	if ( clnt->app_port == 0 ) /* Old Ivy Protocol Dont check */
		return 0;
	/* recherche dans la liste des clients de la presence de clnt */
	IVY_LIST_EACH( allClients, client )
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
		/* client different mais applicationID identique */
		/* TODO est-ce utile ??? verif dans UDP 
		if ( (client != clnt) && (clnt->app_id == client->app_id) )
			{
				return 1;
			}
		*/
			
	}
	return 0;
}



static void Receive( Client client, void *data, char *line )
{
	IvyClientPtr clnt;
	int err,id;
	MsgRcvPtr rcv;
	int argc = 0;
	char *argv[MAX_MATCHING_ARGS];
	char *arg;
	int kind_of_msg = Bye;
	IvyBinding ivyBind;
	
	const char *errbuf;
	int erroffset;

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
			
			TRACE("Quitting  %s\n",  line);

			SocketClose( client );
			break;
		case Error:
			printf ("Received error %d %s\n",  id, arg);
			break;
		case AddRegexp:


			TRACE("Regexp  id=%d exp='%s'\n",  id, arg);

			if ( !IvyBindingFilter( arg ) )
				{

				TRACE("Warning: regexp '%s' filtered, removing from %s\n",arg,ApplicationName);

				if ( application_bind_callback )
					  {
					    (*application_bind_callback)( clnt, application_bind_data, id, arg, IvyFilterBind );
					  }
				return;
				}

			ivyBind = IvyBindingCompile( arg, & erroffset, & errbuf );
			if ( ivyBind != NULL ) {
			  addOrChangeRegexp (arg, clnt, id, ivyBind);
			} else { 
			  printf("Error compiling '%s', %s\n", arg, errbuf);
			  MsgSendTo( client, Error, erroffset, errbuf );
			}

			break;
		case DelRegexp:
		  
		  TRACE("Regexp Delete id=%d\n",  id);
		  if (delRegexpForOneClient (arg, clnt, id)) {
		    if ( application_bind_callback )  {
		      (*application_bind_callback)( clnt, application_bind_data, id, arg, 
						    IvyRemoveBind );
		    }
		  }
		  break;
		case StartRegexp:

			TRACE("Regexp Start id=%d Application='%s'\n",  id, arg);

#ifdef OPENMP
			clnt->endRegexpReceived=0;
#endif // OPENMP
			
			clnt->app_name = strdup( arg );
			clnt->app_port = id;
			if ( CheckConnected( clnt ) )
			{			

			TRACE("Quitting  already connected %s\n",  line);

			IvySendError( clnt, 0, "Application already connected" );
			SocketClose( client );
			}
			break;
		case EndRegexp:
			
			TRACE("Regexp End id=%d\n",  id);

			if ( application_callback )
				{
				(*application_callback)( clnt, application_user_data, IvyApplicationConnected );
				}
			if ( ready_message )
				{
				int count;
				count = ClientCall( clnt, ready_message );
				// count = IvySendMsg ("%s", ready_message );
				//				printf ("%s sending READY MESSAGE %d\n", clnt->app_name, count);
				}
#ifdef OPENMP
			clnt->endRegexpReceived=1;
			regenerateRegPtrArrayCache();
#endif // OPENMP
			break;
		case Msg:
			
			TRACE("Message id=%d msg='%s'\n", id, arg);

			IVY_LIST_EACH( msg_recv, rcv )
				{
				if ( id == rcv->id )
					{
					arg = nextArg( arg, ARG_END);	
					while ( arg )
						{
						argv[argc++] = arg;
						arg = nextArg( 0, ARG_END );
						}
					TRACE("Calling  id=%d argc=%d for %s\n", id, argc,rcv->regexp);
					if ( rcv->callback ) (*rcv->callback)( clnt, rcv->user_data, argc, argv );
					return;
					}
				}
			printf("Callback Message id=%d not found!!!'\n", id);
			break;
		case DirectMsg:
			
			TRACE("Direct Message id=%d msg='%s'\n", id, arg);

			if ( direct_callback)
				(*direct_callback)( clnt, direct_user_data, id, arg );
			break;

		case Die:
			
			TRACE("Die Message\n");

			if ( application_die_callback)
				(*application_die_callback)( clnt, application_die_user_data, id );
			IvyCleanup();
			exit(0);
			break;

		case Ping:
			
			TRACE("Ping Message\n");
			MsgSendTo( client, Pong, id, "" );
			break;

		case Pong:
			
			TRACE("Pong Message\n");
			printf("Receive unhandled Pong message (ivy-c not able to send ping)\n");
			break;

		default:
			printf("Receive unhandled message %s\n",  line);
			break;
		}
		
}

static IvyClientPtr SendService( Client client, const char *appname )
{
	IvyClientPtr clnt;
	MsgRcvPtr msg;
	IVY_LIST_ADD_START( allClients, clnt )
		clnt->client = client;
		clnt->app_name = strdup(appname);
		clnt->app_port = 0;
		MsgSendTo( client, StartRegexp, ApplicationPort, ApplicationName);
		IVY_LIST_EACH(msg_recv, msg )
			{
			MsgSendTo( client, AddRegexp,msg->id,msg->regexp);
			}
		MsgSendTo( client, EndRegexp, 0, "");
		
	IVY_LIST_ADD_END( allClients, clnt )
	  //printf ("DBG> SendService addAllClient: name=%s; client->client=%p\n", appname, clnt->client);

	return clnt;
}

static void ClientDelete( Client client, void *data )
{
	IvyClientPtr clnt;

#ifdef DEBUG
	char *remotehost;
	unsigned short remoteport;
#endif
	clnt = (IvyClientPtr)data;
	if ( application_callback )  {
	  (*application_callback)( clnt, application_user_data, IvyApplicationDisconnected );
	}
	
#ifdef DEBUG
	/* probably bogus call, but this is for debug only anyway */
	SocketGetRemoteHost( client, &remotehost, &remoteport );
	TRACE("Deconnexion de %s:%hu\n", remotehost, remoteport );
#endif /*DEBUG */
	delOneClient (client);
}

static void *ClientCreate( Client client )
{

  // #ifdef DEBUG
  char *remotehost;
  unsigned short remoteport;
  SocketGetRemoteHost( client, &remotehost, &remoteport );
  printf ("%s : Connexion de %s:%hu client=%p\n", 
	  ApplicationName, remotehost, remoteport, client);
  // #endif /*DEBUG */
  return SendService (client, "Unknown");
}

static void BroadcastReceive( Client client, void *data, char *line )
{	
	Client app;
	int err;
	int version;
	unsigned short serviceport;
	char appid[2048];
	char appname[2048];
	unsigned short remoteport;
	char *remotehost = 0;

	memset( appid, 0, sizeof( appid ) );
	memset( appname, 0, sizeof( appname ) );
	err = sscanf (line,"%d %hu %s %[^\n]", &version, &serviceport, appid, appname);
	if ( err < 2 ) {
		/* ignore the message */
		SocketGetRemoteHost (client, &remotehost, &remoteport );
		printf (" Bad supervision message, expected 'version port' from %s:%d\n",
				remotehost, remoteport);
		return;
	}
	if ( version != VERSION ) {
		/* ignore the message */
		SocketGetRemoteHost (client, &remotehost, &remoteport );
		fprintf (stderr, "Bad Ivy version, expected %d and got %d from %s:%d\n",
			VERSION, version, remotehost, remoteport);
		return;
	}
	/* check if we received our own message. SHOULD ALSO TEST THE HOST */
	if ( strcmp( appid , ApplicationID) ==0 ) return;
	if (serviceport == ApplicationPort) return;
	
#ifdef DEBUG
	SocketGetRemoteHost (client, &remotehost, &remoteport );
	TRACE(" Broadcast de %s:%hu port %hu\n", remotehost, remoteport, serviceport );
#endif /*DEBUG */

	/* connect to the service and send the regexp */
	app = SocketConnectAddr(SocketGetRemoteAddr(client), serviceport, 0, Receive, ClientDelete );
	if (app) {
		IvyClientPtr clnt;
		clnt = SendService( app, appname );
		SocketSetData( app, clnt);
	} else {
	  printf ("SocketConnectAddr error .....\n");
	  SocketSetData( app, NULL);
	}
}
static unsigned long currentTime()
{
#define MILLISEC 1000
	unsigned long current;
#ifdef WIN32
	current = GetTickCount();
#else
	struct timeval stamp;
        gettimeofday( &stamp, NULL );
        current = stamp.tv_sec * MILLISEC + stamp.tv_usec/MILLISEC;
#endif
        return  current;
}

static const char * GenApplicationUniqueIdentifier()
{
	static char appid[2048];
	unsigned long curtime;
	curtime = currentTime();
	srand( curtime );
	sprintf(appid,"%d:%lu:%d",rand(),curtime,ApplicationPort);
	return appid;
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

	if ( getenv( "IVY_DEBUG_BINARY" )) debug_binary_msg = 1;
}

void IvySetBindCallback( IvyBindCallback bind_callback, void *bind_data )
{
  application_bind_callback=bind_callback;
  application_bind_data=bind_data;
}

void IvySetFilter( int argc, const char **argv)
{
	IvyBindingSetFilter( argc, argv );
	if ( getenv( "IVY_DEBUG_FILTER" )) debug_filter = 1;

}

void IvyStop (void)
{
	IvyChannelStop();
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
	unsigned short port;

	
	/*
	 * Initialize TCP port
	 */
	server = SocketServer (ANYPORT, ClientCreate, ClientDelete, Receive);
	ApplicationPort = SocketServerGetPort (server);
	ApplicationID = GenApplicationUniqueIdentifier();
	        
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
				/* test mask value agaisnt CLASS D */
				if ( IN_MULTICAST( mask ) )
					SocketAddMember (broadcast , mask );

				SocketSendBroadcast (broadcast, mask, SupervisionPort, "%d %hu %s %s\n", VERSION, ApplicationPort, ApplicationID, ApplicationName); 
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

	TRACE ("Listening on TCP:%hu\n",ApplicationPort);

}

/* desabonnements */
void
IvyUnbindMsg (MsgRcvPtr msg)
{
	IvyClientPtr clnt;
	/* Send to already connected clients */
	IVY_LIST_EACH (allClients, clnt ) {
		MsgSendTo( clnt->client, DelRegexp,msg->id, "");
	}
	IVY_LIST_REMOVE( msg_recv, msg  );
}

/* demande de reception d'un message */

MsgRcvPtr
IvyBindMsg (MsgCallback callback, void *user_data, const char *fmt_regex, ... )
{
	static IvyBuffer buffer = { NULL, 0, 0};
	va_list ap;
	static int recv_id = 0;
	IvyClientPtr clnt;
	MsgRcvPtr msg;

	va_start (ap, fmt_regex );
	buffer.offset = 0;
	make_message( &buffer, fmt_regex, ap );
	va_end  (ap );

	substituteInterval (&buffer);

	/* add Msg to the query list */
	IVY_LIST_ADD_START( msg_recv, msg )
		msg->id = recv_id++;
		msg->regexp = strdup(buffer.data);
		msg->callback = callback;
		msg->user_data = user_data;
	IVY_LIST_ADD_END( msg_recv, msg )
	/* Send to already connected clients */
	/* recherche dans la liste des requetes recues de mes clients */
	IVY_LIST_EACH( allClients, clnt ) {
		MsgSendTo( clnt->client, AddRegexp,msg->id,msg->regexp);
	}
	return msg;
}

/* changement de regexp d'un bind existant precedement fait avec IvyBindMsg */
MsgRcvPtr
IvyChangeMsg (MsgRcvPtr msg, const char *fmt_regex, ... )
{
	static IvyBuffer buffer = { NULL, 0, 0};
	va_list ap;
	IvyClientPtr clnt;

	va_start (ap, fmt_regex );
	buffer.offset = 0;
	make_message( &buffer, fmt_regex, ap );
	va_end  (ap );

	substituteInterval (&buffer);

	/* change Msg in the query list */
	msg->regexp = strdup(buffer.data);
	
	/* Send to already connected clients */
	/* recherche dans la liste des requetes recues de mes clients */
	IVY_LIST_EACH( allClients, clnt ) {
	  MsgSendTo( clnt->client, AddRegexp,msg->id,msg->regexp);
	}
	return msg;
}



int IvySendMsg(const char *fmt, ...) /* version dictionnaire */
{
  int match_count = 0;
  static IvyBuffer buffer = { NULL, 0, 0}; /* Use static mem to eliminate multiple call to malloc /free */
  va_list ap;
  
  /* construction du buffer message à partir du format et des arguments */
  if( fmt == 0 || strlen(fmt) == 0 ) return 0;	
  va_start( ap, fmt );
  buffer.offset = 0;
  make_message( &buffer, fmt, ap );
  va_end ( ap );

  /* test du contenu du message */
  if ( debug_binary_msg  )
    {
      if ( IvyCheckBuffer( buffer.data ) )
	return 0;
    }

  /*   pour toutes les regexp */

#ifdef OPENMP 
  //#define TABLEAU_PREALABLE_SEQUENTIEL 1
#define TABLEAU_PREALABLE 1
  //#define SINGLE_NOWAIT  1
  //#define SCHEDULE_GUIDED 1
  //#define SEQUENTIEL_DEBUG 1

#ifdef SCHEDULE_GUIDED
  int count;
#pragma omp parallel  default(none) private(count) shared(ompDictCache, buffer) \
                      reduction(+:match_count) 
  {
#pragma omp for schedule(guided) // après debug mettre  schedule(guided, 10)
  for(count=0; count<ompDictCache.numPtr; count++) {
    match_count += RegexpCall (ompDictCache.msgPtrArray[count], buffer.data);
  }
}  
#endif // SCHEDULE_GUIDED


#ifdef  TABLEAU_PREALABLE
  int count; // PARALLEL FOR
#pragma omp parallel for default(none) private(count) shared(ompDictCache, buffer) \
			 reduction(+:match_count)
  for(count=0; count<ompDictCache.numPtr; count++) {
    match_count += RegexpCall (ompDictCache.msgPtrArray[count], buffer.data);
  }
#endif // TABLEAU_PREALABLE


#ifdef  TABLEAU_PREALABLE_SEQUENTIEL
  int count; 
  for(count=0; count<ompDictCache.numPtr; count++) {
    match_count += RegexpCall (ompDictCache.msgPtrArray[count], buffer.data);
  }
#endif // TABLEAU_PREALABLE_SEQUENTIEL


#ifdef SINGLE_NOWAIT // OPEMMP LISTE
  MsgSndDictPtr msgSendDict;
#pragma omp parallel  default(shared)  private(msgSendDict) reduction(+:match_count)
  for (msgSendDict=messSndByRegexp; msgSendDict ; msgSendDict=msgSendDict->hh.next) {
#pragma omp single nowait 
    match_count += RegexpCall (msgSendDict, buffer.data);
  }
#endif // SINGLE_NOWAIT

#ifdef SEQUENTIEL_DEBUG // OPEMMP LISTE
  MsgSndDictPtr msgSendDict;

  for (msgSendDict=messSndByRegexp; msgSendDict ; msgSendDict=msgSendDict->hh.next) {
    match_count += RegexpCall (msgSendDict, buffer.data);
  }
#endif // SEQUENTIEL_DEBUG



#else // PAS OPENMP
  MsgSndDictPtr msgSendDict;

  for (msgSendDict=messSndByRegexp; msgSendDict ; msgSendDict=msgSendDict->hh.next) {
    match_count += RegexpCall (msgSendDict, buffer.data);
  }
#endif

  TRACE_IF( match_count == 0, "Warning no recipient for %s\n",buffer.data);
  /* si le message n'est pas emit et qu'il y a des filtres alors WARNING */
  if ( match_count == 0 && debug_filter )
    {
      IvyBindindFilterCheck( buffer.data );
    }
  return match_count;
}


/* teste de la presence de binaire dans les message Ivy */
static int IvyCheckBuffer( const char* buffer )
{
	const char * ptr = buffer;
	while ( *ptr )
	{
		if ( *ptr++ < ' ' ) 
		{
			fprintf(stderr," IvySendMsg bad msg to send binary data not allowed ignored %s\n",
				buffer );
			return 1;
		}
	}
	return 0;
}


void IvySendError( IvyClientPtr app, int id, const char *fmt, ... )
{
	static IvyBuffer buffer = { NULL, 0, 0}; /* Use static mem to eliminate multiple call to malloc /free */
	va_list ap;
	
	va_start( ap, fmt );
	buffer.offset = 0;
	make_message( &buffer, fmt, ap );
	va_end ( ap );
	MsgSendTo( app->client, Error, id, buffer.data);
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
void IvyDefaultBindCallback( IvyClientPtr app, void *user_data, int id, char* regexp,  IvyBindEvent event)
{
	switch ( event )  {
	case IvyAddBind:
		printf("Application: %s on %s add regexp %d : %s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
		break;
	case IvyRemoveBind:
		printf("Application: %s on %s remove regexp %d :%s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
		break;
	case IvyFilterBind:
		printf("Application: %s on %s as been filtred regexp %d :%s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
		break;
	case IvyChangeBind:
	        printf("Application: %s on %s change regexp %d : %s\n", IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
		break;
		break;
	default:
		printf("Application: %s unkown event %d\n",IvyGetApplicationName( app ), event);
		break;
	}
}

IvyClientPtr IvyGetApplication( char *name )
{
	IvyClientPtr app = 0;
	IVY_LIST_ITER( allClients, app, strcmp(name, app->app_name) != 0 );
	return app;
}

char *IvyGetApplicationList(const char *sep)
{
	static char applist[4096]; /* TODO remove that ugly Thing */
	IvyClientPtr app;
	applist[0] = '\0';
	IVY_LIST_EACH( allClients, app )
		{
		strcat( applist, app->app_name );
		strcat( applist, sep );
		}
	return applist;
}

char **IvyGetApplicationMessages( IvyClientPtr app )
{
#define MAX_REGEXP 4096
	static char *messagelist[MAX_REGEXP+1];/* TODO remove that ugly Thing */
	GlobRegPtr  msg;
	int msgCount= 0;
	memset( messagelist, 0 , sizeof( messagelist ));
	/* recherche dans la liste des requetes recues de ce client */
	IVY_LIST_EACH( app->srcRegList, msg )
	{
	messagelist[msgCount++]= msg->str_regexp;
	if ( msgCount >= MAX_REGEXP )
		{
		fprintf(stderr,"Too Much expression(%d) for buffer\n",msgCount);
		break;
		}
	}
	return messagelist;
}

static void substituteInterval (IvyBuffer *src)
{
  /* pas de traitement couteux s'il n'y a rien à interpoler */
  if (strstr (src->data, "(?I") == NULL) {
    return;
  } else {
	char *curPos;
    char *itvPos;
    IvyBuffer dst = {NULL, 0, 0};
    dst.size = 8192;
    dst.data = malloc (dst.size);

    curPos = src->data;
    while ((itvPos = strstr (curPos, "(?I")) != NULL) {
      /* copie depuis la position courante jusqu'à l'intervalle */
      int lenCp, min,max;
      char withDecimal;
      lenCp = itvPos-curPos;
      memcpy (&(dst.data[dst.offset]), curPos, lenCp);
      curPos=itvPos;
      dst.offset += lenCp;

      /* extraction des paramètres de l'intervalle */
      sscanf (itvPos, "(?I%d#%d%c", &min, &max, &withDecimal);

      /*      printf ("DBG> substituteInterval min=%d max=%d withDecimal=%d\n",  */
      /*      min, max, (withDecimal != 'i'));    */
  
      /* generation et copie de l'intervalle */
      regexpGen (&(dst.data[dst.offset]), dst.size-dst.offset, min, max, (withDecimal != 'i'));
      dst.offset = strlen (dst.data);

      /* consommation des caractères décrivant intervalle dans la chaine source */
      curPos = strstr (curPos, ")");
      curPos++;
    }
    strncat (dst.data, curPos, dst.size-dst.offset);
    free (src->data);
    src->data = dst.data;
  }
}


static void freeClient ( IvyClientPtr client)
{
  GlobRegPtr srcReg;

  /* on libere la chaine nom de l'appli*/
  if (client->app_name != NULL) {
    free (client->app_name);
    client->app_name = NULL;
    /* on libere la liste des clients */
    IVY_LIST_EACH (client->srcRegList, srcReg) {
      if (srcReg->str_regexp != NULL) {
	free (srcReg->str_regexp);
	srcReg->str_regexp = NULL;
      }
    }
    IVY_LIST_EMPTY (client->srcRegList);
  }
}







static void delRegexpForOneClientFromDictionary (const char *regexp, const IvyClientPtr client)
{
  MsgSndDictPtr msgSendDict = NULL;
  IvyClientPtr  client_itr, next;
  TRACE ("ENTER delRegexpForOneClientFromDictionary clnt=%d, reg='%s'\n", client, regexp);

  HASH_FIND_STR(messSndByRegexp, regexp, msgSendDict);
  if (msgSendDict != NULL) {
    /* la clef est trouvée, on itere sur la liste de client associée */
    IVY_LIST_EACH_SAFE ( msgSendDict->clientList, client_itr, next) { 
      /* pour tester 2 IvyClientPtr, on teste la similarité 
	 des pointeur Client qui doivent être uniques */
      if (client_itr->client == client->client) {
	/* on a trouve le client : on l'enleve */
	IVY_LIST_REMOVE (msgSendDict->clientList, client_itr);
	TRACE ("delRegexpForOneClientFromDictionary : IVY_LIST_REMOVE\n");
      }
    }
    /* si la liste de clients associée à cette regexp est vide */
    if ((msgSendDict->clientList == NULL) || (IVY_LIST_IS_EMPTY (msgSendDict->clientList))) {
      TRACE ("delRegexpForOneClientFromDictionary : IvyBindingFree, free, hash_del\n");
      /* on efface le binding */
      IvyBindingFree (msgSendDict->binding);
      /* on enlève l'entrée regexp de la table de hash */
      HASH_DEL (messSndByRegexp, msgSendDict);
      /* on efface la clef (regexp source) */
      free (msgSendDict->regexp_src);
    }
  }
#ifdef OPENMP
  regenerateRegPtrArrayCache ();  
#endif
}




static void delAllRegexpsFromDictionary ()
{
  MsgSndDictPtr msgSendDict;
  IvyClientPtr  client;

  /* pour toutes les entrees du dictionnaire des regexps */
  for (msgSendDict=messSndByRegexp; msgSendDict ; msgSendDict=msgSendDict->hh.next) {
    /* on efface le binding */
    IvyBindingFree (msgSendDict->binding);
    /* on efface la clef (regexp source) */
    free (msgSendDict->regexp_src);
    /* pour chaque client abonne a cette regexp */
    IVY_LIST_EACH ( msgSendDict->clientList, client) { 
      freeClient (client);
    }
    /* on enleve la liste de regexps */
    IVY_LIST_EMPTY(msgSendDict->clientList);
    /* on enleve le couple regexp -> valeur */
    HASH_DEL(messSndByRegexp, msgSendDict);
  }

#ifdef OPENMP
  regenerateRegPtrArrayCache ();  
#endif
}









// HASH_ADD_KEYPTR  	 (hh_name, head, key_ptr, key_len, item_ptr)
// HASH_ADD_STR  	 (         head, keyfield_name,    item_ptr)

static void addRegexpToDictionary (const char* regexp, IvyClientPtr client, int id, 
				   IvyBinding ivyBind)
{
  MsgSndDictPtr msgSendDict = NULL;
  IvyClientPtr  newClient = NULL;
  /* on cherche si une entrée existe deja pour cette regexp source */
  HASH_FIND_STR(messSndByRegexp, regexp, msgSendDict);
    /* l'entree n'existe pas dans le dictionnaire : on la cree */
  if (msgSendDict == NULL) {
    msgSendDict = malloc (sizeof (struct _msg_snd_dict));
    msgSendDict->regexp_src = strdup (regexp);
    msgSendDict->binding = ivyBind;
    msgSendDict->clientList = NULL;

    /* HASH_ADD_STR ne fonctionne que si la clef est un tavbleau de char, si c'est un pointeur 
       if faut utiliser HASH_ADD_KEYPTR */
    HASH_ADD_KEYPTR(hh, messSndByRegexp, msgSendDict->regexp_src, strlen (msgSendDict->regexp_src), msgSendDict); 
#ifdef OPENMP
    // On ne regenere le cache qu'après recpetion du endregexp, ça permet d'eviter
    // de regenerer inutilement le cache à chaqye nouvelle regexp initiale
    // par contre, après le end regexp, il faut regenerer le cache à chaque
    // nouvel abonnement
    if (client->endRegexpReceived == 1)
      addRegToPtrArrayCache (msgSendDict);  
#endif
  } 


  /* on ajoute le client à la liste des clients abonnés */
  IVY_LIST_ADD_START (msgSendDict->clientList, newClient);
  newClient->app_name = strdup (client->app_name);
  newClient->app_port = client->app_port;
  newClient->client = client->client;
  newClient->id = id;
  /* au niveau du champ liste de client du dictionnaire, on n'a pas besoin
     de la liste des regexps sources (qui n'est necessaire que pour
     la liste globale des clients) */
  newClient->srcRegList = NULL;
  IVY_LIST_ADD_END (msgSendDict->clientList, newClient);
}



static void changeRegexpInDictionary (const char* regexp, IvyClientPtr client,
				      int id, IvyBinding ivyBind) 
{
  //  printf ("ENTER changeRegexpInDictionary\n");
  delRegexpForOneClientFromDictionary (regexp, client);
  addRegexpToDictionary (regexp, client, id, ivyBind);
}




/* met a jour le dictionnaire et la liste globale */
static void delOneClient (const Client client)
{
  IvyClientPtr client_itr;
  GlobRegPtr   regxpSrc =NULL, next;

  TRACE ("ENTER delOneClient\n");
  /* on cherche le client dans la liste globale des clients */
  IVY_LIST_ITER (allClients, client_itr,  client_itr->client != client );
  /* si on le trouve */
  if (client_itr != NULL) {
    TRACE ("DEBUG delOneClient %s, client_itr->client=%p, clientRef=%p\n", 
	    client_itr->app_name, client_itr->client, client);




    /* pour chaque regexp source de ce client */
    IVY_LIST_EACH_SAFE (client_itr->srcRegList, regxpSrc, next) {
      /* on met a jour la liste des clients associee a la regexp source */
      delRegexpForOneClient (regxpSrc->str_regexp,  client_itr, regxpSrc->id);
      /* on libere la memoire associee a la regexp source */
      if (regxpSrc->str_regexp != NULL) {
	free (regxpSrc->str_regexp);
	regxpSrc->str_regexp = NULL;
      }
    }


    /* on libere la liste de regexp source */
    IVY_LIST_EMPTY (client_itr->srcRegList);
    /* on enleve l'entree correspondant a ce client dans la liste globale */
    IVY_LIST_REMOVE (allClients, client_itr);
  }
}


static char delRegexpForOneClient (const char *regexp, const IvyClientPtr client, int id) 
{  
  IvyClientPtr client_itr = NULL;
  GlobRegPtr   regxpSrc = NULL, next = NULL;
  char removed = 0;

  TRACE ("ENTER delRegexpForOneClient id=%d\n", id);
  
  /* on enleve du dictionnaire */

  delRegexpForOneClientFromDictionary (regexp, client);

  /* on enleve de la liste globale */
  /* recherche du client */
  IVY_LIST_ITER (allClients, client_itr,  client_itr->client != client->client );
  if (client_itr != NULL) {
    /* pour chaque regexp source de ce client */
    IVY_LIST_EACH_SAFE (client_itr->srcRegList, regxpSrc, next) {
      /* si on trouve notre regexp, on la supprime */
      if (regxpSrc->id == id) {
	removed = 1;
	if (regxpSrc->str_regexp != NULL) {
	  free (regxpSrc->str_regexp);
	  regxpSrc->str_regexp = NULL;
	}
	TRACE ("DBG> IVY_LIST_REMOVE (%p, %p)\n", client_itr->srcRegList, regxpSrc);
	IVY_LIST_REMOVE (client_itr->srcRegList, regxpSrc);  
      }
    }
  }
  return (removed);
}

static void  addOrChangeRegexp (const char* regexp, IvyClientPtr client, int id,
				IvyBinding ivyBind)
{
  MsgSndDictPtr msgSendDict = NULL;
  IvyClientPtr  client_itr = NULL;

  //  printf ("ENTER addOrChangeRegexp\n");
 /* on teste si la regexp existe deja et si il faut faire un changeRegexp */
  HASH_FIND_STR(messSndByRegexp, regexp, msgSendDict);
  /* la regexp n'existe pas du tout */
  if (msgSendDict == NULL) {
    addRegexp (regexp, client, id, ivyBind);
  } else {
    /* la regexp existe, mais l'id existe elle pour le client */
    IVY_LIST_ITER( msgSendDict->clientList, client_itr, ( client_itr->client != client->client));
    if (( client_itr != NULL) &&  (client_itr->id == id)) {
      /* si oui on fait un change regexp */
      changeRegexp (regexp, client, id, ivyBind);
    } else {
      /* si non on fait un add regexp */
      addRegexp (regexp, client, id, ivyBind);
    }
  }
}


static void addRegexp (const char* regexp, IvyClientPtr client, int id,
				   IvyBinding ivyBind) 
{
  IvyClientPtr client_itr = NULL;
  GlobRegPtr   regxpSrc = NULL;


  //  printf ("ENTER addRegexp\n");

  /* on ajoute au dictionnaire */
  addRegexpToDictionary (regexp, client, id, ivyBind);

  /* on ajoute a la liste globale */
  /* recherche du client */
  IVY_LIST_ITER (allClients, client_itr,  client_itr->client != client->client );

  /* si le client n'existe pas, faut le creer */
  if (client_itr == NULL) {
/*     IVY_LIST_ADD_START (allClients, client_itr); */
/*     client_itr->app_name = strdup (client->app_name); */
/*     client_itr->app_port = client->app_port; */
/*     client_itr->client = client->client; */
/*     client_itr->srcRegList = NULL; */
/*     IVY_LIST_ADD_END (allClients, client_itr); */
    fprintf(stderr, "addRegexp ERROR\n");
  }

  /* on ajoute la regexp à la liste de regexps */
  IVY_LIST_ADD_START (client_itr->srcRegList, regxpSrc);
  regxpSrc->id = id;
  regxpSrc->str_regexp = strdup (regexp);
  IVY_LIST_ADD_END (client_itr->srcRegList, regxpSrc);
  if (application_bind_callback) {
    (*application_bind_callback)( client, application_bind_data, id, regexp, IvyAddBind);
  }
}



static void changeRegexp (const char* regexp, IvyClientPtr client, int id, 
				      IvyBinding ivyBind) 
{
  IvyClientPtr client_itr = NULL;
  GlobRegPtr   regxpSrc = NULL, next = NULL;
  /* on change dans le dictionnaire */
  //  printf ("ENTER changeRegexp\n");
  changeRegexpInDictionary (regexp, client, id , ivyBind);

  /* on change dans la liste globale */
  /* recherche du client */
  IVY_LIST_ITER (allClients, client_itr,  client_itr->client != client->client );
  if (client_itr != NULL) {
    /* pour chaque regexp source de ce client */
    IVY_LIST_EACH_SAFE (client_itr->srcRegList, regxpSrc, next) {
      /* si on trouve notre regexp, on la change */
      if (regxpSrc->id == id) {
	free (regxpSrc->str_regexp);
	regxpSrc->str_regexp = strdup (regexp);
      }
    }
  }
  if (application_bind_callback) {
    (*application_bind_callback)( client, application_bind_data, id, regexp, IvyChangeBind);
  }
}

#ifdef OPENMP
static void regenerateRegPtrArrayCache ()
{
  int count=0;
  MsgSndDictPtr msgSendDict;
  ompDictCache.numPtr = 0;

  for (msgSendDict=messSndByRegexp; msgSendDict != NULL ; 
       msgSendDict=msgSendDict->hh.next) {
    ompDictCache.numPtr++;
  }
  
  if (ompDictCache.numPtr >= ompDictCache.size) {
    ompDictCache.size = (ompDictCache.numPtr*2) + 128;
    ompDictCache.msgPtrArray = realloc (ompDictCache.msgPtrArray, 
					sizeof (MsgSndDictPtr) * ompDictCache.size);
  }


  for (msgSendDict=messSndByRegexp; msgSendDict != NULL ; 
       msgSendDict=msgSendDict->hh.next) {
    ompDictCache.msgPtrArray [count++] = msgSendDict;
  }
}


static void addRegToPtrArrayCache (MsgSndDictPtr newReg)
{
  int count=0;
  MsgSndDictPtr msgSendDict;
  ompDictCache.numPtr = 0;

  for (msgSendDict=messSndByRegexp; msgSendDict != NULL ; 
       msgSendDict=msgSendDict->hh.next) {
    ompDictCache.numPtr++;
  }
  
  if (ompDictCache.numPtr >= ompDictCache.size) {
    ompDictCache.size = (ompDictCache.numPtr*2) + 128;
    ompDictCache.msgPtrArray = realloc (ompDictCache.msgPtrArray, 
					sizeof (MsgSndDictPtr) * ompDictCache.size);
    for (msgSendDict=messSndByRegexp; msgSendDict != NULL ; 
	 msgSendDict=msgSendDict->hh.next) {
      ompDictCache.msgPtrArray [count++] = msgSendDict;
    }
  } else {
    // on ajoute juste le nouveau pointeur
    ompDictCache.msgPtrArray [ompDictCache.numPtr-1] = newReg;
  }
}
#endif // OPENMP
