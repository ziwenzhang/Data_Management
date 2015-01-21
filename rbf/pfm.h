#ifndef _pfm_h_
#define _pfm_h_

#include <string.h>
#include <stdio.h>
#include <iostream>
using namespace std;

typedef int RC;
typedef unsigned PageNum;

#define PAGE_SIZE 4096
#define PAGE_DIRECTORY_MAX_SIZE 1022		//The number of Data Pages that a single directory page can index
#define DIRECTORY_PAGE_INDEX 1023

bool fileExists(string fileName);
class FileHandle;


class PagedFileManager
{
public:
    static PagedFileManager* instance();                     // Access to the _pf_manager instance

    RC createFile    (const char *fileName);                         // Create a new file
    RC destroyFile   (const char *fileName);                         // Destroy a file
    RC openFile      (const char *fileName, FileHandle &fileHandle); // Open a file
    RC closeFile     (FileHandle &fileHandle);                       // Close a file

protected:
    PagedFileManager();                                   // Constructor
    ~PagedFileManager();                                  // Destructor

private:
    static PagedFileManager *_pf_manager;
};


class FileHandle
{
public:
    FILE *pFile;
    unsigned totalDataPageNum;
    unsigned totalDirectoryPageNum;

	FileHandle();                                                    // Default constructor
    ~FileHandle();                                                   // Deconstructor

    FileHandle(FileHandle &f)
	{
		//pFile = new FILE();
		FILE *pfile = f.pFile;
		pFile = pfile;
		totalDataPageNum = f.totalDataPageNum;
		totalDirectoryPageNum = f.totalDirectoryPageNum;
	}

    RC readDirectoryPage(PageNum dirPageNum, void *data);				// Get a specific directory page, dirPageNum counts from 0
    RC writeDirectoryPage(PageNum dirPageNum, const void *data);		// Write a specific directory page, dirPageNum counts from 0
    RC updateNumOfFreeSpace(PageNum pageNum, int change);				// Set the a given data page's number of Free Space in the directory page by using difference between the changes
    RC setNumOfFreeSpace(PageNum pageNum, unsigned nFreeSpace);				// Set the a given data page's number of Free Space in the directory page
    RC setMoreDirPage(PageNum dirPageNum, int more);					// Set the more field in a given directory page

    RC readPage(PageNum pageNum, void *data);                           // Get a specific page
    RC writePage(PageNum pageNum, const void *data);                    // Write a specific page
    RC appendPage(const void *data);                                    // Append a specific page
    unsigned getNumberOfPages();                                        // Get the number of pages in the file

 };

 #endif
