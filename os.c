#include <string.h>
#include <stdlib.h>
#include "os.h"
#include <avr/io.h>
#include <avr/interrupt.h>

//Comment out the following line to remove debugging code from compiled version.
#define DEBUG

typedef void (*voidfuncptr) (void);      /* pointer to void f(void) */ 


/*===========
  * RTOS Internal
  *===========
  */

/**
  * This internal kernel function is the context switching mechanism.
  * It is done in a "funny" way in that it consists two halves: the top half
  * is called "Exit_Kernel()", and the bottom half is called "Enter_Kernel()".
  * When kernel calls this function, it starts the top half (i.e., exit). Right in
  * the middle, "Cp" is activated; as a result, Cp is running and the kernel is
  * suspended in the middle of this function. When Cp makes a system call,
  * it enters the kernel via the Enter_Kernel() software interrupt into
  * the middle of this function, where the kernel was suspended.
  * After executing the bottom half, the context of Cp is saved and the context
  * of the kernel is restore. Hence, when this function returns, kernel is active
  * again, but Cp is not running any more. 
  * (See file "switch.S" for details.)
  */
extern void CSwitch();
extern void Exit_Kernel();    /* this is the same as CSwitch() */

extern void a_main();

/* Prototype */
void Task_Terminate(void);
void enter_sleep_queue();

/** 
  * This external function could be implemented in two ways:
  *  1) as an external function call, which is called by Kernel API call stubs;
  *  2) as an inline macro which maps the call into a "software interrupt";
  *       as for the AVR processor, we could use the external interrupt feature,
  *       i.e., INT0 pin.
  *  Note: Interrupts are assumed to be disabled upon calling Enter_Kernel().
  *     This is the case if it is implemented by software interrupt. However,
  *     as an external function call, it must be done explicitly. When Enter_Kernel()
  *     returns, then interrupts will be re-enabled by Enter_Kernel().
  */ 
extern void Enter_Kernel();

#define Disable_Interrupt()		asm volatile ("cli"::)
#define Enable_Interrupt()		asm volatile ("sei"::)
  
typedef enum errnum {
  E_DEFAULT = 0,
  E_EXCEEDS_MAXEVENT = 1,
  E_EXCEEDS_MAXMUTEX = 2,
  E_EXCEEDS_MAXPROCESS = 3,
  E_DNE = 4
} ERR_NO;

volatile static ERR_NO err_no = E_DEFAULT;

void error() {
  
  int i;
  for(i = 0; i < err_no+1; i++) {
    PORTB = 0x80;
    PORTB = 0x00;
  }
}

/**
  *  This is the set of states that a task can be in at any given time.
  */
typedef enum process_states 
{ 
   DEAD = 0, 
   BLOCKED,
   READY, 
   RUNNING,
} PROCESS_STATES;

/**
  * This is the set of kernel requests, i.e., a request code for each system call.
  */
typedef enum kernel_request_type 
{
   NONE = 0,
   CREATE,
   NEXT,
   TERMINATE,
   SUSPEND,
   SLEEP,
   CREATE_MUTEX,
   LOCK_MUTEX,
   UNLOCK_MUTEX,
   CREATE_EVENT,
   EVENT_WAIT,
   EVENT_SIGNAL
} KERNEL_REQUEST_TYPE;

/**
  * Each task is represented by a process descriptor, which contains all
  * relevant information about this task. Holds relevant information for 
  * preforming kernel requests
  */
typedef struct ProcessDescriptor {
  // ID, Kernel Request, and State of the process
  PID id;
  KERNEL_REQUEST_TYPE request;
  PROCESS_STATES state;

  // Priority and argument of process assigned on creation
  int argument;
  PRIORITY priority;
  
  //Suspended Flag and Number of Ticks to sleep
  int sus;
  TICK sleep_time;
  
  //Mutex and event id's used for kernel requests
  MUTEX mid;
  EVENT e;

  //Argument and priority of task to create used for kernel requests
  int create_argument;
  PRIORITY create_priority;
  
  //Stack pointer into the workspace
  unsigned char *sp;
  unsigned char workSpace[WORKSPACE]; 
  voidfuncptr  code;
} PD;


/**
  * This table contains ALL process descriptors. It doesn't matter what
  * state a task is in.
  */
static PD Process[MAXTHREAD];

/**
  * The process descriptor of the currently RUNNING task.
  */
