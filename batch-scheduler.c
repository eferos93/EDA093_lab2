/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/random.h" //generate random numbers

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
struct condition waitingToGo[2][2];
struct lock block;
int currentDirection; //either 0 or 1
int slotsFree;

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
		for (int i=0; i<2; i++)
			for (int j=0; j<2; j++)
				cond_init(&waitingToGo[i][j]);
		lock_init(&block);
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

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{
	
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
		lock_acquire(&block);
		while(slotsFree==0 || slotsFree>0 && currentDirection!= task.direction) {
			cond_wait(&waitingToGo[task.direction][task.priority], &block);
		}
		while(task.priority==NORMAL && !list_empty(&waitingToGo[task.direction][HIGH].waiters)){
			cond_wait(&waitingToGo[task.direction][task.priority], &block);
		}
    slotsFree--;
		currentDirection=task.direction;
		lock_release(&block);
}

/* task processes data on the bus send/receive */
void transferData(task_t task)
{
    printf("Enters bus\n");
		timer_sleep(random_ulog() % 50);
		printf("Exits bus\n", );
}

/* task releases the slot */
void leaveSlot(task_t task)  //CHANGED
{
    lock_acquire(&block);
		slotsFree++;
		if(!list_empty(&(waitingToGo[currentDirection][HIGH].waiters))) {
			cond_signal(&waitingToGo[currentDirection][HIGH], &block);
		} else if(!list_empty(&waitingToGo[currentDirection][NORMAL].waiters)) {
			cond_signal(&waitingToGo[currentDirection][NORMAL], &block);
		} else if (slotsFree==BUS_CAPACITY) {
			cond_broadcast(&waitingToGo[1-currentDirection][HIGH], &block);
			cond_broadcast(&waitingToGo[1-currentDirection][NORMAL], &block);
		}
		lock_release(&block);
}
