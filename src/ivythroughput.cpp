/*
 *	IvyThroughput
 *
 *	Copyright (C) 2008
 *	Centre d'Études de la Navigation Aérienne
 */
 
/*

  TODO : ° utiliser la commande script pour logger le nb de
	 ° bug pourquoi ça marche plus si plus d'un client ?

 */


// g++ ivythroughput.cpp -o ivythroughput -L/usr/local/lib64/ -Wl,-rpath,/usr/local/lib64/ -livy -lpcrecpp

/* SCENARIO  
   
  ° traitement des options :
    -v (affi version) -b bus, -r regexp file, -m message file, -n : nombre de recepteurs
    -t [type de test => ml (memory leak), tp (throughput)

    test memory leak
  ° fork d'un emetteur et d'un recepteur : le recepteur s'abonne à toutes les regexps,
    se desabonne, se reabonne etc etc en boucle : on teste que l'empreinte mémoire
    de l'emetteur ne grossisse pas

    test throughput :
  ° fork d'un emetteur et d'un ou plusieurs recepteurs : les recepteurs s'abonnent à toutes les regexps
  ° l'emetteur envoie en boucle tous les messages du fichier de message
  ° l'emetteur note le temps d'envoi des messages
  ° l'emetteur envoie un die all et quitte
*/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <signal.h>

#include "ivysocket.h"
#include "ivy.h"
#include "timer.h"
#include "ivyloop.h"
#include <pcrecpp.h>

#include <string>
#include <list>
#include <map>

typedef std::list<std::string>  ListOfString;
typedef std::list<pid_t> ListOfPid;
typedef std::map<unsigned int, bool> MapUintToBool;
typedef struct {
  unsigned int currentBind;
  unsigned int totalBind;
} InfoBind;
typedef std::map<string, InfoBind> MapBindByClnt;

#define MILLISEC 1000.0

typedef enum  {memoryLeak, throughput} KindOfTest ;


extern char *optarg;
extern int   optind, opterr, optopt;

void recepteur (const char* bus, KindOfTest kod, unsigned int inst, 
		const ListOfString& regexps);
void emetteur (const char* bus, KindOfTest kod, int testDuration, 
	       const ListOfString& messages, int regexpSize);

bool getMessages (const char*fileName, ListOfString &messages);
bool getRegexps (const char*fileName, ListOfString &regexps);

double currentTime();
void binCB( IvyClientPtr app, void *user_data, int id, char* regexp,  IvyBindEvent event ) ;
void binCBR( IvyClientPtr app, void *user_data, int id, char* regexp,  IvyBindEvent event ) ;
void stopCB (TimerId id, void *user_data, unsigned long delta);
void sendAllMessageCB (TimerId id, void *user_data, unsigned long delta);
void recepteurReadyCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void otherRecepteurReadyCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void recepteurCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void startOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void endOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void aliveCB (TimerId id, void *user_data, unsigned long delta);

unsigned int nbMess=0, nbReg=0, numClients =1, globalInst;
MapUintToBool   recReady;
KindOfTest   kindOfTest = throughput;
bool	     regexpAreUniq = false;