volatile static PD* Cp; 

/** 
  * Since this is a "full-served" model, the kernel is executing using its own
  * stack. We can allocate a new workspace for this kernel stack, or we can
  * use the stack of the "main()" function, i.e., the initial C runtime stack.
  * (Note: This and the following stack pointers are used primarily by the
  *   context switching code, i.e., CSwitch(), which is written in assembly
  *   language.)
  */         
volatile unsigned char *KernelSp;

/**
  * This is a "shadow" copy of the stack pointer of "Cp", the currently
  * running task. During context switching, we need to save and restore
  * it into the appropriate process descriptor.
  */
volatile unsigned char *CurrentSp;

/** index to next task to run */
volatile static unsigned int NextP;  

/** index to next task id available*/
volatile static unsigned int FreeTaskId;  

/** 1 if kernel has been started; 0 otherwise. */
volatile static unsigned int KernelActive;  


/*
  * This internal kernel function is a part of the "scheduler". It chooses the 
  * next task to run, i.e., Cp.
  */
static void Dispatch(){
  PRIORITY high_priority = 10;

  int i;
  for(i = 0; i < MAXTHREAD; i++){
    if(Process[i].state == READY && Process[i].sus != 1 && Process[i].priority < high_priority){
      high_priority = Process[i].priority;
    }
  }

  while(Process[NextP].state != READY || Process[NextP].sus == 1 || Process[NextP].priority > high_priority) {
    NextP = (NextP + 1) % MAXTHREAD;
  }

  Cp = &(Process[NextP]);
  CurrentSp = Cp->sp;
  Cp->state = RUNNING;

  NextP = (NextP + 1) % MAXTHREAD;
}

/*
  * This checks if any tasks are ready and a higher priority than the current task, return 1 if true
  */
static int Preemptive_Check(){
  PRIORITY current_priority = Cp->priority;

  int i;
  for(i = 0; i < MAXTHREAD; i++){
    if(Process[i].state == READY && Process[i].sus != 1 && Process[i].priority < current_priority){
      current_priority = Process[i].priority;
    }
  }

  if(current_priority < Cp->priority){
    Cp->state = READY;
    return 1;
  }

  return 0;
}

/**
  *  This is the set of states that a Mutex can be in at any given time.
  */
typedef enum mutex_states { 
   LOCKED = 0, 
   UNLOCKED
} MUTEX_STATES;

/**
  * Each Mutex is represented by a mutex descriptor, which contains all
  * relevant information about this mutex.
  */
typedef struct MutexDescriptor {
    MUTEX_STATES state;   
    MUTEX id;
    PD* owner;
    PRIORITY original_priority;
    unsigned int lock_count;
    struct mutex_node *wait_queue;
} MD;

/* MUTEX CODE
 *
 *
 *
 *
 */

/* Add one for 1st indexed array  */
static MD Mutex[MAXMUTEX];

/** number of mutexes created so far */
volatile static unsigned int Mutexes;  

/**
  *  Node for blocked queue for mutex
  */
struct mutex_node{
  PD* pd;
  struct mutex_node *next;
};

/* Event structures */
typedef struct EventDescriptor {
  EVENT id;
  unsigned int signalled;
  PD *waiting_p;
} ED;

volatile static unsigned int Events;

/* add one for 1st indexed array use */
static ED Event[MAXEVENT];


void Kernel_Create_Mutex(PRIORITY priority){
  

  Mutex[Mutexes].state = UNLOCKED;
  Mutex[Mutexes].id = Mutexes;
  Mutex[Mutexes].owner = NULL;
  Mutex[Mutexes].original_priority = priority;
  Mutex[Mutexes].lock_count = 0;
  Mutex[Mutexes].wait_queue = NULL;
};

void Kernel_Lock_Mutex(MUTEX mid, PD* Ct){
  
  // If Mutex is unlocked or the calling task is the owner
  if(Mutex[mid].state == UNLOCKED || Ct->id == Mutex[mid].owner->id){
    
    // Set the Mutex Descriptor
    Ct->state = READY;
    Mutex[mid].owner = Ct;
    Mutex[mid].original_priority = Ct->priority;
    Mutex[mid].lock_count++;
    Mutex[mid].state = LOCKED;
  
  // Else Block the calling task and add it to the wait list 
  }else{
    Ct->state = BLOCKED;
    struct mutex_node *new_mutex_node = (struct mutex_node *) malloc(sizeof(struct mutex_node)); 
    new_mutex_node->pd = (PD*) Ct;
    new_mutex_node->next = Mutex[mid].wait_queue;
    Mutex[mid].wait_queue = new_mutex_node;
    
    //Inherit priority if higher priority task is waiting 
    if(Ct->priority < Mutex[mid].owner->priority){
      Mutex[mid].owner->priority = Ct->priority; 
    }

    Dispatch();
  }
};

