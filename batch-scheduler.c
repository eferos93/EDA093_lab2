/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "lib/random.h" //to generate random numbers


#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct {
	int direction;
	int priority;
} task_t;

//struct semaphore slotsFree;
struct condition waitingToGo[2][2]; //Condition matrix for all task types
struct lock block;
int currentDirection; //either 0 or 1
int slotsFree; // 0 <= slotsFree <= BUS_CAPACITY

void init_bus(void);
void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);


void oneTask(task_t task);/*Task requires to use the bus and executes methods below*/
	void getSlot(task_t task); /* task tries to use slot on the bus */
	void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
	void leaveSlot(task_t task); /* task release the slot */



/* initializes semaphores */
void init_bus(void){

        random_init((unsigned int)123456789);
        slotsFree=BUS_CAPACITY;
        int i;
        int j;
        for (i=0; i<2; i++)
                for (j=0; j<2; j++)
                        cond_init(&waitingToGo[i][j]);
        lock_init(&block); //Initiate lock
        currentDirection=SENDER;
}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
 *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread.
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(unsigned int num_tasks_send, unsigned int num_tasks_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{
        char nameArr[7] = "Thread";
        const char *name = nameArr; //Name of all threads
        unsigned int i; //Used in loops below

        for (i = 0; i < num_priority_send; i++){ //Create a thread for all priority tasks going in the SENDER direction
                thread_create(name, 0, senderPriorityTask, NULL);
        }
        for (i = 0; i < num_priority_receive; i++){ 
                thread_create(name, 0, receiverPriorityTask, NULL);

        }
        for (i = 0; i < num_tasks_receive; i++){
                thread_create(name, 0, receiverTask, NULL);

        }
        for (i = 0; i < num_tasks_send; i++){
                thread_create(name, 0, senderTask, NULL);
        }
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
        task_t task = {SENDER, NORMAL};
        oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
        task_t task = {SENDER, HIGH};
        oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
        task_t task = {RECEIVER, NORMAL};
        oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
        task_t task = {RECEIVER, HIGH};
        oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
  getSlot(task);
  transferData(task);
  leaveSlot(task);
}

/* task tries to get slot on the bus subsystem */
void getSlot(task_t task)
{
        lock_acquire(&block); //Aquire block, or sleep until can be aquired

        //Wait: 
        // -IF all slots occupied OR
        // -IF there are free slots < 3 AND
        // -- IF task has normal priority AND highprioritylists are not empty
        // -- OR wrong direction

        while( slotsFree == 0 || (slotsFree < 3 && ((task.priority == NORMAL && (!list_empty(&waitingToGo[task.direction][HIGH].waiters) || !list_empty(&waitingToGo[1-task.direction][HIGH].waiters))) 
                || currentDirection != task.direction)) ) { //|| (currentDirection != task.direction) && slotsFree != 3) { //If no free slots or the direction is different from your own -> wait 
            cond_wait(&waitingToGo[task.direction][task.priority], &block); //Release lock and wait until signalled
        }

        slotsFree--;
	currentDirection=task.direction;
	lock_release(&block);
}

/* task processes data on the bus send/receive */
void transferData(task_t task)
{
    //printf("Enters bus, priority: %d, direction: %d\n", task.priority, task.direction);
    timer_sleep(random_ulong() % 20);
    //printf("Exits buspriority: %d, direction: %d\n", task.priority, task.direction);
}

/* task releases the slot */
void leaveSlot(task_t task)  //CHANGED
{
        lock_acquire(&block);
        slotsFree++;

        if(!list_empty(&(waitingToGo[currentDirection][HIGH].waiters))) { //Any priority tasks in the current direction waiting?
                cond_signal(&waitingToGo[currentDirection][HIGH], &block); //Signal one
        } else if(!list_empty(&waitingToGo[1-currentDirection][HIGH].waiters)) { //If priority task waiting to go in the other direction
                if (slotsFree==BUS_CAPACITY) { //Only broadcast if bus is free
                        cond_broadcast(&waitingToGo[1-currentDirection][HIGH], &block);
                }
        } else if (!list_empty(&waitingToGo[currentDirection][NORMAL].waiters)) {

                cond_signal(&waitingToGo[currentDirection][NORMAL], &block); //Signal one

        } else if (!list_empty(&waitingToGo[1-currentDirection][NORMAL].waiters)) {

                if (slotsFree==BUS_CAPACITY) { //Only broadcast if bus is free
                        cond_broadcast(&waitingToGo[1-currentDirection][NORMAL], &block);
                }
        }
        
        lock_release(&block);
}

