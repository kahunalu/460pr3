#ifndef _OS_H_
#define _OS_H_
   
#define MAXTHREAD     16
#define WORKSPACE     256   /** in bytes, per THREAD */
#define MAXMUTEX      8
#define MAXEVENT      8
#define MSECPERTICK   10   /** resolution of a system tick in milliseconds */
#define MINPRIORITY   10   /** 0 is the highest priority, 10 the lowest */


#ifndef NULL
#define NULL          0   /** undefined */
#endif

#define Disable_Interrupt()     asm volatile ("cli"::)
#define Enable_Interrupt()      asm volatile ("sei"::)

typedef void (*voidfuncptr) (void);      /** pointer to void f(void) */

typedef unsigned int PID;        /** always non-zero if it is valid */
typedef unsigned int MUTEX;      /** always non-zero if it is valid */
typedef unsigned int PRIORITY;
typedef unsigned int EVENT;      /** always non-zero if it is valid */
typedef unsigned int TICK;

/**
  *  This is the set of states that a task can be in at any given time.
  */
typedef enum process_states {
    DEAD = 0,
    READY,
    RUNNING,
    SLEEPING,
    BLOCKED_ON_MUTEX,
    WAITING,
    TERMINATED
} PROCESS_STATES;

/**
  * This is the set of kernel requests
  */
typedef enum kernel_request_type {
    NONE = 0,
    CREATE,
    NEXT,
    SLEEP,
    TERMINATE,
    SUSPEND,
    RESUME,
    MUTEX_INIT,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    EVENT_INIT,
    EVENT_WAIT,
    EVENT_SIGNAL
} KERNEL_REQUEST_TYPE;

/**
  *  This is the set of states that a mutex can be in at any given time.
  */
typedef enum mutex_state {
    DISABLED,
    FREE,
    LOCKED
} MUTEX_STATE;

/**
  * Each mutex is represented by a mutex struct, which contains all
  * relevant information about this mutex.
  */
typedef struct Mutex {
    MUTEX m;
    MUTEX_STATE state;
    PID owner;
    unsigned int lockCount;
} MTX;

/**
  *  This is the set of states that a mutex can be in at any given time.
  */
typedef enum event_state {
    INACTIVE,
    UNSIGNALLED,
    SIGNALLED
} EVENT_STATE;

/**
  * Each event is represented by a event struct, which contains all
  * relevant information about this event.
  */
typedef struct Event {
    EVENT e;
    EVENT_STATE state;
    PID p;
} EVT;

/**
  * Each task is represented by a process descriptor, which contains all
  * relevant information about this task. For convenience, we also store
  * the task's stack, i.e., its workspace, in here.
  */
typedef struct ProcessDescriptor {
    PID p;
    unsigned char *sp;   /* stack pointer into the "workSpace" */
    unsigned char workSpace[WORKSPACE]; 
    PROCESS_STATES state;
    PRIORITY py;
    PRIORITY inheritedPy;
    int arg;
    voidfuncptr  code;   /* function to be executed as a task */
    KERNEL_REQUEST_TYPE request;
    unsigned int response;
    TICK wakeTickOverflow;
    TICK wakeTick;
    MUTEX m;
    EVENT eWait;
    EVENT eSend;
    unsigned int suspended;
    PID pidAction;
} PD;

// void OS_Init(void);      redefined as main()
void OS_Abort(void);

PID  Task_Create( void (*f)(void), PRIORITY py, int arg);
void Task_Terminate(void);
void Task_Next(void); // Same as yield
int  Task_GetArg();
void Task_Suspend( PID p );          
void Task_Resume( PID p );

void Task_Sleep(TICK t);  // sleep time is at least t*MSECPERTICK

MUTEX Mutex_Init(void);
void Mutex_Lock(MUTEX m);
void Mutex_Unlock(MUTEX m);

EVENT Event_Init(void);
void Event_Wait(EVENT e);
void Event_Signal(EVENT e);

#endif /* _OS_H_ */