void Kernel_Unlock_Mutex(MUTEX mid, PD* Ct){

  if(Mutex[mid].state == LOCKED && Ct->id == Mutex[mid].owner->id){
    Mutex[mid].lock_count--;
    if(!Mutex[mid].lock_count){
      Mutex[mid].state = UNLOCKED;

      // Return inheirited priority
      Mutex[mid].owner->priority = Mutex[mid].original_priority;
      
      if(Mutex[mid].wait_queue != NULL){
        /* dequeue dead processes */
        while(Mutex[mid].wait_queue->pd->state == DEAD) {
          Mutex[mid].wait_queue = Mutex[mid].wait_queue->next;
        }
        PD* next_task = Mutex[mid].wait_queue->pd;
        
        Mutex[mid].wait_queue = Mutex[mid].wait_queue->next; 
        Kernel_Lock_Mutex(mid, next_task);
      }
    }
  }
};

/* Create event */
void Kernel_Create_Event() {
  

  Event[Events].id = Events;
  Event[Events].signalled = 0;
  Event[Events].waiting_p = NULL;
}

/* Leaves current task blocked on the event and calls the dispatcher if there are no other tasks waiting on it
* If the event has already been signalled or there is another task waiting on it the current task keeps running
*/
void Kernel_Event_Wait(PD* Ct) {
  unsigned int index = (unsigned int)(Ct->e);

  // If another task is waiting on the event NoOp
  if(Event[index].waiting_p != NULL) {
    Ct->state = RUNNING;

  // If waiting for event that has been signaled, reset signal and continue
  } else if(Event[index].signalled == 1) {
    Ct->state = RUNNING;
    Event[index].signalled = 0;

  // Else Block current task, associate task with event and dispatch next task
  }else{
    Cp->state = BLOCKED;    
    Event[index].waiting_p = Ct;
    Dispatch();
  }
}

/* Records that the event has been signalled
* If there is already a task waiting on that event this task is unblocked
*/
void Kernel_Event_Signal(EVENT e) {
  unsigned int index = (unsigned int)(e);

  Event[index].signalled = 1;
  if(Event[index].waiting_p != NULL) {
    //change waiting task's state to READY
    PORTA |= (1<<PA0);
    PORTA &= ~(1<<PA0);
    Event[index].waiting_p->state = READY;
    Event[index].waiting_p = NULL;
    Event[index].signalled = 0;
  }
}



/** number of tasks created so far */
volatile static unsigned int Tasks;  

/**
 * When creating a new task, it is important to initialize its stack just like
 * it has called "Enter_Kernel()"; so that when we switch to it later, we
 * can just restore its execution context on its stack.
 * (See file "cswitch.S" for details.)
 */
void Kernel_Create_Task_At( PD *p, void (*f)(void), PRIORITY py, int arg) 
{   
   unsigned char *sp;

#ifdef DEBUG
   int counter = 0;
#endif

   //Changed -2 to -1 to fix off by one error.
   sp = (unsigned char *) &(p->workSpace[WORKSPACE-1]);



   /*----BEGIN of NEW CODE----*/
   //Initialize the workspace (i.e., stack) and PD here!

   //Clear the contents of the workspace
   memset(&(p->workSpace),0,WORKSPACE);

   //Notice that we are placing the address (16-bit) of the functions
   //onto the stack in reverse byte order (least significant first, followed
   //by most significant).  This is because the "return" assembly instructions 
   //(rtn and rti) pop addresses off in BIG ENDIAN (most sig. first, least sig. 
   //second), even though the AT90 is LITTLE ENDIAN machine.

   //Store terminate at the bottom of stack to protect against stack underrun.
   *(unsigned char *)sp-- = ((unsigned int)Task_Terminate) & 0xff;
   *(unsigned char *)sp-- = (((unsigned int)Task_Terminate) >> 8) & 0xff;
   *(unsigned char *)sp-- = 0x00;
   
   //Place return address of function at bottom of stack
   *(unsigned char *)sp-- = ((unsigned int)f) & 0xff;
   *(unsigned char *)sp-- = (((unsigned int)f) >> 8) & 0xff;
   *(unsigned char *)sp-- = 0x00;

#ifdef DEBUG
   //Fill stack with initial values for development debugging
   //Registers 0 -> 31 and the status register
   for (counter = 0; counter < 34; counter++)
   {
      *(unsigned char *)sp-- = counter;
   }
#else
   //Place stack pointer at top of stack
   sp = sp - 34;
#endif
      
  p->sp = sp;    /* stack pointer into the "workSpace" */
  p->code = f;   /* function to be executed as a task */
  p->request = NONE;
  p->id = (PID) FreeTaskId;
  p->argument = arg;
  p->priority = py;


   p->state = READY;

}