int main(int argc, char *argv[])
{
  int c;
  int testDuration = 10;
  char *bus ;
  char  regexpFile[1024] = "testivy/regexp.txt";
  char  messageFile[1024] = "testivy/plantageradargl.ivy";
  ListOfString messages, regexps;
  pid_t        pid;
  ListOfPid    recPid;
  
  const char* helpmsg =
    "[options] \n"
    "\t -b bus\tdefines the Ivy bus to which to connect to, defaults to 127:2010\n"
    "\t -v \t prints the ivy relase number\n\n"
    "\t -t \t type of test :  ml (memory leak) or tp (throughput)\n"
    "\t -r \t regexfile\tread list of regexp's from file\n"
    "\t -p \t each client will prepend regexp with uniq string to "
             "simulate N clients with differents regexps\n"
    "\t -m \t messageFile\tread list of messages from file\n"
    "\t -n \t number of clients\n" 
    "\t -d \t duration of the test in seconds\n" ;


  if (getenv("IVYBUS") != NULL) {
    bus = strdup (getenv("IVYBUS"));
  } else {
    bus = strdup ("127.0.0.1:2000") ;
  }


  while ((c = getopt(argc, argv, "vpb:r:m:n:t:d:")) != EOF)
    switch (c) {
    case 'b':
      strcpy (bus, optarg);
      break;
    case 'v':
      printf("ivy c library version %d.%d\n",IVYMAJOR_VERSION, IVYMINOR_VERSION);
      break; 
    case 'p':
      regexpAreUniq = true;
      break; 
    case 't':
      kindOfTest = (strncasecmp (optarg, "ml", 2) == 0) ? memoryLeak : throughput;
      break;
    case 'r':
      strcpy (regexpFile, optarg);
      break;
    case 'm':
      strcpy (messageFile, optarg);
      break;
    case 'n':
      numClients = atoi (optarg);
      break;
    case 'd':
      testDuration = atoi (optarg);
      break;
    default:
      printf("usage: %s %s",argv[0],helpmsg);
      exit(1);
    }

  if (!getRegexps (regexpFile, regexps)) 
    {return (1);};

  if (kindOfTest != memoryLeak) {
    if (!getMessages (messageFile, messages)) 
      {return (1);};
  }

#ifdef DEBUG 
  ListOfString::iterator iter;  
  for (iter=regexps.begin(); iter != regexps.end(); iter++) {
    std::cout << "DBG> |" << *iter << "|\n";
  }

  for (iter=messages.begin(); iter != messages.end(); iter++) {
    std::cout << "DBG> |" << *iter << "|\n";
  }
#endif // DEBUG



  for (unsigned int i=0; i< numClients; i++) {
    if ((pid = fork ()) == 0) {
      // fils
      recepteur (bus, kindOfTest, i, regexps);
      exit (0);
    } else {
      recPid.push_back (pid);
      recReady[i]=false;
    }
  }
  
  emetteur  (bus, kindOfTest, testDuration, messages, regexps.size());

  ListOfPid::iterator  iter;  
  for (iter=recPid.begin(); iter != recPid.end(); iter++) {
    kill (*iter, SIGTERM);
  }

  for (iter=recPid.begin(); iter != recPid.end(); iter++) {
    waitpid (*iter, NULL, 0);
  }
  
  

  return (0);
}




/*
#                                           _      _
#                                          | |    | |
#                  ___   _ __ ___     ___  | |_   | |_     ___   _   _   _ __
#                 / _ \ | '_ ` _ \   / _ \ | __|  | __|   / _ \ | | | | | '__|
#                |  __/ | | | | | | |  __/ \ |_   \ |_   |  __/ | |_| | | |
#                 \___| |_| |_| |_|  \___|  \__|   \__|   \___|  \__,_| |_|
*/
void emetteur (const char* bus, KindOfTest kod, int testDuration, 
	       const ListOfString& messages, int regexpSize)
{
  printf ("DBG> emetteur start, pid=%d\n", getpid());
  IvyInit ("IvyThroughputEmit", "IvyThroughputEmit Ready", NULL,NULL,NULL,NULL);
  //  double origin = currentTime();


  IvySetBindCallback (binCB, (void *) regexpSize);
  IvyBindMsg (recepteurReadyCB, (void *) &messages, 
	      "^IvyThroughputReceive_(\\d+)\\s+(Bindone|Ready)");

  TimerRepeatAfter (1, testDuration *1000, stopCB, NULL);

  IvyStart (bus);
  IvyMainLoop ();
}



/*
#                                             _ __    _
#                                            | '_ \  | |
#                 _ __    ___    ___    ___  | |_) | | |_     ___   _   _   _ __
#                | '__|  / _ \  / __|  / _ \ | .__/  | __|   / _ \ | | | | | '__|
#                | |    |  __/ | (__  |  __/ | |     \ |_   |  __/ | |_| | | |
#                |_|     \___|  \___|  \___| |_|      \__|   \___|  \__,_| |_|
*/
void recepteur (const char* bus, KindOfTest kod, unsigned int inst, 
		const ListOfString& regexps)
{
  std::string agentName = "IvyThroughputReceive_";
  std::stringstream stream ;
  stream << inst;
  agentName +=  stream.str();
  std::string agentNameReady (agentName + " Ready");
  //double origin = currentTime();
  globalInst = inst;
  static int nbOtherReady = numClients -1;

  usleep (100 * 1000);

  printf ("DBG> recepteur_%d start, pid=%d\n", inst, getpid());
  IvyInit (agentName.c_str(), agentNameReady.c_str(), NULL,NULL,NULL,NULL);
  
  unsigned int debugInt = 0;
  ListOfString::const_iterator  iter;  
  for (iter=regexps.begin(); iter != regexps.end(); iter++) {
    debugInt++;
    string reg = *iter;
    if (regexpAreUniq) { (reg += " ") += stream.str();}
    IvyBindMsg (recepteurCB, (void *) inst, reg.c_str());
  }
  IvyBindMsg (startOfSeqCB, NULL, "^start(OfSequence)");
  IvyBindMsg (endOfSeqCB, NULL, "^end(OfSequence)");
  IvyBindMsg (otherRecepteurReadyCB, (void *) &nbOtherReady, 
	      "^IvyThroughputReceive_(\\d+)\\s+Ready");


  TimerRepeatAfter (TIMER_LOOP, 1000, aliveCB, NULL);

  IvyStart (bus);
  IvyMainLoop ();
}

