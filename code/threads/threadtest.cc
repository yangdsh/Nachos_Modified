// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"
// testnum is set in main.cc
int testnum = 1;
int max = 3, num = 0, reader ; //生产者消费者队列长度 
int pool[20] ;
extern bool allowed, iftime, syn ;
Lock* lock ; 
Condition *cond, *full, *empty ;
Semaphore *Mutex, *Full, *Empty, *Buf ; 
Barrier* barrier ;

//----------------------------------------------------------------------
// LoopThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
LoopThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
		TS();
    }
}

void SimpleThread(int which)
{
	printf("*** thread %d with priority %d finished\n", currentThread->getTid() , currentThread->prior );
	return;
}

void WakeUp(int arg)
{
	int tid = arg%1000 ;
	int version = arg/1000 ;
	//printf("t:%d, v:%d\n", tid, version) ;
	Thread* t = Tpool[tid];
	if(version = t->version) scheduler->JustCreated(t) ;
}

void TimeAdvance(int none)
{
	printf("*** Just created thread %d with priority %d\n", 
						currentThread->getTid() , currentThread->prior );
	int advance = 0 ;
	for(int i = 1 ; i <= 5 ; i ++)
	{
		printf("Thread %d, TimeTick is: %d, Used Time is: %d\n", 
				currentThread->getTid(), currentThread->TimeTick, currentThread->UsedTime) ;
		printf("Assigning a time past of: ", currentThread->getTid()) ;
		scanf("%d", &advance) ;
		if(advance == -1)
		{
			currentThread->Yield();
			continue ;
		}
		if(advance == -2) 
		{
			interrupt->Schedule(WakeUp, currentThread->getTid()+
							(++currentThread->version)*1000, 50, TimerInt);
			IntStatus oldLevel = interrupt->SetLevel(IntOff);
			currentThread->Sleep();
			(void) interrupt->SetLevel(oldLevel);
			continue ;
		}
		currentThread->UsedTime += advance; 
		stats->totalTicks += advance;
		stats->systemTicks += advance;
		interrupt->OneTick() ;
	}
}

void LockThread(int arg)
{
	printf("Begin \n") ;
	while( !lock->isHeldByCurrentThread() ) lock->Acquire() ;
	if(arg)
	{	
		printf("Yield \n") ;
		currentThread->Yield() ;
		printf("Continue \n") ;
	}
	printf("Finish \n") ;
	lock->Release() ;
}

void CondThread(int arg)
{
	printf("Begin\n") ;
	lock->Acquire() ;
	if(arg)
	{
		printf("Wait\n") ;
		cond->Wait(lock) ;
	}
	else
	{	
		printf("Signal\n") ;
		cond->Signal(lock) ;
	}
	lock->Release() ;
	printf("Finish\n") ;
}

void producer(int arg)
{
	/* 
	lock->Acquire() ;
	if(num == max)
	{
		printf("producer waiting, num = %d\n", num) ;
		full->Wait(lock) ;
		printf("producer waking, num = %d\n", num) ;
	}
	pool[num++] = arg ;
	printf("produce, id = %d\n", arg) ;
	if(num == 1)
		empty->Signal(lock) ;
	lock->Release() ;
	*/
	Full -> P() ;
	Mutex -> P() ;
	pool[num++] = arg ;
	printf("produce, id = %d\n", arg) ;
	Mutex -> V() ;
	Empty -> V() ;	
}

void Produce(int none)
{
	for(int i = 1 ; i <= 10 ; i ++)
	{
		producer(i) ;
	}
}

void consumer(int* arg)
{
/*
	lock->Acquire() ;
	if(num == 0)
	{
		printf("consumer waiting, num = %d\n", num) ;
		empty->Wait(lock) ;
		printf("consumer waking, num = %d\n", num) ;
	}
	*arg = pool[--num] ;
	printf("consume, id = %d\n", *arg) ;
	if(num == max-1) full->Signal(lock) ;
	lock->Release() ;
*/
	Empty -> P() ;
	Mutex -> P() ;
	*arg = pool[--num] ;
	printf("consume, id = %d\n", *arg) ;
	Mutex -> V() ;
	Full -> V() ;
}

void Consume(int none)
{
	int arg[1] ;
	for(int i = 1 ; i <= 10 ; i ++)
	{
		consumer(arg) ;
	}
}

void BarThread(int arg)
{
	printf("Thread %d doing the first part\n", arg) ;
	barrier->Reach() ;
	printf("Thread %d doing the second part\n", arg) ;
}

char str[100] ;
void myRead(int arg)
{
	char temp[100] ;
	Mutex->P() ;
	if(reader == 0) Buf->P() ;
	reader ++ ;
	Mutex->V() ;
	strcpy(temp, str) ;
	printf("reader %d reads: %s\n", arg, str) ;
	printf("reader %d yields\n", arg) ;
	currentThread->Yield() ;
	Mutex->P() ;
	reader -- ;
	if(reader == 0) Buf->V() ;
	printf("reader %d finishes\n", arg) ;
	Mutex->V() ;
}

