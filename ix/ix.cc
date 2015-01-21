
#include "ix.h"

IndexManager* IndexManager::_index_manager = 0;

IndexManager* IndexManager::instance()
{
    if(!_index_manager)
        _index_manager = new IndexManager();

    return _index_manager;
}

IndexManager::IndexManager()
{
	pfm = PagedFileManager::instance();
	//ix_handle = new indexHandle();
}

IndexManager::~IndexManager()
{
	if (!_index_manager) {
		delete _index_manager;
		_index_manager = NULL;
	}
}

RC IndexManager::createFile(const string &fileName)
{
	RC rc = -1;

	rc = pfm->createFile(fileName.c_str());
	if (rc != 0)
		return rc;

	// add hdr page
	char *tempPage = new char[PAGE_SIZE];
	memset(tempPage, 0, PAGE_SIZE);

	FileHandle fileHandle;
	rc = pfm->openFile(fileName.c_str(), fileHandle);

	fileHandle.appendPage(tempPage);

	return rc;
}

RC IndexManager::destroyFile(const string &fileName)
{
	RC rc = -1;

	rc = pfm->destroyFile(fileName.c_str());
	if (rc != 0)
		return rc;

	return rc;
}

RC IndexManager::openFile(const string &fileName, FileHandle &fileHandle)
{
	RC rc = -1;

	rc = pfm->openFile(fileName.c_str(), fileHandle);
	if (rc != 0)
		return rc;

	return rc;
}

RC IndexManager::closeFile(FileHandle &fileHandle)
{
	RC rc = -1;

	rc = pfm->closeFile(fileHandle);
	if (rc != 0)
		return rc;

	return rc;
}

RC IndexManager::insertEntry(FileHandle &fileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
	RC rc = -1;

	indexHandle *ix_handle = new indexHandle();

	rc = ix_handle->openIndex(fileHandle);
	if (rc != 0)
		return rc;

	rc = ix_handle->insertEntry(fileHandle, attribute, key, rid);
	if (rc != 0)
		return rc;
	delete ix_handle;
	ix_handle = NULL;
	return rc;
}

RC IndexManager::deleteEntry(FileHandle &fileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
	RC rc = -1;

	indexHandle *ix_handle = new indexHandle();

	rc = ix_handle->openIndex(fileHandle);
	if (rc != 0)
		return rc;

	rc = ix_handle->deleteEntry(fileHandle, attribute, key, rid);
	if (rc != 0)
		return rc;

	delete ix_handle;
	ix_handle = NULL;
	return rc;
}

RC IndexManager::scan(FileHandle &fileHandle,
    const Attribute &attribute,
    const void      *lowKey,
    const void      *highKey,
    bool			lowKeyInclusive,
    bool        	highKeyInclusive,
    IX_ScanIterator &ix_ScanIterator)
{
	RC rc = -1;

	if (fileHandle.pFile == NULL)
	{
		return -1;
	}

	indexHandle *ix_handle = new indexHandle();

	//ix_handle->setFileHandle(fHandle);
	ix_handle->openIndex(fileHandle);

	BtreeNode *node = ix_handle->findLeftmostNode();

	do {

		int i;
		char *key = new char[PAGE_SIZE];
		RID rid;


		for (i = 0; i < node->getNumKeys(); i++) {
			rc = node->getKey(i, key);
			rid = node->getRID(i);
			if (rc == -1)
				return rc;

			bool flag = false;
			flag = fallsRange(node, key, lowKey, highKey, lowKeyInclusive, highKeyInclusive);

			if (flag == true) {
				ix_ScanIterator.addKey(key);
				ix_ScanIterator.addRID(rid);
			}
		}
		delete []key;
		key = NULL;
	} while (node->getRight() != -1 && (node = ix_handle->fetchNode(node->getRight())));


	for(int i = 0; i < ix_handle->getHeight(); i++)
	{
		if((ix_handle->getPath())[i] == node)
			node = NULL;
	}
	if(node != NULL)
	{
		delete(node);
		node = NULL;
	}
	delete(ix_handle);
	ix_handle = NULL;
	//delete(fHandle);
	//fHandle = NULL;
	return 0;
}

bool IndexManager::fallsRange(BtreeNode *node, void *key, const void *lowK, const void *highK, bool lowKeyInclusive, bool highKeyInclusive) {

	bool flag = false;

	if (lowK == NULL) {
		flag = true;
	}
	if (lowK != NULL) {
		if (lowKeyInclusive) {
			if (node->cmpKey(key, lowK) >= 0) {
				flag = true;
			}
			else {
				flag = false;
				return flag;
			}
		}
		else {
			if (node->cmpKey(key, lowK) > 0) {
				flag = true;
			}
			else {
				flag = false;
				return flag;
			}
		}

	}

	if (highK == NULL) {
		flag = true;
		return flag;
	}
	if (highK != NULL) {
		if (highKeyInclusive) {
			if (node->cmpKey(key, highK) <= 0) {
				flag = true;
			}
			else {
				flag = false;
			}
		}
		else {
			if (node->cmpKey(key, highK) < 0) {
				flag = true;
			}
			else {
				flag = false;
			}
		}
	}

	return flag;
}

IX_ScanIterator::IX_ScanIterator()
{
	iterator = 0;
	indexIteratorOpen = true;
}

IX_ScanIterator::IX_ScanIterator(AttrType &attrType)
{
	iterator = 0;
	indexIteratorOpen = true;
	this->attrType = attrType;
}

IX_ScanIterator::~IX_ScanIterator()
{
	for (vector<void *>::iterator iter = keys.begin(); iter != keys.end(); iter++)
	{
		if(*iter != NULL)
		{
			delete ((char*)*iter);
			*iter = NULL;
		}
	}
}

void IX_ScanIterator::resetState()
{
	iterator=0;

	for(vector< void * > ::iterator iter = keys.begin(); iter != keys.end(); ++iter )
	{
		if(*iter != NULL)
		{
			delete ((char*)*iter);
			*iter = NULL;
		}
	}
	indexIteratorOpen = true;
	rids.clear();
	keys.clear();
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key)
{
	unsigned size = keys.size();

	if (iterator >= size)
	{
		return IX_EOF;
	}

	// rid
	rid = rids.at(iterator);

	// key
	int varcharLength = 0;
	if(attrType == TypeVarChar)
	{
		memcpy(&varcharLength, (char *)(keys.at(iterator)), sizeof(int));
	}
	memcpy((char *)key, keys.at(iterator), sizeof(int) + varcharLength);
	//key = keys.at(iterator);

	iterator++;

	RC rc = 0;
	return rc;
}

RC IX_ScanIterator::close()
{
	if(indexIteratorOpen == true)
	{
		indexIteratorOpen = false;
		resetState();
		return 0;
	}
	return -1;
}

void IX_ScanIterator::addKey(void *key)
{
	int length = 0;
	if(attrType == TypeVarChar)
	{
		memcpy(&length, (char *)key, sizeof(int));
		length += sizeof(int);
	}
	else
		length = sizeof(int);
	char * temp = new char[length];
	memcpy(temp, (char *)key, length);
	keys.push_back(temp);
}

void IX_PrintError (RC rc)
{
}