// ===========================================================================



/*
#                  __ _          _      __  __                                 __ _
#                 / _` |        | |    |  \/  |                               / _` |
#                | (_| |   ___  | |_   | \  / |   ___   ___    ___     __ _  | (_| |   ___
#                 \__, |  / _ \ | __|  | |\/| |  / _ \ / __|  / __|   / _` |  \__, |  / _ \
#                  __/ | |  __/ \ |_   | |  | | |  __/ \__ \  \__ \  | (_| |   __/ | |  __/
#                 |___/   \___|  \__|  |_|  |_|  \___| |___/  |___/   \__,_|  |___/   \___|
*/

bool getMessages (const char*fileName, ListOfString &messages)
{
  FILE *infile;
  char buffer [1024*64];
  pcrecpp::RE pcreg ("\"(.*)\"$");
  string  aMsg;

  infile = fopen(fileName, "r");
  if (!infile) {
    fprintf (stderr, "impossible d'ouvrir %s en lecture\n", fileName);
    return false;
  }
  
  while (fgets (buffer, sizeof (buffer), infile) != NULL) {
    if (pcreg.PartialMatch (buffer, &aMsg)) {
      messages.push_back (aMsg);
      nbMess++;
    } 
  }
  fclose (infile);
  return (true);
}


/*
#                  __ _          _      _____            __ _                  _ __
#                 / _` |        | |    |  __ \          / _` |                | '_ \
#                | (_| |   ___  | |_   | |__) |   ___  | (_| |   ___  __  __  | |_) |  ___
#                 \__, |  / _ \ | __|  |  _  /   / _ \  \__, |  / _ \ \ \/ /  | .__/  / __|
#                  __/ | |  __/ \ |_   | | \ \  |  __/   __/ | |  __/  >  <   | |     \__ \
#                 |___/   \___|  \__|  |_|  \_\  \___|  |___/   \___| /_/\_\  |_|     |___/
*/
bool getRegexps (const char*fileName, ListOfString &regexps)
{
  FILE *infile;
  char buffer [1024*64];
  pcrecpp::RE pcreg ("add regexp \\d+ : (.*)$");
  string  aMsg;

  infile = fopen(fileName, "r");
  if (!infile) {
    fprintf (stderr, "impossible d'ouvrir %s en lecture\n", fileName);
    return false;
  }
  
  while (fgets (buffer, sizeof (buffer), infile) != NULL) {
    if (pcreg.PartialMatch (buffer, &aMsg)) {
      regexps.push_back (aMsg);
      nbReg++;
    } 
  }
  fclose (infile);

  return (true);
}



/*
#                                              _______   _
#                                             |__   __| (_)
#                  ___   _   _   _ __   _ __     | |     _    _ __ ___     ___
#                 / __| | | | | | '__| | '__|    | |    | |  | '_ ` _ \   / _ \
#                | (__  | |_| | | |    | |       | |    | |  | | | | | | |  __/
#                 \___|  \__,_| |_|    |_|       |_|    |_|  |_| |_| |_|  \___|
*/
double currentTime()
{
  double current;
  
  struct timeval stamp;
  gettimeofday( &stamp, NULL );
  current = (double)stamp.tv_sec * MILLISEC + (double)(stamp.tv_usec/MILLISEC);
  return  current;
}


