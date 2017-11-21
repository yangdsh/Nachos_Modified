// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    printf("Allocate: %d\n", numSectors) ;
    int numSemiHeaders = divRoundUp(numSectors, NumSemiDirect);
    if (freeMap->NumClear() < numSectors + numSemiHeaders)
	return FALSE;		// not enough space
	
	/*for (int i = 0; i < numSemiHeaders; i++)
		dataSectors[i] = freeMap->Find();
	*/
	freeMap->FindGroup(numSemiHeaders, dataSectors);
	int idx, idy = 0, offset ;
	int semiDirect[NumSemiDirect] ;
    while( idy < numSectors )
    {
    	idx = idy / NumSemiDirect ;
    	/*
		offset = idy % NumSemiDirect ;
    	semiDirect[offset] = freeMap->Find();
    	idy ++ ;
    	if( offset == NumSemiDirect - 1 )
    		synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect); 
		*/
		if( numSectors - idy > NumSemiDirect )
		{
			freeMap->FindGroup(NumSemiDirect, semiDirect);	
			synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect);
			idy += NumSemiDirect ;
		}
		else
		{
			freeMap->FindGroup(numSectors - idy, semiDirect);	
			synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect);
			idy += numSectors - idy ;
		}		
	}
	if( offset != NumSemiDirect - 1 )
    	synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect);
    return TRUE;
}

bool
FileHeader::Expand(BitMap *freeMap, int expandSize)
{ 
    int newNumBytes = expandSize ;
    int newNumSectors  = divRoundUp(expandSize, SectorSize);
    //printf("numSectors: %d\n", numSectors) ;
    int numSemiHeaders = divRoundUp(numSectors, NumSemiDirect);
    int newNumSemiHeaders = divRoundUp(newNumSectors, NumSemiDirect);
    printf("Expand: %d, %d\n", expandSize, newNumSectors) ;
    if (freeMap->NumClear() < newNumSectors - numSectors 
							+ newNumSemiHeaders - numSemiHeaders)
	return FALSE;		// not enough space
	
	//for (int i = numSemiHeaders; i < newNumSemiHeaders; i++)
		//dataSectors[i] = freeMap->Find();
	if( newNumSemiHeaders-numSemiHeaders > 0 )
		freeMap->FindGroup(newNumSemiHeaders-numSemiHeaders, &dataSectors[numSemiHeaders]);
	int idx, idy = numSectors, offset ;
	int semiDirect[NumSemiDirect] ;
	if( idy % NumSemiDirect != 0 )
	{
		idx = idy / NumSemiDirect ;
		synchDisk->ReadSector(dataSectors[idx], (char*) semiDirect);
	}
    while( idy < newNumSectors )
    {
    	idx = idy / NumSemiDirect ;
    	offset = idy % NumSemiDirect ;
    	/*
    	semiDirect[offset] = freeMap->Find();
    	idy ++ ;
    	if( offset == NumSemiDirect - 1 )
    		synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect); 	
    	*/
    	if( newNumSectors - idy + offset > NumSemiDirect )
		{
			freeMap->FindGroup(NumSemiDirect-offset, &semiDirect[offset]);	
			synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect);
			idy += NumSemiDirect-offset ;
		}
		else
		{
			freeMap->FindGroup(newNumSectors - idy, &semiDirect[offset]);	
			synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect);
			idy += newNumSectors - idy ;
		}
	}
	if( offset != NumSemiDirect - 1 )
    	synchDisk->WriteSector(dataSectors[idx], (char*) semiDirect);
    
    numBytes = newNumBytes ;
    numSectors = newNumSectors ;
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    int semiDirect[NumSemiDirect] ;
    int numSemiHeaders = divRoundUp(numSectors, SectorSize);
	for (int i = 0; i < numSemiHeaders; i++) {
		ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
		if( i != numSemiHeaders - 1 )
		{
			synchDisk->ReadSector(dataSectors[i], (char*) semiDirect);
			for( int j = 0 ; j < NumSemiDirect; j ++ )
			{
				//printf("%d, %d, deallocate: %d\n", i, j, semiDirect[j]) ;
				if(freeMap->Test(semiDirect[j])) {
					freeMap->Clear(semiDirect[j]) ;
				}
			}
		}
		else
		{
			int last = numSectors % NumSemiDirect ;
			synchDisk->ReadSector(dataSectors[i], (char*) semiDirect);
			for( int j = 0 ; j < last ; j ++ )
			{
				//printf("%d, %d, deallocate: %d\n", i, j, semiDirect[j]) ;
				ASSERT(freeMap->Test(semiDirect[j])) ;
				freeMap->Clear(semiDirect[j]) ;
			}
		}
		freeMap->Clear((int) dataSectors[i]);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int idx = offset / SectorSize / NumSemiDirect ;
    int newoffset = (offset / SectorSize) % NumSemiDirect ;
    int semiDirect[NumSemiDirect] ;
    synchDisk->ReadSector(dataSectors[idx], (char*) semiDirect);
	return(semiDirect[newoffset]);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}
