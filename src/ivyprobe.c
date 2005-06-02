/*
 *	Ivy probe
 *
 *	Copyright (C) 1997-2004
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main and only file
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 * 	         Yannick Jestin <jestin@cena.fr>
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
#define DEFAULT_IVYPROBE_NAME "IVYPROBE"
#define DEFAULT_READY " Ready"
#include "version.h"

#define IVYMAINLOOP

#ifdef XTMAINLOOP
#undef IVYMAINLOOP
#endif
#ifdef GLIBMAINLOOP
#undef IVYMAINLOOP
#endif

#ifdef GLUTMAINLOOP
#undef IVYMAINLOOP
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#ifdef __MINGW32__
#include <regex.h> 
#include <getopt.h>
#endif
#else
#include <sys/time.h>
#include <unistd.h>
#ifdef __INTERIX
extern char *optarg;
extern int optind;
#endif
#ifndef USE_PCRE_REGEX
#include <regex.h> 
#else
#define OVECSIZE 60 /* must be multiple of 3, for regexp return */
#include <pcre.h>
#endif


#endif

#include "ivychannel.h"
#include "ivysocket.h"
#include "ivy.h"
#include "timer.h"
#ifdef XTMAINLOOP
#include "ivyxtloop.h"
#include <X11/Intrinsic.h>
XtAppContext cntx;

#endif

int app_count = 0;
int wait_count = 0;
int fbindcallback = 0;

void DirectCallback(IvyClientPtr app, void *user_data, int id, char *msg )
{
	printf("%s sent a direct message, id=%d, message=%s\n",
	    IvyGetApplicationName(app),id,msg);
}

void BindCallback(IvyClientPtr app, void *user_data,  IvyBindEvent event, char *regexp  )
{
  char *sevent;
  sevent= event==IvyAddBind ? "added" :"removed";
  
  printf("%s has modified his binding, regexp=%s is %s\n",
	    IvyGetApplicationName(app),regexp,sevent);
}

void Callback (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	int i;
	printf ("%s sent %d args:",IvyGetApplicationName(app),argc);
	for  (i = 0; i < argc; i++)
			printf(" '%s'",argv[i]);
	printf("\n");
}

char * Chop(char *arg)
{
  	int len;
	if (arg==NULL) return arg;
	len=strlen(arg)-1;
	if ((*(arg+len))=='\n') *(arg+len)=0;
	return arg;
}

void HandleStdin (Channel channel, HANDLE fd, void *data)
{
	char buf[4096];
	char *line;
	char *cmd;
	char *arg;
	int id;
	IvyClientPtr app;
	int err;
	line = fgets(buf, 4096, stdin);
	if  (!line)	{
		IvyChannelClose (channel);
		IvyStop();
		return;
	}
	if  (*line == '.') {
		cmd = strtok (line, ".: \n");

		if  (strcmp (cmd, "die") == 0) {
			arg = strtok (NULL, " \n");
			if  (arg) {
				app = IvyGetApplication (arg);
				if  (app)
					IvySendDieMsg (app);
					else printf ("No Application %s!!!\n",arg);
			}

		} else if (strcmp(cmd, "dieall-yes-i-am-sure") == 0) {
			arg = IvyGetApplicationList();
			arg = strtok (arg, " \n");
			while  (arg) {
				app = IvyGetApplication (arg);
				if  (app)
					IvySendDieMsg (app);
				else
					printf ("No Application %s!!!\n",arg);
				arg = strtok (NULL, " ");
			}
			
		} else if (strcmp(cmd,  "bind") == 0) {
		  arg = strtok (NULL, "'");
		  if  (arg) {
#ifndef USE_PCRE_REGEX
		    regex_t reg;
		    int err;
		    Chop(arg);
		    if (err=regcomp(&reg,arg,REG_ICASE|REG_EXTENDED)!=0) {
		      char errbuf[4096];
		      regerror (err, &reg, errbuf, 4096);
		      printf("Error compiling '%s', %s, not bound\n", arg, errbuf);
#else
		    pcre *regexp;
		    const char *errbuf;
		    int erroffset;
		    Chop(arg);
		    regexp = pcre_compile(arg, 0,&errbuf,&erroffset,NULL);
		    if (regexp==NULL) {
		      printf("Error compiling '%s', %s, not bound\n", arg, errbuf);
#endif
		    } else {
		      IvyBindMsg (Callback, NULL, Chop(arg));
		    }
		  }

		} else if  (strcmp(cmd,  "where") == 0) {
			arg = strtok (NULL, " \n");
			if  (arg) {
				app = IvyGetApplication (arg);
				if  (app)
					printf ("Application %s on %s\n",arg, IvyGetApplicationHost (app));
					else printf ("No Application %s!!!\n",arg);
			}
		} else if  (strcmp(cmd, "direct") == 0) {
			arg = strtok (NULL, " \n");
			if  (arg) {
				app = IvyGetApplication (arg);
				if  (app) {
					arg = strtok (NULL, " ");
					id = atoi (arg) ;
					arg = strtok (NULL, "'");
					IvySendDirectMsg (app, id, Chop(arg));
				} else
					printf ("No Application %s!!!\n",arg);
			}
			
		} else if  (strcmp(cmd, "who") == 0) {
			printf("Apps: %s\n", IvyGetApplicationList());

		} else if  (strcmp(cmd, "help") == 0) {
			fprintf(stderr,"Commands list:\n");
			printf("	.help					 - this help\n");
			printf("	.quit					 - terminate this application\n");
			printf("	.die appname			 - send die msg to appname\n");
			printf("	.dieall-yes-i-am-sure	 - send die msg to all applis\n");
			printf("	.direct appname	id 'arg' - send direct msg to appname\n");
			printf("	.where appname			 - on which host is appname\n");
			printf("	.bind 'regexp'			 - add a msg to receive\n");
			printf("	.bindcall				 - add callback to binding\n");
			printf("	.who					 - who is on the bus\n");
		} else if  (strcmp(cmd, "bindcall") == 0) {
		  if (!fbindcallback) {
		    IvySetBindCallback(BindCallback, NULL);
		    fbindcallback=1;
		  } else {
			IvySetBindCallback(NULL, NULL);
		    fbindcallback=0;
		  }
		} else if  (strcmp(cmd, "quit") == 0) {
			exit(0);
		}
	} else {
		cmd = strtok (buf, "\n");
		if ( cmd )
		{
		err = IvySendMsg (cmd);
		printf("-> Sent to %d peer%s\n", err, err == 1 ? "" : "s");
		}
	}
}

void ApplicationCallback (IvyClientPtr app, void *user_data, IvyApplicationEvent event)
{
	char *appname;
	char *host;
	char **msgList;
	appname = IvyGetApplicationName (app);
	host = IvyGetApplicationHost (app);
	switch  (event)  {

	case IvyApplicationConnected:
		app_count++;
		printf("%s connected from %s\n", appname,  host);
/*		printf("Application(%s): Begin Messages\n", appname);*/
		msgList = IvyGetApplicationMessages (app);
		while (*msgList )
			printf("%s subscribes to '%s'\n",appname,*msgList++);
/*		printf("Application(%s): End Messages\n",appname);*/
		if  (app_count == wait_count)

		IvyChannelOpen (0, NULL, NULL, HandleStdin);
		break;

	case IvyApplicationDisconnected:
		app_count--;
		printf("%s disconnected from %s\n", appname,  host);
		break;

	default:
		printf("%s: unkown event %d\n", appname, event);
		break;
	}
}


#ifndef XTMAINLOOP
void TimerCall(TimerId id, void *user_data, unsigned long delta)
{
	printf("Timer callback: %d delta %lu ms\n", (int)user_data, delta);
	IvySendMsg ("TEST TIMER %d", (int)user_data);
	/*if  ((int)user_data == 5) TimerModify (id, 2000);*/
}
#endif
#ifdef GLUTMAINLLOP
void
display(void)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();
}
#endif

