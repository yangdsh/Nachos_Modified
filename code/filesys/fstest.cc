// fstest.cc 
//	Simple test routines for the file system.  
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file 
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"
#include "console.h"

#define TransferSize 	1000 	// make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void
Copy(char *from, char *to)
{
    FILE *fp;
    OpenFile* openFile;
    int amountRead, fileLength;
    char *buffer;

// Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {	 
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }

// Figure out length of UNIX file
    fseek(fp, 0, 2);		
    fileLength = ftell(fp);
    fseek(fp, 0, 0);
	
// Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength, "")) {	 // Create Nachos file
	printf("Copy: couldn't create output file %s\n", to);
	fclose(fp);
	return;
    }
  
    openFile = fileSystem->Open(to, "");
    ASSERT(openFile != NULL);
    
// Copy the data in TransferSize chunks
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
	openFile->Write(buffer, amountRead);	
    delete [] buffer;

// Close the UNIX and the Nachos files
    delete openFile;
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void
Print(char *name)
{
    OpenFile *openFile;    
    int i, amountRead;
    char *buffer;

    if ((openFile = fileSystem->Open(name)) == NULL) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    
    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
	for (i = 0; i < amountRead; i++)
	    printf("%c", buffer[i]);
    delete [] buffer;

    delete openFile;		// close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName 	"TestFile1"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	((int)(ContentSize * 20))

static void 
FileWrite()
{
    OpenFile *openFile;    
    int i, numBytes;

    printf("Sequential write of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);

    openFile = fileSystem->Open(FileName, "");
    if (openFile == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	return;
    }

    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
	if (numBytes < 10) {
	    printf("Perf test: unable to write %s\n", FileName);
	    delete openFile;
	    return;
	}
    }
    delete openFile;	// close file
}

static void 
FileRead()
{
    OpenFile *openFile;    
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);

    if ((openFile = fileSystem->Open(FileName, "")) == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	delete [] buffer;
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(buffer, ContentSize);
	if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
	    printf("Perf test: unable to read %s\n", FileName);
	    delete openFile;
	    delete [] buffer;
	    return;
	}
    }
    delete [] buffer;
    delete openFile;	// close file
}

void
PerformanceTest()
{
    printf("Starting file system performance test:\n");
    //stats->Print();
    if (!fileSystem->Create(FileName, 128, "")) {
      	printf("Perf test: can't create %s\n", FileName);
      	//return;
    }
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FileName, "")) {
      printf("Perf test: unable to remove %s\n", FileName);
      return;
    }
    //stats->Print();
}

void DirectoryTest()
{
	fileSystem->CreateDirectory("dir1", "") ;
	fileSystem->CreateDirectory("dir2", "") ;
	fileSystem->List("") ;
	fileSystem->Create("File1", 100, "dir1/") ;
	fileSystem->CreateDirectory("dir3", "dir2/") ;
	fileSystem->List("dir1") ;
	fileSystem->List("dir2") ;
	fileSystem->Create("File3", 100, "dir2/dir3/") ;
	fileSystem->List("dir2/dir3") ;
}

void ForkFunction(int arg)
{
	OpenFile *openFile = NULL ;    
    int i, numBytes;
	if( currentThread->getTid() == 2 ){
	
    	if (!fileSystem->Create(FileName, 128, "")) {
      		printf("Perf test: can't create %s\n", FileName);
      		//return;
    	}
    }
    while (openFile == NULL) {
		openFile = fileSystem->Open(FileName, "");
		currentThread->Yield() ;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
		if (numBytes < 10) {
	    	printf("Perf test: unable to write %s\n", FileName);
	    	delete openFile;
	    	return;
		}
    }
    delete openFile ;
    openFile = NULL ;
    while (openFile == NULL) {
		openFile = fileSystem->Open(FileName, "");
		currentThread->Yield() ;
    }
    currentThread->Yield() ;
    char *buffer = new char[ContentSize];
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(buffer, ContentSize);
        if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
	    	printf("Perf test: unable to read %s, %s\n", FileName, buffer);
	    	delete openFile;
	    	delete [] buffer;
	    	return;
		}
    }
    delete [] buffer;
    delete openFile;
    currentThread->Yield() ;
    if (!fileSystem->Remove(FileName, "")) {
      printf("Perf test: unable to remove %s\n", FileName);
      return;
    }
}

SynchConsole *synchConsole ;
Semaphore *in, *out ;
OpenFile * pipe ;

void PipeThread1(int arg)
{
	while(1)
	{
		char ch = synchConsole->GetChar() ;
		out->P() ;
		pipe->WriteAt(&ch, 1, 0) ;
		in->V() ;
		if(ch == 'q') break ;
	}
}
void PipeThread2(int arg)
{
	while(1)
	{
		char ch ;
		in->P() ;
		pipe->ReadAt(&ch, 1, 0) ;
		out->V() ;
		synchConsole->PutChar(ch) ;
		if(ch == 'q') break ;
	}
}

void FileThreadTest()
{
/*	
	for(int i = 0 ; i < 2 ; i ++)
	{
		Thread * thread ;
		if( i == 0 ) thread = new Thread("Test2", 1) ;
		else thread = new Thread("Test3", 1) ;
		thread->Fork(ForkFunction, 0) ; 
	}
*/
	synchConsole = new SynchConsole(NULL, NULL) ;
	in = new Semaphore("in", 0) ;
	out = new Semaphore("out", 1) ;
	fileSystem->Create("pipe", 128, "") ;
	pipe = fileSystem->Open("pipe", "") ;
	Thread *t1 = new Thread("Test2", 1) ;
	Thread *t2 = new Thread("Test3", 1) ;
	t1->Fork(PipeThread1, 0) ;
	t2->Fork(PipeThread2, 0) ;
}
