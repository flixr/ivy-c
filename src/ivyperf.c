/*
 *	Ivy perf mesure le temp de round trip
 *
 *	Copyright (C) 1997-2004
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Main and only file
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.fr>
 * 	         Yannick Jestin <jestin@cena.fr>
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#include "version.h"
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
#endif


#include "ivysocket.h"
#include "ivy.h"
#include "timer.h"
#include "ivyloop.h"
#define MILLISEC 1000.0

static double currentTime()
{
        double current;
#ifdef WIN32
        current = GetTickCount();
#else
        struct timeval stamp;
        gettimeofday( &stamp, NULL );
        current = (double)stamp.tv_sec * MILLISEC + (double)(stamp.tv_usec/MILLISEC);
#endif
        return  current;
}

void Reply (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	IvySendMsg ("pong ts=%s tr=%f", *argv, currentTime());
}
void Pong (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	double current = currentTime();
	double ts = atof( *argv++ );
	double tr = atof( *argv++ );
	double roundtrip1 = tr-ts;
	double roundtrip2 = current - tr;
	double roundtrip3 = current - ts;
	fprintf(stderr,"roundtrip %f %f %f \n", roundtrip1, roundtrip2, roundtrip3 );
}

void TimerCall(TimerId id, void *user_data, unsigned long delta)
{
	int count = IvySendMsg ("ping ts=%f", currentTime() );
	if ( count == 0 ) fprintf(stderr, "." );
}


int main(int argc, char *argv[])
{
	

	/* Mainloop management */

	IvyInit ("IvyPerf", "IvyPerf ready", NULL,NULL,NULL,NULL);
	
	IvyBindMsg (Reply, NULL, "^ping ts=(.*)");
	IvyBindMsg (Pong, NULL, "^pong ts=(.*) tr=(.*)");

	IvyStart (0);

	TimerRepeatAfter (TIMER_LOOP, 200, TimerCall, (void*)1);
	

	IvyMainLoop (0);
	return 0;
}