/**
  *  Create a new task
  */
static void Kernel_Create_Task(void (*f)(void), PRIORITY py, int arg) {


   /* find a DEAD PD that we can use*/
   for(FreeTaskId = 0; FreeTaskId< MAXTHREAD; FreeTaskId++) {
       if(Process[FreeTaskId].state == DEAD) break;
   }

   Kernel_Create_Task_At(&(Process[FreeTaskId]), f, py, arg);
}



/**
  * This is the kernel which dispatches the initial a_main function and then 
  * indeffinitely handles Kernel requests. The Kernel also handles context switching
  * between the task and the kernel
  */
static void Next_Kernel_Request() {
  // Dispatch the main function of the application
  Dispatch();

  // Continue Handling Kernel Requests
  while(1) {
    // Once handled clear the Kernel request
    Cp->request = NONE;

    // Switch context back to the previous/newly selected task from the kernel
    CurrentSp = Cp->sp;
    Exit_Kernel();

    //Tasks making system calls will be enter the kernel here

    // save the Cp's stack pointer
    Cp->sp = (unsigned char *) CurrentSp;

    //Based on the Cp's kernel request execute certain cases 
    switch(Cp->request){
      case CREATE:
        Kernel_Create_Task(Cp->code, Cp->create_priority, Cp->create_argument);
        if(Preemptive_Check()) Dispatch();
        break;
      case SUSPEND:
        Cp->sus = 1;
        if(Cp->state == RUNNING){
          Cp->state = READY;
        }
        Dispatch();
        break;
      case SLEEP:
        Cp->sus = 1;
        if(Cp->state == RUNNING){
          Cp->state = READY;
        }
        enter_sleep_queue();
        Dispatch();
        break;
      case NEXT:
      case NONE:
        Cp->state = READY;
        Dispatch();
        break;
      case TERMINATE:
        /* deallocate all resources used by this task */
        --Tasks;
        Cp->state = DEAD;
        Dispatch();
        break;
      case CREATE_MUTEX:
        Kernel_Create_Mutex(Cp->priority);
        break;
      case LOCK_MUTEX:
        Kernel_Lock_Mutex(Cp->mid, (PD*) Cp);
        break;
      case UNLOCK_MUTEX:
        Kernel_Unlock_Mutex(Cp->mid, (PD*) Cp);
        if(Preemptive_Check()) Dispatch();
        break;
      case CREATE_EVENT:
        Kernel_Create_Event();
        break;
      case EVENT_WAIT:
        Kernel_Event_Wait((PD*) Cp);
        break;
      case EVENT_SIGNAL:
        Kernel_Event_Signal(Cp->e);
        if(Preemptive_Check()) {
          PORTA |= (1<<PA1);
          PORTA &= ~(1<<PA1);
          Dispatch();
        }
        break;
      default:
        /* System Failure if entered */
        break;
    }
  } 
}



/*================
  * RTOS  API  and Stubs
  *================
  */

void OS_Abort(void);

/*================
  *  FUNCTIONS
  *================
  */

/**
  * Create task with given priority and argument, return the process ID
  */
PID Task_Create( void (*f)(void), PRIORITY py, int arg){
  if (Tasks == MAXTHREAD) {
    err_no = E_EXCEEDS_MAXPROCESS;
    error();
    return;  /* Too many tasks! */
   }

  if(KernelActive){
    Disable_Interrupt();
    Cp->request = CREATE;
    Cp->code = f;
    Cp->create_argument = arg;
    Cp->create_priority = py;
    Cp->sus = 0;
    Enter_Kernel();
  } else { 
    Kernel_Create_Task(f, py, arg);
  }
  return FreeTaskId;
}

