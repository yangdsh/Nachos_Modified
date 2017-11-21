// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

extern int tlbAccess, badAddress ;
int turn = 0 ;
int PageTime = 0 ;

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
	
	//halt
    if ((which == SyscallException) && (type == SC_Halt)) {
		DEBUG('a', "Shutdown, initiated by user program.\n");
   		interrupt->Halt();
    }
    //exit
	else if ((which == SyscallException) && (type == SC_Exit)) {
		DEBUG('a', "Finish, initiated by user program.\n");
		printf("System call implement by Yang: exit\n") ;
   		currentThread->Finish();
    }   
    //tlb miss 
    else if (which == TLBMissException)
    {
    	tlbAccess ++ ;
		unsigned int VAddr = machine->registers[BadVAddrReg] ;
    	unsigned int vpn = (unsigned) VAddr / PageSize ;
    	unsigned int offset = (unsigned) VAddr % PageSize ;
    	TranslationEntry entry ;
    	bool findEntry = FALSE ;
    	
    	tryAgain:
    	for( int i = 0; i < NumPhysPages; i ++ )
    	{
    		if( (machine->invertedList[i]).vpn == vpn 
    			&& (machine->invertedList[i]).used == TRUE
				&& (machine->invertedList[i]).tid == currentThread->getTid() )
    		{
    			entry = (machine->invertedList[i]).entry ;
    			findEntry = TRUE ;
    			//printf("page in, vpn: %d, page: %d\n", vpn, i) ;
    			
    			(machine->invertedList[i]).lastTime = PageTime ;
    			PageTime ++ ;
    			break ;
			}
		}
		if( findEntry == FALSE )
		{
	    	DEBUG('a', "virtual page # %d is not valid!\n", 
				VAddr, machine->pageTableSize);
	    	machine->RaiseException(PageFaultException, VAddr);
	    	goto tryAgain ;
		}
		//if(tlbAccess%100000 < 5) printf("TLBAccess: %d, VPN: %d, %d, %d\n", tlbAccess, vpn, badAddress, VAddr ) ;
		
/*		
		//1. 轮转调度 
		machine->tlb[turn] = *entry ;
		printf("TLBIndex: %d\n", turn) ;
		turn = (turn + 1) % TLBSize ;
*/		
		//2. LRU调度
		int index = 0, min = 1000000000 ;
		for(int i = 0; i < TLBSize; i ++)
		{
			if( (machine->tlb[i]).time <= min ) 
			{
				min = (machine->tlb[i]).time ;
				index = i ;
			}
		}
		machine->tlb[index] = entry ;
		//printf("tlb: %d, valid: %d\n", index, entry.valid) ;
		//printf("TLBIndex: %d\n", index) ;		
/*
		//2. LFU调度
		int index = 0, min = 10000000 ;
		for(int i = 0; i < TLBSize; i ++)
		{
			if( (machine->tlb[i]).freq <= min ) 
			{
				min = (machine->tlb[i]).freq ;
				index = i ;
			}
		}
		machine->tlb[index] = *entry ;
		printf("TLBIndex: %d\n", index) ;
*/
	}
	//page miss 
    else if (which == PageFaultException)
    {
		int virtualAddr = machine->registers[BadVAddrReg];
    	int vpn = virtualAddr / PageSize ;
    	int find = -1 ;
    	int min = 1000000000 ;
    	for( int i = 0 ; i < NumPhysPages; i ++ )
    	{
    		if( (machine->invertedList[i]).used == FALSE )
    		{
    			find = i ;
    			break ;
			}
		}
		
		if( find == -1 )
		for( int i = 0 ; i < NumPhysPages; i ++ )
    	{
    		int cannot = 0 ;
			if( (machine->invertedList[i]).lastTime < min )
    		{
    			for(int j = 0 ; j < 4 ; j ++ )
					if( (machine->tlb[j]).virtualPage == (machine->invertedList[i]).vpn && 
						currentThread->getTid() == (machine->invertedList[i]).tid)
							cannot = 1 ;
				if( cannot == 1 ) continue ;
				min = (machine->invertedList[i]).lastTime ;
    			find = i ;
			}
		}
		//printf("page fault: %d, vpn: %d, page: %d\n", PageTime, vpn, find) ;
		
		if( (machine->invertedList[find]).used )
		{
			int tid = (machine->invertedList[find]).tid ;
			int ovpn = (machine->invertedList[find]).vpn ;
			if(freeTid[tid] == 0)
			{
				Thread * thread = Tpool[tid] ;
				bcopy(&machine->mainMemory[find*PageSize], &thread->space->swap[ovpn*PageSize], PageSize) ;
			}
		}
		
		bcopy(&currentThread->space->swap[vpn*PageSize], &machine->mainMemory[find*PageSize], PageSize) ;
		
		(machine->invertedList[find]).used = TRUE ;
		(machine->invertedList[find]).tid = currentThread->getTid() ;
		(machine->invertedList[find]).vpn = vpn ;
		
		(machine->invertedList[find]).entry.valid = TRUE;
		(machine->invertedList[find]).entry.use = FALSE;
		(machine->invertedList[find]).entry.dirty = FALSE;
		(machine->invertedList[find]).entry.readOnly = FALSE;
		(machine->invertedList[find]).entry.freq = 0;
		(machine->invertedList[find]).entry.time = machine->TLBtime;
		(machine->invertedList[find]).entry.physicalPage = find ;  
		(machine->invertedList[find]).entry.virtualPage = vpn ;
	}
	else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