void myWrite(int appand)
{
	Buf->P() ;
	strcat(str, (char*)(&appand)) ;
	printf("writer %d writes: %s\n", appand-'a'+1, str) ;
	printf("writer %d finishes\n", appand-'a'+1) ;
	Buf->V() ;	
}
//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call LoopThread, and then calling LoopThread ourselves.
//----------------------------------------------------------------------

//测试TS命令 
void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = createThread("forked thread", 5);
	if (t == NULL)
	{
		printf("Error: too many threads!\n");
		return;
	}
    t->Fork(LoopThread, 1);
    LoopThread(0);
}

//测试进程数量限制 
void
TestForNum()
{
	DEBUG('t', "Entering TestForNum");
	for (int i = 2; i <= 129; i++)
	{
		printf("create:%d\n", i);
		Thread *t = createThread("forked thread", 5);
		if (t == NULL)
		{
			printf("Error: too many threads!\n");
			continue;
		}
		t->Fork(SimpleThread, i);
	}
	TS();
}

//测试抢占调度 
void
TestForPrior1(int none)
{
	DEBUG('t', "Entering TestForPrior1");
	
	Thread *t = createThread("thread 3 priority 4", 4);
	t->Fork(SimpleThread, 1);
	printf("*** thread %d with priority %d finished\n", currentThread->getTid() , currentThread->prior );

}
void
TestForPrior2()
{
	DEBUG('t', "Entering TestForPrior2");
	
	Thread *t = createThread("thread 2 priority 5", 5);
	t->Fork(TestForPrior1, 1);	
	printf("*** thread %d with priority %d finished\n", currentThread->getTid() , currentThread->prior );
}

//测试时间片轮转和多级反馈队列 
void
TestForTick()
{
	allowed = 1 ;
	DEBUG('t', "Entering TestForTick");
	//printf("*** creating thread %d with priority %d\n", currentThread->getTid() , currentThread->prior );
	Thread *t = createThread("time thread 2", 2);
	t->Fork(TimeAdvance, 1);
	t = createThread("time thread 3", 2);
	t->Fork(TimeAdvance, 1);	
	t = createThread("time thread 4", 2);
	t->Fork(TimeAdvance, 1);
}

//测试锁机制 
void TestForLock()
{
	DEBUG('t', "Entering TestForLock");
	lock = new Lock("myLock") ;
	Thread *t = createThread("thread 1", 2);
	t->Fork(LockThread, 1);
	t = createThread("thread 2", 2);
	t->Fork(LockThread, 0);
}

//测试条件变量机制 
void TestForCond()
{
	DEBUG('t', "Entering TestForLock");
	lock = new Lock("myLock") ;
	cond = new Condition("myCond") ;
	Thread *t = createThread("thread 1", 2);
	t->Fork(CondThread, 1);
	t = createThread("thread 2", 2);
	t->Fork(CondThread, 0);
}

//测试生产者消费者问题 
void TestForProdCons()
{
	DEBUG('t', "Entering Test For Producer&Consumer");
	lock = new Lock("myLock") ;
	Full = new Semaphore("FullSem", 3) ;
	Empty = new Semaphore("EmptySem", 0) ;
	Mutex = new Semaphore("MutexSem", 1) ;
	full = new Condition("fullCond") ;
	empty = new Condition("emptyCond") ;
	Thread *t = createThread("Producer", 2);
	t->Fork(Produce, 1);
	t = createThread("Consumer", 2);
	t->Fork(Consume, 1);
}

//测试屏障机制 
void TestForBarrier()
{
	DEBUG('t', "Entering Test For Barrier");
	num = 3 ;
	barrier = new Barrier("myBarrier", num) ;
	for(int i = 1 ; i <= num ; i ++)
	{
		char* string = new char[10] ;
		strcpy(string, "Thread") ;
		string[6] = i+'0'; string[7] = 0;
		Thread *t = createThread(string, 2);
		t->Fork(BarThread, i) ;
	}
	printf("waiting for every thread reaching the barrier\n") ;
	barrier->Wait() ;
	printf("every thread reaching the barrier\n") ;
}

//测试读写者问题 
void TestForRW() 
{ 
	DEBUG('t', "Entering Test For Read&Write");
	Buf = new Semaphore("BufSem", 1) ;
	Mutex = new Semaphore("MutexSem", 1) ;
	Thread *t ;
	for( int i = 1 ; i <= 4 ; i ++ )
	{
		t = createThread("Writer", 2);
		t->Fork(myWrite, i-1+'a');
		t = createThread("Reader", 2);
		t->Fork(myRead, i);
	}
}

void TestForFile()
{
	;
}
//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
		/*tsFlag = 1 ;
		ThreadTest1();*/
		TestForFile() ;
		break;
	case 2:
		TestForNum();
		break;
	case 3:
		iftime = 1 ;
		TestForPrior2();
		break;
	case 4:
		iftime = 1 ;
		TestForTick();
		break;
	case 5:
		syn = 1 ;
		TestForLock();		
		break;
	case 6:
		syn = 1 ;
		TestForCond();		
		break;
	case 7:
		syn = 1 ;
		TestForProdCons() ;
		break ;
	case 8:
		syn = 1 ;
		TestForBarrier() ;
		break ;
	case 9:
		syn = 1 ;
		TestForRW() ;
		break ;
    default:
	printf("No test specified.\n");
	break;
    }
}