/**
  * The calling task terminates itself.
  */
void Task_Terminate(void) {
  if (KernelActive) {
    Disable_Interrupt();
    Cp -> request = TERMINATE;
    Enter_Kernel();
    /* never returns here! */
  }
}

void Task_Yield(void){
  if (KernelActive) {
    Disable_Interrupt();
    Cp ->request = NEXT;
    Enter_Kernel();
    Enable_Interrupt();
  }
};

int Task_GetArg(void){
  return Cp->argument;
};

/**
  * Suspends a task until it's ID is passed to resume
  * Does nothing if the task is already suspended
  */
void Task_Suspend( PID p ){
  /* if the task to be suspended is currently running, then let the kernel get a new task running */
  if(Cp->id == p && KernelActive) {
      Disable_Interrupt();
      Cp->request = SUSPEND;
      Enter_Kernel();
      Enable_Interrupt();
  } else {
    int x;
    for(x=0; x < MAXTHREAD; x++) {
      if(Process[x].id == p) {
        Process[x].sus = 1;
        break;
      }
    }
    /* if there are no tasks with this PID then set err_no to does not exist */
    if(x == MAXTHREAD) {
      err_no = E_DNE;
      error();
    }
  }
};

/**
* Resumes a suspended task
* Does nothing if the task isn't suspended
*/
void Task_Resume( PID p ){
  int x;
  for(x=0; x < MAXTHREAD; x++) {
    if(Process[x].id == p) {
      Process[x].sus = 0;
      break;
    }
  }

  /* if there are no tasks with this PID then set err_no to does not exist */
  if(x == MAXTHREAD) {
    err_no = E_DNE;
    error();
  }
};

/* Sleep code
 *
 */

struct sleep_node {
  PD* pd;
  int sleep_expected_count;
  int sleep_actual_count;
  struct sleep_node *next;
};

struct sleep_node *sleep_queue_head = NULL;

void sleep_delete(PID id){

  struct sleep_node *temp, *prev;
  
  temp = sleep_queue_head;
  
  while(temp!=NULL){
    if(temp->pd->id == id){
      if(temp==sleep_queue_head){
        sleep_queue_head=temp->next;
        free(temp);
      }else{
        prev->next=temp->next;
        free(temp);
      }
    }else{
      prev = temp;
      temp = temp->next;
    }
  }
}

void Task_Sleep(TICK t){
  Disable_Interrupt();
  Cp->request = SLEEP;
  Cp->sleep_time = t;
  Enter_Kernel();
  Enable_Interrupt();
};

ISR(TIMER1_COMPA_vect){
  // Initialize linked list pointers
  struct sleep_node *curr_sleep_node = sleep_queue_head;
  struct sleep_node *next_sleep_node = curr_sleep_node->next;
  int id = MAXTHREAD;

  // For each node in linked list
  while(curr_sleep_node != NULL){

    // Increment interrupt count
    curr_sleep_node->sleep_actual_count++;

    // Set the next sleep node
    next_sleep_node = curr_sleep_node->next;

    // If interrupt count is >= what is expected resume task and remove from list
    if(curr_sleep_node->sleep_actual_count >= curr_sleep_node->sleep_expected_count){
      id = curr_sleep_node->pd->id;
      Task_Resume(id);
      sleep_delete(id);
    }

    curr_sleep_node = next_sleep_node;
  }

  // If there is a task
  if(id != MAXTHREAD){
    if(Preemptive_Check()){
      Cp->request = NEXT;
      Enter_Kernel();
    };
  }

  // If nothing left in the list turn off timer interrupts
  if(sleep_queue_head == NULL){
    TIMSK1  = 0;
  }else{
    TCNT1   = 0;
  }
}

void enter_sleep_queue(){
  struct sleep_node *new_sleep_node = (struct sleep_node *) malloc(sizeof(struct sleep_node)); 

  new_sleep_node->pd = (PD*) Cp;
  new_sleep_node->next = sleep_queue_head;
  new_sleep_node->sleep_actual_count = 0;
  new_sleep_node->sleep_expected_count = (int)(Cp->sleep_time); 
  sleep_queue_head = new_sleep_node;

  // Control registers
  TCCR1A  = 0;

  // Configure timer 1 for CTC mode
  TCCR1B  |= (1<<WGM12); 
  
  // start timer
  TIMSK1   |= (1<<OCIE1A);

  //Output compare register, interrupt every MSPERTICK
  OCR1A   = 625;

  // Configure with 256 prescaler
  TCCR1B  |= (1<<CS12);

  //Start count to 0
  TCNT1   = 0;
}

