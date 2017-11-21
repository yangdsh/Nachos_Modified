// synchdisk.cc 
//	Routines to synchronously access the disk.  The physical disk 
//	is an asynchronous device (disk requests return immediately, and
//	an interrupt happens later on).  This is a layer on top of
//	the disk providing a synchronous interface (requests wait until
//	the request completes).
//
//	Use a semaphore to synchronize the interrupt handlers with the
//	pending requests.  And, because the physical disk can only
//	handle one operation at a time, use a lock to enforce mutual
//	exclusion.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchdisk.h"

//----------------------------------------------------------------------
// DiskRequestDone
// 	Disk interrupt handler.  Need this to be a C routine, because 
//	C++ can't handle pointers to member functions.
//----------------------------------------------------------------------

static void
DiskRequestDone (int arg)
{
    SynchDisk* disk = (SynchDisk *)arg;

    disk->RequestDone();
}

//----------------------------------------------------------------------
// SynchDisk::SynchDisk
// 	Initialize the synchronous interface to the physical disk, in turn
//	initializing the physical disk.
//
//	"name" -- UNIX file name to be used as storage for the disk data
//	   (usually, "DISK")
//----------------------------------------------------------------------

SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    for( int i = 0 ; i < NumSectors ; i ++)
    {
    	lockset[i] = new Lock("sector lock") ;
	}
    disk = new Disk(name, DiskRequestDone, (int) this);
    turn = 0 ;
    for( int i = 0 ; i < 16 ; i ++ )
    	tag[i] = -1 ;
}

//----------------------------------------------------------------------
// SynchDisk::~SynchDisk
// 	De-allocate data structures needed for the synchronous disk
//	abstraction.
//----------------------------------------------------------------------

SynchDisk::~SynchDisk()
{
    for( int i = 0 ; i < 16 ; i ++ )
    {
		if(tag[i] != -1) {
				disk -> WriteRequest(tag[i] , cache[i]) ;
				semaphore->P();
			}
	}
	delete disk;
    delete lock;
    delete semaphore;
}

//----------------------------------------------------------------------
// SynchDisk::ReadSector
// 	Read the contents of a disk sector into a buffer.  Return only
//	after the data has been read.
//
//	"sectorNumber" -- the disk sector to read
//	"data" -- the buffer to hold the contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    for( int i = 0 ; i < 16 ; i ++ )
    {
    	if(tag[i] == sectorNumber)
    	{
    		bcopy(cache[i], data, SectorSize) ;
    		lock->Release() ;
    		return ;
		}
	}
	for( int i = 0 ; i < 4 ; i ++ )
	{
		if( sectorNumber + i < NumSectors )
		{
			if(tag[turn] != -1) {
				disk -> WriteRequest(tag[turn] , cache[turn]) ;
				semaphore->P();
			}
			disk -> ReadRequest(sectorNumber + i , cache[turn]) ;
			semaphore->P();	
			if( i == 0 ) bcopy(cache[turn], data, SectorSize) ;
			tag[turn] = sectorNumber + i ;
			turn = (turn + 1) % 16 ;
		}
	}
    lock->Release() ;
    return ;
}

//----------------------------------------------------------------------
// SynchDisk::WriteSector
// 	Write the contents of a buffer into a disk sector.  Return only
//	after the data has been written.
//
//	"sectorNumber" -- the disk sector to be written
//	"data" -- the new contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    for( int i = 0 ; i < 16 ; i ++ )
    {
    	if(tag[i] == sectorNumber)
    	{
    		bcopy(data, cache[i], SectorSize) ;
    		lock->Release() ;
    		return ;
		}
	}
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    
    lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::RequestDone
// 	Disk interrupt handler.  Wake up any thread waiting for the disk
//	request to finish.
//----------------------------------------------------------------------

void
SynchDisk::RequestDone()
{ 
    semaphore->V();
}