int main(int argc, char *argv[])
{
	int c;
	int timer_test = 0;
	char busbuf [1024] = "";
	const char* bus = 0;
	char agentnamebuf [1024] = "";
	const char* agentname = DEFAULT_IVYPROBE_NAME;
	char agentready [1024] = "";
	const char* helpmsg =
	  "[options] [regexps]\n\t-b bus\tdefines the Ivy bus to which to connect to, defaults to 127:2010\n\t-t\ttriggers the timer test\n\t-n name\tchanges the name of the agent, defaults to IVYPROBE\n\t-v\tprints the ivy relase number\n\nregexp is a Perl5 compatible regular expression (see ivyprobe(1) and pcrepattern(3) for more info\nuse .help within ivyprobe\n\t-s bindcall\tactive the interception of regexp's subscribing or unscribing\n";
	while ((c = getopt(argc, argv, "vn:d:b:w:t:s")) != EOF)
			switch (c) {
			case 'b':
				strcpy (busbuf, optarg);
				bus = busbuf;
				break;
			case 'w':
				wait_count = atoi(optarg) ;
				break;
			case 'n':
				strcpy(agentnamebuf, optarg);
				agentname=agentnamebuf;
			case 'v':
				printf("ivy c library version %d.%d\n",IVYMAJOR_VERSION,IVYMINOR_VERSION);
				break;
			case 't':
			        timer_test = 1;
			        break;
			case 's':
			        IvySetBindCallback(BindCallback, NULL);
			        fbindcallback=1;
				break;
			default:
				printf("usage: %s %s",argv[0],helpmsg);
				exit(1);
			}
	sprintf(agentready,"%s Ready",agentname);

	/* Mainloop management */
#ifdef XTMAINLOOP
	/*XtToolkitInitialize();*/
	cntx = XtCreateApplicationContext();
	IvyXtChannelAppContext (cntx);
#endif
#ifdef GLUTMAINLLOOP
	glutInit(&argc, argv);
	glutCreateWindow("IvyProbe Test");
	glClearColor(0.49, 0.62, 0.75, 0.0);
	glutDisplayFunc(display);
#endif
	IvyInit (agentname, agentready, ApplicationCallback,NULL,NULL,NULL);
	IvySetApplicationPriority( 0 ); /* lower priority */
	IvyBindDirectMsg( DirectCallback,NULL);
	for  (; optind < argc; optind++)
		IvyBindMsg (Callback, NULL, argv[optind]);

	if  (wait_count == 0)
#ifndef WIN32
/* Stdin not compatible with select , select only accept socket */
	IvyChannelOpen (0, NULL, NULL, HandleStdin);
#endif


	IvyStart (bus);

	if  (timer_test) {
#ifdef IVYMAINLOOP
		TimerRepeatAfter (TIMER_LOOP, 1000, TimerCall, (void*)1);
		TimerRepeatAfter (5, 5000, TimerCall, (void*)5);
#endif
	}

#ifdef XTMAINLOOP
	XtAppMainLoop (cntx);
#endif

	IvyMainLoop (0);

	return 0;
}
