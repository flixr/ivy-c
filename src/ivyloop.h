/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop handling around select
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *		 Stéphane Chatty <chatty@cena.fr>
 *
 *	$Id$
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 *
 */

#ifndef IVYLOOP_H
#define IVYLOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ivychannel.h"


extern void IvyMainLoop(void(*hook)(void) );
extern void IvyIdle();


#ifdef __cplusplus
}
#endif

#endif

