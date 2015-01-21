#include "pfm.h"
#include <sys/stat.h>


unsigned getDataPageOffset(PageNum pageNum)
{
	return pageNum / PAGE_DIRECTORY_MAX_SIZE + 1 + pageNum;
}

unsigned getDirPageOffset(PageNum pageNum)
{
	return (pageNum / PAGE_DIRECTORY_MAX_SIZE) * DIRECTORY_PAGE_INDEX;
}

unsigned getOffsetInDirPage(PageNum pageNum)
{
	return pageNum % PAGE_DIRECTORY_MAX_SIZE;
}

unsigned getDirPageNum(PageNum pageNum)
{
	return (pageNum / PAGE_DIRECTORY_MAX_SIZE);
}

bool fileExists(string fileName)
{
    struct stat stFileInfo;
    if(stat(fileName.c_str(), &stFileInfo) == 0) return true;
    else return false;
}

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
	delete _pf_manager;
	_pf_manager = 0;
}


RC PagedFileManager::createFile(const char *fileName)
{
	RC rc = -1;
	FILE *pFile;
	if (fileExists(fileName))	//check if the file has been created
		cerr << "The file name has already existed.";
	else
	{
		pFile = fopen(fileName, "wb");	//open in write bit mode
		if (pFile != NULL)
		{
			char * tempPage = new char[PAGE_SIZE];
			int temp = 0;
			//nDataPage
			memcpy(tempPage + PAGE_SIZE - sizeof(int), &temp, sizeof(int));
			//more
			memcpy(tempPage + PAGE_SIZE - 2 * sizeof(int), &temp, sizeof(int));
			if (fseek(pFile, 0, SEEK_SET) != 0)
				return rc;
			if (fwrite(tempPage, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
				return rc;
			fclose(pFile);
			rc = 0;
			delete []tempPage;
			tempPage = NULL;
		}
	}
	return rc;
}


RC PagedFileManager::destroyFile(const char *fileName)
{
	RC rc = -1;
	if (!fileExists(fileName))
		cerr<<"The file name does not exist.";
	else
	{
		if (remove(fileName) != 0)
			cerr << "Error deleting file.";
		else
			rc = 0;
	}
	return rc;
}


RC PagedFileManager::openFile(const char *fileName, FileHandle &fileHandle)
{
    RC rc = -1;
	fileHandle.pFile = fopen(fileName, "r+b");	//open in read write bit mode
    if (fileHandle.pFile != NULL)
    {
    	char *tempPage = new char[PAGE_SIZE];
    	unsigned temp;
    	for(unsigned i = 0; ; i++)
    	{
    		if (fseek(fileHandle.pFile, i * DIRECTORY_PAGE_INDEX, SEEK_SET) != 0)
				return rc;
			if (fread(tempPage, 1, PAGE_SIZE, fileHandle.pFile) != PAGE_SIZE)
				return rc;
			//nDataPage
			memcpy(&temp, tempPage + PAGE_SIZE - sizeof(int), sizeof(int));
			fileHandle.totalDataPageNum += temp;
			fileHandle.totalDirectoryPageNum += 1;
			memcpy(&temp, tempPage + PAGE_SIZE - 2 * sizeof(int), sizeof(int));
			if (temp == 0)
				break;
    	}
    	rc = 0;
    	delete []tempPage;
    	tempPage = NULL;
    }
	return rc;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
	RC rc = -1;
	if (fileHandle.pFile != NULL)
	{
		fclose(fileHandle.pFile);
		fileHandle.pFile = NULL;
		rc = 0;
	}
	return rc;
}


FileHandle::FileHandle()
{
	pFile = NULL;	//Initialize pointer pFile to NULL pointer
	totalDataPageNum = 0;
	totalDirectoryPageNum = 0;
}


FileHandle::~FileHandle()
{
	if (pFile != NULL)
	{
		fclose(pFile);
		pFile = NULL;
	}
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
	RC rc = -1;

	if (pFile == NULL)
		return rc;
	if (pageNum >= getNumberOfPages())
		return rc;
	if (fseek(pFile, getDataPageOffset(pageNum) * PAGE_SIZE, SEEK_SET) != 0)
		return rc;
	if (fread(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
		return rc;

	rc = 0;
    return rc;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
	RC rc = -1;

	if (pFile == NULL)
		return rc;
	if (pageNum >= getNumberOfPages())
		return rc;
	if (fseek(pFile, getDataPageOffset(pageNum) * PAGE_SIZE, SEEK_SET) != 0)
		return rc;
	if (fwrite(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
		return rc;
	fflush(pFile);

	rc = 0;
	return rc;
}

RC FileHandle::readDirectoryPage(PageNum dirPageNum, void *data)
{
	RC rc = -1;

	if (pFile == NULL)
		return rc;
	if (dirPageNum >= totalDirectoryPageNum)
		return rc;
	if (fseek(pFile, dirPageNum * DIRECTORY_PAGE_INDEX * PAGE_SIZE, SEEK_SET) != 0)
		return rc;
	if (fread(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
		return rc;

	rc = 0;
	return rc;

}

RC FileHandle::writeDirectoryPage(PageNum dirPageNum, const void *data)
{
	RC rc = -1;

	if (pFile == NULL)
		return rc;
	if (dirPageNum >= totalDirectoryPageNum)
		return rc;
	if (fseek(pFile, dirPageNum * DIRECTORY_PAGE_INDEX * PAGE_SIZE, SEEK_SET) != 0)
		return rc;
	if (fwrite(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
		return rc;

	rc = 0;
	return rc;

}

RC FileHandle::setMoreDirPage(PageNum dirPageNum, int more)
{
	RC rc = -1;
	char *tempPage = new char[PAGE_SIZE];

	if (readDirectoryPage(dirPageNum, tempPage) !=0)
		return rc;

	memcpy(tempPage + PAGE_SIZE - 2 * sizeof(int), &more, sizeof(int));

	if (writeDirectoryPage(dirPageNum, tempPage) !=0)
		return rc;

	delete []tempPage;
	tempPage = NULL;
	rc = 0;
	return rc;
}

RC FileHandle::updateNumOfFreeSpace(PageNum pageNum, int change)
{
	RC rc = -1;
	char *tempPage = new char[PAGE_SIZE];
	int nFreeSpace;

	if (readDirectoryPage(getDirPageNum(pageNum), tempPage) != 0)
		return rc;

	//Make the change in given data page's nFreeSpace
	memcpy(&nFreeSpace, tempPage + getOffsetInDirPage(pageNum) * sizeof(int), sizeof(int));
	nFreeSpace += change;
	memcpy(tempPage + getOffsetInDirPage(pageNum) * sizeof(int), &nFreeSpace, sizeof(int));

	if (writeDirectoryPage(getDirPageNum(pageNum), tempPage) != 0)
		return rc;

	delete []tempPage;
	tempPage = NULL;
	rc = 0;
	return rc;
}

RC FileHandle::setNumOfFreeSpace(PageNum pageNum, unsigned nFreeSpace)
{
	RC rc = -1;
	char *tempPage = new char[PAGE_SIZE];

	if (readDirectoryPage(getDirPageNum(pageNum), tempPage) != 0)
		return rc;

	memcpy(tempPage + getOffsetInDirPage(pageNum) * sizeof(int), &nFreeSpace, sizeof(int));

	if (writeDirectoryPage(getDirPageNum(pageNum), tempPage) != 0)
		return rc;

	delete []tempPage;
	tempPage = NULL;
	rc = 0;
	return rc;
}

RC FileHandle::appendPage(const void *data)
{
	if (pFile == NULL)
		return -1;
	char *tempPage = new char[PAGE_SIZE];
	int temp = PAGE_SIZE - 2 * sizeof(int);
	unsigned nextPage = totalDataPageNum;
	if (nextPage / PAGE_DIRECTORY_MAX_SIZE + 1 > totalDirectoryPageNum)
	{
		//New data page cannot be inserted into the existing Page Directory
		//Need to append a new Directory Page first and then append the data page

		//Change the more field in the directory page before the newly insert one
		if(this->setMoreDirPage(totalDirectoryPageNum - 1, 1) != 0)
			return -1;

		//Append new directory page
		//Add nFreeSpace in Page Directory for new data page
		memcpy(tempPage + getOffsetInDirPage(nextPage) * sizeof(int), &temp, sizeof(int));
		//Change the number of Entry in this Directory Page
		temp = 1;
		memcpy(tempPage + PAGE_SIZE - sizeof(int), &temp, sizeof(int));
		temp = 0;
		memcpy(tempPage + PAGE_SIZE - 2 * sizeof(int), &temp, sizeof(int));
		if (fseek(pFile, 0, SEEK_END) != 0)
			return -1;
		if (fwrite(tempPage, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
			return -1;
		totalDirectoryPageNum += 1;

		//Append new data page then
		if (fseek(pFile, 0, SEEK_END) != 0)
			return -1;
		if (fwrite(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
			return -1;
		totalDataPageNum += 1;
	}
	else
	{
		//New data page can be inserted into the existing Page Directory
		readDirectoryPage(getDirPageNum(nextPage), tempPage);

		//Add nFreeSpace in Page Directory for new data page
		memcpy(tempPage + getOffsetInDirPage(nextPage) * sizeof(int), &temp, sizeof(int));
		//Change the number of Entry in this Directory Page
		memcpy(&temp, tempPage + PAGE_SIZE - sizeof(int), sizeof(int));
		temp += 1;
		memcpy(tempPage + PAGE_SIZE - sizeof(int), &temp, sizeof(int));

		//Write this Directory Page back
		writeDirectoryPage(getDirPageNum(nextPage), tempPage);

		totalDataPageNum += 1;

		//Append new data page
		if (fseek(pFile, 0, SEEK_END) != 0)
			return -1;
		if (fwrite(data, 1, PAGE_SIZE, pFile) != PAGE_SIZE)
			return -1;
	}
	delete []tempPage;
	tempPage = NULL;
	fflush(pFile);
	return 0;
}


unsigned FileHandle::getNumberOfPages()
{
    return totalDataPageNum;
}
