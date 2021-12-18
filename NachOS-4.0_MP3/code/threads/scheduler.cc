// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    readyList_L1 = new SortedList<Thread *>(compareBurstTime); 
    readyList_L2 = new SortedList<Thread *>(comparePriority);
    readyList_L3 = new List<Thread *>;
    //readyList = new List<Thread *>;
    toBeDestroyed = NULL;
}

int compareBurstTime(Thread *a, Thread *b){
    // less-burst-time thread is smaller
    double at = a->getBurstTime() - a->getUsedTime();
    double bt = b->getBurstTime() - b->getUsedTime();
    at = (at > 0 ? at : 0);
    bt = (bt > 0 ? bt : 0);
    return ((at == bt) ? 0 : ((at > bt) ? 1 : -1));
}

int comparePriority(Thread *a, Thread *b){
    // higher-priority thread is smaller
    int ap = a->getPriority();
    int bp = b->getPriority();
    return ((ap == bp) ? 0 : ((ap > bp) ? -1 : 1));
}


//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList_L1;
    delete readyList_L2;
    delete readyList_L3;
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    // 把一個thread放進ready queue的過程不可被interrupt打斷(為甚麼?), 所以把interrupt disable掉
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    
    // MP3: put thread into ready lists(L1, L2, L3)

    Thread *oldThread = kernel->currentThread;
    double oldUsedTime_tmp = oldThread->getUsedTime() + (kernel->stats->userTicks - oldThread->burstStartTime);
    
    if(thread->getPriority() >= 100) {
        readyList_L1->Insert(thread);
        
        if(oldThread->getPriority() >= 100 && oldThread->getBurstTime() - oldUsedTime_tmp > thread->getBurstTime() - thread->getUsedTime()) {
            //oldThread->setUsedTime(oldUsedTime_tmp);
            //oldThread->Yield();
        }
        else if(oldThread->getPriority() < 100) {
            //oldThread->setUsedTime(oldUsedTime_tmp);
            //oldThread->Yield();
        }
    }
    else if(thread->getPriority() >= 50) {
        readyList_L2->Insert(thread);

        if(oldThread->getPriority() < 50){
            //oldThread->setUsedTime(oldUsedTime_tmp);
            //oldThread->Yield();
        }
    }
    else {
        readyList_L3->Append(thread);
    }
    // 做完應該要把interrupt enable回來, 這裡沒做可能是在caller那邊才做?
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);


    if (!readyList_L1->IsEmpty()) {
		return readyList_L1->RemoveFront();
    }
    else if (!readyList_L2->IsEmpty()) {
		return readyList_L2->RemoveFront();
    }
    else if (!readyList_L3->IsEmpty()) {
		return readyList_L3->RemoveFront();
    }
    else {
        return NULL;
        // 找出下個要執行的thread的同時從ready queue移除它
    	//return readyList_L3->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
	    toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    nextThread->burstStartTime = kernel->stats->userTicks;
    nextThread->waitStartTime = 0;
    nextThread->setWaitedTime(0);
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList_L3->Apply(ThreadPrint);
}

// MP3: update priority and adjust the readyLists
void
Scheduler::updateThreadPriority()
{
    //cout << "in Scheduler::updateThreadPriority" << '\n';
    SortedList<Thread *> *tmpList_L1 = new SortedList<Thread *>(compareBurstTime);
    SortedList<Thread *> *tmpList_L2 = new SortedList<Thread *>(comparePriority);
    SortedList<Thread *> *tmp1 = readyList_L1, *tmp2 = readyList_L2;

    List<Thread *> *tmpList_L3 = new List<Thread *>();
    List<Thread *> *tmp3 = readyList_L3;


    ListIterator<Thread *> iter1(this->readyList_L1);

	for (; !iter1.IsDone(); iter1.Next()) {
	    iter1.Item()->aging(kernel->stats->userTicks - iter1.Item()->waitStartTime);
        tmpList_L1->Insert(iter1.Item());
    }
    readyList_L1 = tmpList_L1;
    delete tmp1;

    ListIterator<Thread *> iter2(this->readyList_L2);
    //ListIterator<Thread *> *tmp = iter2;
	for (; !iter2.IsDone(); iter2.Next()) {
	    bool upgrade = iter2.Item()->aging(kernel->stats->userTicks - iter2.Item()->waitStartTime);
        if(upgrade) {
            tmpList_L1->Insert(iter2.Item());
        }
        else {
            tmpList_L2->Insert(iter2.Item());
        }
    }
    readyList_L2 = tmpList_L2;
    delete tmp2;

    ListIterator<Thread *> iter3(this->readyList_L3);

	for (; !iter3.IsDone(); iter3.Next()) {
	    bool upgrade = iter3.Item()->aging(kernel->stats->userTicks - iter3.Item()->waitStartTime);
        if(upgrade) {
            tmpList_L2->Insert(iter3.Item());
        }
        else {
            tmpList_L3->Append(iter3.Item());
        }
    }
    readyList_L3 = tmpList_L3;
    delete tmp3;

    //cout << "out Scheduler::updateThreadPriority" << '\n';
}