/*================
  * MUTEX FUNCTIONS
  *================
  */



MUTEX Mutex_Init(void){
  
  if(KernelActive){
    Disable_Interrupt();
    if(Mutexes >= MAXMUTEX) {
      err_no = E_EXCEEDS_MAXMUTEX;
      error();
      return; /* prevents creation of too many mutexes */
    }
    Cp->request = CREATE_MUTEX;
    Enter_Kernel();
    return Mutexes++;
  }else{
    Kernel_Create_Mutex(10);
    return Mutexes++;
  }
};

void Mutex_Lock(MUTEX m){
  
  Disable_Interrupt();
    if(m < 0 || m >= MAXMUTEX || Mutex[m].id == MAXMUTEX) {
      err_no = E_DNE;
      error();
      Enable_Interrupt();
      return;
  }
  Cp->request = LOCK_MUTEX;
  Cp->mid = m;
  Enter_Kernel();
};

void Mutex_Unlock(MUTEX m){
  
  Disable_Interrupt();
  if(m < 0 || m >= MAXMUTEX || Mutex[m].id == MAXMUTEX) {
    err_no = E_DNE;
    error();
    Enable_Interrupt();
    return;
  }
  Cp->request = UNLOCK_MUTEX;
  Cp->mid = m;
  Enter_Kernel();
};



/*================
  * EVENT FUNCTIONS
  *================
  */

/* Sends a request to the kernel for the creation of an event
* Returns event id
*/
EVENT Event_Init(void){
  if(Events >= MAXEVENT) {
    err_no = E_EXCEEDS_MAXEVENT;
    error();
    return;
  }
  if(KernelActive) {
    Disable_Interrupt();
    Cp->request = CREATE_EVENT;
    Enter_Kernel();
    return (EVENT)Events++;
  } else {
    Kernel_Create_Event();
    return (EVENT)Events++;
  }
};

/* If event is waited on make EVENT_WAIT kernel request with the event id
*
*/
void Event_Wait(EVENT e){
  
  Disable_Interrupt();
  if(e < 0 || e >= MAXEVENT || Event[e].id == MAXEVENT) {
    err_no = E_DNE;  
    error();
    Enable_Interrupt();
    return;
  }
  Cp->e = e;
  Cp->request = EVENT_WAIT;
  Enter_Kernel();
};

/* When an event is signalled, it wakes up a waiting task if there is one, otherwise it is recorded
*only one outstanding signal on an event is recorded, hence any subsequent signals on the same event will be lost
*/
void Event_Signal(EVENT e){
  
  Disable_Interrupt();
  if(e < 0 || e >= MAXEVENT || Event[e].id == MAXEVENT) {
    err_no = E_DNE;

    error();
    Enable_Interrupt();
    return;
  }
  Cp->e = e;
  Cp->request = EVENT_SIGNAL;
  Enter_Kernel();
};


/**
* This function initializes the RTOS and must be called before any other
* system calls.
*/
void main() {
  Tasks = 0;
  Mutexes = 0;
  Events = 0;
  KernelActive = 0;
  NextP = 0;
  FreeTaskId = 0;

  int x;
  //Reminder: Clear the memory for the task on creation.
  for (x = 0; x < MAXTHREAD; x++) {
    memset(&(Process[x]),0,sizeof(PD));
    Process[x].state = DEAD;
  }

  //Reminder: Clear the memory for the mutexes on creation.
  for (x = 0; x < MAXMUTEX; x++) {
    memset(&(Mutex[x]),0,sizeof(MD));
    Mutex[x].state = UNLOCKED;
    Mutex[x].id = MAXMUTEX;
  }

  //Reminder: Clear the memory for the events on creation.
  for (x = 0; x < MAXEVENT; x++) {
    memset(&(Event[x]),0,sizeof(ED));
    Event[x].id = MAXEVENT;
  }

  //Create the Application main task with the highest priority
  Task_Create(a_main, 0, 0);

  //Begin the RTOS
  Disable_Interrupt();
  KernelActive = 1;
  Next_Kernel_Request();
}
