/*
 *	Ivy unbind Test
 *
 *	Copyright (C) 1997-2004
 *	Centre d'Études de la Navigation Aérienne
 *
 *	Authors: Yannick Jestin <jestin@cena.fr>
 *
 *	Please refer to file ../src/version.h for the
 *	copyright notice regarding this software
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ivyloop.h>
#include <ivy.h>
#define REGEXP "^ub"

void Callback (IvyClientPtr app, void *user_data, int argc, char *argv[]) {
  MsgRcvPtr *ptr = (MsgRcvPtr *) user_data;
  printf ("%s sent unbind message, unbinding to %s\n",
      IvyGetApplicationName(app),REGEXP);
  IvyUnbindMsg(*ptr);
}

int main(int argc, char *argv[]) {
  MsgRcvPtr ptr;
  IvyInit("TestUnbind","TestUnbind Ready",NULL,NULL,NULL,NULL);
  ptr=IvyBindMsg(Callback,&ptr,REGEXP);
  printf("bound to %s\n",REGEXP);
  IvyStart(NULL);
  IvyMainLoop(0);
  return 0;
}
