#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#ifdef XTMAINLOOP
#include "ivyxtloop.h"
#else
#include "ivyloop.h"
#endif
#include "ivysocket.h"
#include "ivy.h"
#include "timer.h"
#ifdef XTMAINLOOP
#include <X11/Intrinsic.h>
XtAppContext cntx;
#endif

int app_count = 0;
int wait_count = 0;

void Callback( BusClientPtr app, void *user_data, int argc, char *argv[])
{
	int i;
	printf(" %s Called function %d args:",GetApplicationName(app),argc);
	for ( i = 0; i < argc; i++ )
			printf(" '%s'",argv[i]);
	printf("\n");
}
void HandleStdin( Channel channel, HANDLE fd, void *data)
{
	char buf[4096];
	char *line;
	char *cmd;
	char *arg;
	int id;
	BusClientPtr app;
	int err;
	line = gets( buf);
	if ( !line )
		{
#ifdef XTMAINLOOP
		BusXtChannelClose( channel );
#else
		BusLoopChannelClose( channel );
		BusLoopChannelStop();
#endif
		return;
		}
	if ( *line == '.' )
	{
		cmd = strtok( line, ".: ");
		if ( strcmp(cmd, "die" ) == 0 )
			{
			arg = strtok( NULL, " " );
			if ( arg )
				{
				app = GetApplication( arg );
				if ( app )
					SendDieMsg( app );
					else printf( "No Application %s!!!\n",arg);
				}
			}
		if ( strcmp(cmd, "dieall-yes-i-am-sure") == 0 )
                        {
			arg = GetApplicationList();
			arg = strtok( arg, " " );
			while ( arg )
				{
				app = GetApplication( arg );
                                if ( app )
                                        SendDieMsg( app );
                                        else printf( "No Application %s!!!\n",arg);
                                arg = strtok( NULL, " ");
				}
			
                        }
                
		if ( strcmp(cmd,  "bind" ) == 0 )
			{
			arg = strtok( NULL, "'" );
			if ( arg )
				{
				BindMsg( Callback, NULL, arg );
				}
			}
		if ( strcmp(cmd,  "where" ) == 0 )
			{
			arg = strtok( NULL, " " );
			if ( arg )
				{
				app = GetApplication( arg );
				if ( app )
					printf( "Application %s on %s\n",arg, GetApplicationHost( app ));
					else printf( "No Application %s!!!\n",arg);
				}
			}
		if ( strcmp(cmd, "direct" ) == 0 )
			{
			arg = strtok( NULL, " " );
			if ( arg )
				{
				app = GetApplication( arg );
				if ( app )
					{
					arg = strtok( NULL, " " );
					id = atoi( arg ) ;
					arg = strtok( NULL, "'" );
					SendDirectMsg( app, id, arg );
					}
				else printf( "No Application %s!!!\n",arg);
				}
			
			}
		if ( strcmp(cmd, "who") == 0 )
			{
			printf("Apps: %s\n", GetApplicationList());
			}
		if ( strcmp(cmd, "help") == 0 )
			{
			printf("Commands list:\n");
			printf("	.help					 - this help\n");
			printf("	.quit					 - terminate this application\n");
			printf("	.die appname			 - send die msg to appname\n");
			printf("	.direct appname	id 'arg' - send direct msg to appname\n");
			printf("	.where appname			 - on which host is appname\n");
			printf("	.bind 'regexp'			 - add a msg to receive\n");
			printf("	.who					 - who is on the bus\n");
			}
		if ( strcmp(cmd, "quit") == 0 )
			{
			exit(0);
			}
	}
	else 
	{
	err = SendMsg( buf );
	printf("Sent:%d\n",err);
	}
}

void ApplicationCallback( BusClientPtr app, void *user_data, BusApplicationEvent event)
{
	char *appname;
	char *host;
	char **msgList;
	appname = GetApplicationName( app );
	host = GetApplicationHost( app );
	switch ( event )  {
	case BusApplicationConnected:
		app_count++;
		printf("Application(%s): ready on %s\n", appname,  host);
		printf("Application(%s): Begin Messages\n", appname);
		msgList = GetApplicationMessages( app );
		while( *msgList  )
			printf("Application(%s): Receive '%s'\n",appname,*msgList++);
		printf("Application(%s): End Messages\n",appname);
		if ( app_count == wait_count )
#ifdef XTMAINLOOP
		BusXtChannelSetUp( 0, NULL, NULL, HandleStdin);
#else
		BusLoopChannelSetUp( 0, NULL, NULL, HandleStdin);
#endif
		break;
	case BusApplicationDisconnected:
		app_count--;
		printf("Application(%s): bye on %s\n", appname,  host);
		break;
	default:
		printf("Application(%s): unkown event %d\n", appname, event);
		break;
	}

}
#ifndef XTMAINLOOP
void TimerCall(TimerId id, void *user_data, unsigned long delta)
{
	printf("Timer callback: %d delta %lu ms\n", (int)user_data, delta );
	SendMsg( "TEST TIMER %d", (int)user_data);
	/*if ( (int)user_data == 5 ) TimerModify( id, 2000 );*/
}
#endif
int main(int argc, char *argv[])
{
	
	unsigned short bport = DEFAULT_BUS;
	int c;
	int timer_test = 0;
	while ((c = getopt(argc, argv, "b:w:t")) != EOF)
			switch (c)
					{
					case 'b':
							bport = atoi(optarg) ;
							break;
					case 'w':
							wait_count = atoi(optarg) ;
							break;
					case 't':
							timer_test = 1;
							break;
					}
	/* Mainloop management */
#ifdef XTMAINLOOP
	/*XtToolkitInitialize();*/
	cntx = XtCreateApplicationContext();
	BusXtChannelAppContext( cntx );
	BusSetChannelManagement( BusXtChannelInit, BusXtChannelSetUp, BusXtChannelClose );
#else
	BusSetChannelManagement( BusLoopChannelInit, BusLoopChannelSetUp, BusLoopChannelClose );
#endif
	BusInit("TEST",bport,"TEST READY",ApplicationCallback,NULL,NULL,NULL);
	for ( ; optind < argc; optind++ )
			BindMsg( Callback, NULL, argv[optind] );
	if ( wait_count == 0 )
#ifdef XTMAINLOOP
		BusXtChannelSetUp( 0, NULL, NULL, HandleStdin);
#else
		BusLoopChannelSetUp( 0, NULL, NULL, HandleStdin);
#endif
	BusStart( );
	if ( timer_test )
		{
#ifndef XTMAINLOOP
		TimerRepeatAfter( TIMER_LOOP, 1000, TimerCall, (void*)1 );
		TimerRepeatAfter( 5, 5000, TimerCall, (void*)5 );
#endif
		}
#ifdef XTMAINLOOP
	XtAppMainLoop(cntx);
#else
	BusLoopChannelMainLoop(NULL);
#endif
	return 0;
}
