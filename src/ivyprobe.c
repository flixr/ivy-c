/*
 *	Ivy probe
 *
 *	Copyright (C) 1997-1999
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main and only file
 *
 *	Authors: François-Régis Colin <colin@cenatoulouse.dgac.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void Callback (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	int i;
	printf ("%s sent ",IvyGetApplicationName(app));
	for  (i = 0; i < argc; i++)
			printf(" '%s'",argv[i]);
	printf("\n");
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
#ifdef XTMAINLOOP
		IvyXtChannelClose (channel);
#else
		IvyChannelClose (channel);
		IvyStop();
#endif
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
				IvyBindMsg (Callback, NULL, arg);
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
					IvySendDirectMsg (app, id, arg);
				} else
					printf ("No Application %s!!!\n",arg);
			}
			
		} else if  (strcmp(cmd, "who") == 0) {
			printf("Apps: %s\n", IvyGetApplicationList());

		} else if  (strcmp(cmd, "help") == 0) {
			fprintf(stderr,"Commands list:\n");
			printf("	.help				- this help\n");
			printf("	.quit				- terminate this application\n");
			printf("	.die appname			- send die msg to appname\n");
			printf("	.direct appname	id 'arg'	- send direct msg to appname\n");
			printf("	.where appname			- on which host is appname\n");
			printf("	.bind 'regexp'			- add a msg to receive\n");
			printf("	.who				- who is on the bus\n");
		} else if  (strcmp(cmd, "quit") == 0) {
			exit(0);
		}
	} else {
		cmd = strtok (buf, "\n");
		err = IvySendMsg (cmd);
		printf("-> Sent to %d peer%s\n", err, err == 1 ? "" : "s");
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
#ifdef XTMAINLOOP
		IvyXtChannelSetUp (0, NULL, NULL, HandleStdin);
#else
		IvyChannelSetUp (0, NULL, NULL, HandleStdin);
#endif
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


int main(int argc, char *argv[])
{
	unsigned short bport = DEFAULT_BUS;
	int c;
	int timer_test = 0;
	char dbuf [1024] = "";
	const char* domains = 0;
	while ((c = getopt(argc, argv, "d:b:w:t")) != EOF)
			switch (c) {
			case 'b':
				bport = atoi(optarg) ;
				break;
			case 'd':
				if (domains)
					strcat (dbuf, ",");
				else
					domains = dbuf;
				strcat (dbuf, optarg);
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
	IvyXtChannelAppContext (cntx);
#endif
	IvyInit ("IVYPROBE", bport, "IVYPROBE READY", ApplicationCallback,NULL,NULL,NULL);
	for  (; optind < argc; optind++)
		IvyBindMsg (Callback, NULL, argv[optind]);

	if  (wait_count == 0)
#ifdef XTMAINLOOP
		IvyXtChannelSetUp (0, NULL, NULL, HandleStdin);
#else
		IvyChannelSetUp (0, NULL, NULL, HandleStdin);
#endif

	IvyStart (domains);

	if  (timer_test) {
#ifndef XTMAINLOOP
		TimerRepeatAfter (TIMER_LOOP, 1000, TimerCall, (void*)1);
		TimerRepeatAfter (5, 5000, TimerCall, (void*)5);
#endif
	}

#ifdef XTMAINLOOP
	XtAppMainLoop (cntx);
#else
	IvyMainLoop (0);
#endif
	return 0;
}
