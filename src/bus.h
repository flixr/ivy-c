#ifndef _BUS_H
#define _BUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* numero par default du bus */

#define DEFAULT_BUS 2010

typedef struct _clnt_lst *BusClientPtr;

typedef enum { BusApplicationConnected, BusApplicationDisconnected } BusApplicationEvent;

extern void BusDefaultApplicationCallback( BusClientPtr app, void *user_data, BusApplicationEvent event ) ;
/* callback callback appele sur connection deconnection d'une appli */
typedef void (*BusApplicationCallback)( BusClientPtr app, void *user_data, BusApplicationEvent event ) ;
/* callback appele sur reception de die */
typedef void (*BusDieCallback)( BusClientPtr app, void *user_data, int id ) ;

/* callback appele sur reception de messages normaux */
typedef void (*MsgCallback)( BusClientPtr app, void *user_data, int argc, char **argv ) ;
/* callback appele sur reception de messages directs */
typedef void (*MsgDirectCallback)( BusClientPtr app, void *user_data, int id, char *msg ) ;

/* identifiant d'une expression reguliere ( Bind/Unbind ) */
typedef struct _msg_rcv *MsgRcvPtr;

/* filtrage des regexps */
void BusClasses( int argc, const char **argv);

void BusInit(
			 const char *AppName,				/* nom de l'application */
			 unsigned short busnumber,			/* numero de bus ( port UDP ) */
			 const char *ready,					/* ready Message peut etre NULL */
			 BusApplicationCallback callback,	/* callback appele sur connection deconnection d'une appli */
			 void *data,						/* user data passe au callback */
			 BusDieCallback die_callback,		/* last change callback before die */
			 void *die_data	);					/* user data */

void BusStart();								/* emission du bonjour */

/* query sur les applications connectees */
char *GetApplicationName( BusClientPtr app );
char *GetApplicationHost( BusClientPtr app );
BusClientPtr GetApplication( char *name );
char *GetApplicationList();
char **GetApplicationMessages( BusClientPtr app);
/* demande de reception d'un message */

MsgRcvPtr BindMsg( MsgCallback callback, void *user_data, const char *fmt_regexp, ... ); /* avec sprintf prealable */
void UnbindMsg( MsgRcvPtr id );

/* emmission d'un message d'erreur */
void SendError( BusClientPtr app, int id, const char *fmt, ... );

/* emmission d'un message die pour terminer l'application */
void SendDieMsg( BusClientPtr app );

/* emmission d'un message retourne le nb effectivement emis */

int SendMsg( const char *fmt_message, ... );		/* avec sprintf prealable */

/* Message Direct Inter-application */

void BindDirectMsg( MsgDirectCallback callback, void *user_data);
void SendDirectMsg( BusClientPtr app, int id, char *msg );

#ifdef __cplusplus
}
#endif

#endif