/*
#                 _       _             _____   ____
#                | |     (_)           / ____| |  _ \
#                | |__    _    _ __   | |      | |_) |
#                | '_ \  | |  | '_ \  | |      |  _ <
#                | |_) | | |  | | | | | |____  | |_) |
#                |_.__/  |_|  |_| |_|  \_____| |____/
*/
void binCB( IvyClientPtr app, void *user_data, int id, char* regexp,  IvyBindEvent event ) 
{
  string appName = IvyGetApplicationName( app );
  static MapBindByClnt bindByClnt;

  if (bindByClnt.find (appName) == bindByClnt.end()) {
    (bindByClnt[appName]).currentBind = 0;
    (bindByClnt[appName]).totalBind = (unsigned long) user_data;
  }

  switch ( event )
    {
    case IvyAddBind:
      (bindByClnt[appName]).currentBind ++;
      if ((bindByClnt[appName]).currentBind == (bindByClnt[appName]).totalBind) {
	printf("Application:%s ALL REGEXPS BINDED\n", appName.c_str());
      } else {
	//	printf("Application:%s bind [%d/%d]\n", appName.c_str(), 
	//     (bindByClnt[appName]).currentBind, (bindByClnt[appName]).totalBind);
      }
      break;
    case IvyRemoveBind:
      printf("Application:%s bind '%s' REMOVED\n", appName.c_str(), regexp );
      break;
    case IvyFilterBind:
      printf("Application:%s bind '%s' FILTRED\n", appName.c_str(), regexp );
      break;
    case IvyChangeBind:
      printf("Application:%s bind '%s' CHANGED\n", appName.c_str(), regexp );
      break;
    }
}


void stopCB (TimerId id, void *user_data, unsigned long delta)
{
  IvyStop ();
}


void sendAllMessageCB (TimerId id, void *user_data, unsigned long delta)
{
  ListOfString  *messages = (ListOfString *) user_data;
  double startTime = currentTime();
  unsigned int envoyes=0;

  IvySendMsg ("startOfSequence");
  ListOfString::iterator  iter;  
  for (iter=messages->begin(); iter != messages->end(); iter++) {
    envoyes += IvySendMsg ((*iter).c_str());
  }
  IvySendMsg ("endOfSequence");

  printf ("[ivy %d.%d] envoyer [%d/%d] messages filtrés par %d regexps a %d clients "
	  "prends %.1f secondes\n",
	  IVYMAJOR_VERSION, IVYMINOR_VERSION,
	  envoyes, nbMess, nbReg, numClients, 
	  (currentTime()-startTime) / 1000.0) ;
  TimerRepeatAfter (1, 1000, sendAllMessageCB ,user_data);
}

void recepteurCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  //  unsigned long recN = (long) user_data;
  //  printf (".");
  //  if (!((argc == 1) && (strcmp (argv[0], "OfSequence")) == 0))
  nbMess++;
}


void recepteurReadyCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  ListOfString  *messages = (ListOfString *) user_data;
  unsigned int instance = atoi( *argv++ );
  if ((strcmp (*argv, "Ready") == 0) && (numClients != 1)) {
    return;
  }


  recReady[instance] = true;
  bool readyToStart = true;

  for (unsigned int i=0; i< numClients; i++) {
    if (recReady[i]==false) {
      //      printf ("Emetteur : manque recepteur [%d/%d]\n", i, numClients-1);
      readyToStart = false;
    }
  }
  
  if (readyToStart == true) {
    if (kindOfTest == throughput) {
      TimerRepeatAfter (1, 2000, sendAllMessageCB , messages);
      //      printf ("Emetteur : tous recepteurs prets : on envoie la puree !!\n");
    }
  }
}

void otherRecepteurReadyCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  int *nbOtherReady = (int *) user_data;

  //  printf ("DBG>> otherRecepteurReadyCB\n");
  
  if ((--(*nbOtherReady)) == 0) {
    IvySendMsg ("IvyThroughputReceive_%d Bindone", globalInst);
  } else {
    //    printf ("IvyThroughputReceive_%d nbOtherReady=%d\n", globalInst, *nbOtherReady);
  }
}


void startOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  nbMess = 0;
}


void endOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  //nbMess--;
  printf ("recepteur %d a recu %d messages\n", globalInst, nbMess);
}


void aliveCB (TimerId id, void *user_data, unsigned long delta)
{
  //  printf ("********* recepteur alive CALLED ***********\n");
}
