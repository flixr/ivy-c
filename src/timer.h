/* Module de gestion des timers autour d'un select */

typedef struct _timer *TimerId;
typedef void (*TimerCb)( TimerId id , void *user_data, unsigned long delta );

/* API  le temp est en milli secondes */
#define TIMER_LOOP -1			/* timer en boucle infinie */
TimerId TimerRepeatAfter( int count, long time, TimerCb cb, void *user_data );

void TimerModify( TimerId id, long time );

void TimerRemove( TimerId id );

/* Interface avec select */

struct timeval *TimerGetSmallestTimeout();

void TimerScan();
