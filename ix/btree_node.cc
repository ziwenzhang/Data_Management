
#include "btree_node.h"
#include "string.h"
#include <assert.h>

RC BtreeNode::writeBackPage()
{
	RC rc = -1;
	char * tempPage = new char[PAGE_SIZE];
	int keysLength = numByte - numRIDs * sizeof(RID) - 4 * sizeof(int);
	memset(tempPage, 0, PAGE_SIZE);
	memcpy(tempPage, keys, keysLength);
	memcpy(tempPage + PAGE_SIZE - numRIDs * sizeof(RID) - 4 * sizeof(int), (char *)rids, numRIDs * sizeof(RID) + 4 * sizeof(int));
	rc = fileHandle->writePage(pgNum, tempPage);
	if(rc != 0)
	{
		delete []tempPage;
		tempPage = NULL;
		return rc;
	}
	rc = fileHandle->setNumOfFreeSpace(pgNum, PAGE_SIZE - numByte);
	if(rc != 0)
	{
		delete []tempPage;
		tempPage = NULL;
		return rc;
	}
	delete []tempPage;
	tempPage = NULL;
	rc = 0;
	return rc;
}

RC BtreeNode::getKeyDir()
{
	assert(isValid() == 0);
	RC rc = -1;
	if(keys == NULL)
		return rc;

	unsigned offset = 0;
	memcpy(&keyDir[0], &offset, sizeof(int));
	for(int i = 0; i < numKeys; i++)
	{
		if(attrType == TypeVarChar)
		{
			int varcharLength = 0;
			memcpy(&varcharLength, keys + offset, sizeof(int));
			offset += sizeof(int);
			offset += varcharLength;
			memcpy(&keyDir[i+1], &offset, sizeof(int));
		}
		else if(attrType == TypeInt)
		{
			offset += sizeof(int);
			memcpy(&keyDir[i+1], &offset, sizeof(int));
		}
		else	// TypeFloat
		{
			offset += sizeof(float);
			memcpy(&keyDir[i+1], &offset, sizeof(float));
		}
	}
	rc = 0;
	return rc;
}

BtreeNode::BtreeNode(AttrType attrType, FileHandle& fileHandle, PageNum pageNum)
:fileHandle(NULL), attrType(attrType), pgNum(pageNum), keyDir(NULL), keys(NULL), rids(NULL)
{
	RC rc = -1;
	this->fileHandle = new FileHandle();
	*(this->fileHandle) = fileHandle;
	char * tempPage = new char[PAGE_SIZE];
	rc = this->fileHandle->readPage(pageNum, tempPage);
	if(rc != 0)
		return;

	memcpy(&numKeys, tempPage + PAGE_SIZE - 3 * sizeof(int), sizeof(int));
	memcpy(&numByte, tempPage + PAGE_SIZE - 4 * sizeof(int), sizeof(int));
	numRIDs = numKeys > 0 ? numKeys + 1 : 0;

	// Initialize pointers in a node
	keys = new char[PAGE_SIZE];
	rids = new RID[numRIDs + 2];
	keyDir = new int[numKeys + 1];

	// Load all rids, # of Bytes, # of keys, left, and right into *rids
	int ridsLength = (numRIDs + 2) * sizeof(RID);
	memcpy((char *)rids, tempPage + PAGE_SIZE - ridsLength, ridsLength);

	// Load all key values in the beginning of a page into *keys
	int keysLength = numByte > 0 ? numByte - ridsLength : numByte;
	memcpy(keys, tempPage, keysLength);

	rc = getKeyDir();
	if(rc != 0)
		return;

	if(numKeys == 0)
	{
		setNumByte(getNumByte() + 4 * sizeof(int));
		setLeft(-1);
		setRight(-1);
	}
	delete []tempPage;
	tempPage = NULL;
}

BtreeNode::~BtreeNode()
{
	writeBackPage();
	destroy();
}

int BtreeNode::destroy()
{
	assert(isValid() == 0);
	// TODO think whether we need this constraint
	/*if(numKeys != 0)
		return -1;*/
	if(keys != NULL)
	{
		delete []keys;
		keys = NULL;
	}

	if(rids != NULL)
	{
		delete []rids;
		rids = NULL;
	}

	if(keyDir != NULL)
	{
		delete []keyDir;
		keyDir = NULL;
	}

	return 0;
}


RC BtreeNode::isValid() const
{
	if (numByte < 0)
		return IX_INVALIDSIZE;

	bool ret = true;
	ret = ret && (keys != NULL);
	assert(ret);
	ret = ret && (rids != NULL);
	assert(ret);
	ret = ret && (numKeys >= 0);
	assert(ret);
	ret = ret && (numByte <= PAGE_SIZE);
	if(!ret)
		cerr << "numByte " << numByte << " numKeys was  " << numKeys << endl;
	return ret ? 0 : IX_BADIXPAGE;
}

int BtreeNode::getNumKeys()
{
	assert(isValid() == 0);
	memcpy(&numKeys ,(char *)rids + numRIDs * sizeof(RID) + sizeof(int),  sizeof(int));
	return numKeys;
}

int BtreeNode::setNumKeys(int newNumKeys)
{
	memcpy((char *)rids + numRIDs * sizeof(RID) + sizeof(int), &newNumKeys, sizeof(int));
	numKeys = newNumKeys;
	assert(isValid() == 0);
	return 0;
}

int BtreeNode::getNumByte()
{
	assert(isValid() == 0);
	memcpy(&numByte ,(char *)rids + numRIDs * sizeof(RID),  sizeof(int));
	return numByte;
}

int BtreeNode::setNumByte(int newNumByte)
{
	memcpy((char *)rids + numRIDs * sizeof(RID), &newNumByte, sizeof(int));
	numByte = newNumByte;
	assert(isValid() == 0);
	return 0;
}

int BtreeNode::getNumRIDs()
{
	return numRIDs;
}

int BtreeNode::setNumRIDs(int newNumRIDs)
{
	numRIDs = newNumRIDs;
	return 0;
}

int BtreeNode::getLeft()
{
	assert(isValid() == 0);
	void * loc = (char *)rids + numRIDs * sizeof(RID) + 2 * sizeof(int);
	return *((int *)loc);
}

int BtreeNode::setLeft(int p)
{
	assert(isValid() == 0);
	memcpy((char *)rids + numRIDs * sizeof(RID) + 2 * sizeof(int),
			&p,
			sizeof(int));
	return 0;
}

int BtreeNode::getRight()
{
	assert(isValid() == 0);
	void * loc = (char *)rids + numRIDs * sizeof(RID) + 3 * sizeof(int);
	return *((int *)loc);
}

int BtreeNode::setRight(int p)
{
	assert(isValid() == 0);
	memcpy((char *)rids + numRIDs * sizeof(RID) + 3 * sizeof(int),
			&p,
			sizeof(int));
	return 0;
}


RC BtreeNode::getKey(int pos, void* key) const
{
	assert(isValid() == 0);
	assert(pos >= 0 && pos < numKeys);
	if (pos >= 0 && pos < numKeys)
	{
		memcpy((char *)key, keys + keyDir[pos], keyDir[pos + 1] - keyDir[pos]);
		return 0;
	}
	else
	{
		return -1;
	}
}

int BtreeNode::setKey(int pos, const void* newkey)
{
	assert(isValid() == 0);

	if(attrType == TypeVarChar)
	{
		int varcharLength = 0;
		memcpy(&varcharLength, newkey, sizeof(int));
		if(pos == numKeys)
		{
			if(numByte + varcharLength + sizeof(int) > PAGE_SIZE)
			{
				// The case that a node cannot add the newkey at the end
				return -1;
			}
			else
			{
				// set a new key at the end of the last key in this node
				memcpy(keys + keyDir[pos], newkey, sizeof(int) + varcharLength);
				int *temp = new int[numKeys + 2];
				memcpy(temp, keyDir, sizeof(int) * (numKeys + 1));
				temp[numKeys + 1] = keyDir[pos] + sizeof(int) + varcharLength;
				setNumByte(getNumByte() + sizeof(int) + varcharLength);
				delete []keyDir;
				keyDir = temp;
				temp = NULL;
				setNumKeys(getNumKeys()+1);
			}
		}
		else if(pos < numKeys)
		{
			if(numByte - (keyDir[pos+1]- keyDir[pos]) + varcharLength + sizeof(int) > PAGE_SIZE)
			{
				// After setting new key the page will run out of free space
				return -1;
			}
			else
			{
				char *temp = new char[PAGE_SIZE];
				int length = keyDir[numKeys] - keyDir[pos + 1];
				int offset = keyDir[pos];
				memcpy(temp, keys + keyDir[pos + 1], length);
				memcpy(keys + offset, newkey, varcharLength + sizeof(int));
				offset += (varcharLength + sizeof(int));
				memcpy(keys + offset, temp, length);

				setNumByte(getNumByte() - (keyDir[pos+1]- keyDir[pos]) + varcharLength + sizeof(int));
				getKeyDir();

				delete []temp;
				temp = NULL;
			}
		}
		else
		{
			// shouldn't be there
			return -1;
		}
	}
	else	//TypeInt || TypeReal
	{
		if(pos == numKeys)
		{
			if(numByte + sizeof(int) > PAGE_SIZE)
			{
				return -1;
			}
			else
			{
				// pos should not be greater than numKeys
				//numKeys += 1;
				//setNumKeys(getNumKeys() + 1);
				memcpy(keys + keyDir[pos], newkey, sizeof(int));
				int *temp = new int[numKeys + 2];
				memcpy(temp, keyDir, sizeof(int) * (numKeys + 1));
				temp[numKeys + 1] = keyDir[pos] + sizeof(int);
				delete []keyDir;
				keyDir = temp;
				temp = NULL;
				setNumByte(getNumByte() + sizeof(int));
				setNumKeys(getNumKeys()+1);
			}
		}
		else if(pos < numKeys)
		{
			// keyDir doesn't need to change
			// numByte is also unchanged
			memcpy(keys + pos * sizeof(int), newkey, sizeof(int));
		}
		else
		{
			// shouldn't be there
			return -1;
		}
	}
	return 0;
}

int BtreeNode::copyKey(int pos, void* toKey) const
{
	assert(isValid() == 0);
	assert(pos >= 0 && pos < numKeys);
	if(toKey == NULL)
		return -1;
	if (pos >= 0 && pos < numKeys)
	{
		memcpy((char *)toKey, keys + keyDir[pos], keyDir[pos + 1] - keyDir[pos]);
		return 0;
	}
	else
	{
		return -1;
	}
}

// return 0 if insert was successful
// return -1 if there is no space
int BtreeNode::insertKey(const void* newkey, const RID& newrid)
{
	assert(isValid() == 0);
	int varcharLength = 0;
	bool firstElem = false;
	if(attrType == TypeVarChar)
		memcpy(&varcharLength, newkey, sizeof(int));
	if(numByte + varcharLength + sizeof(int) + sizeof(RID) >= PAGE_SIZE)
		return -1;

	int i = -1;
	char *currKey = new char[PAGE_SIZE];
	for(i = numKeys - 1; i >= 0; i--)
	{
		getKey(i, currKey);
		if (cmpKey(newkey, currKey) >= 0)
			break; // go out and insert at i+1
		setKey(i + 1, currKey);
	}

	int j = -1;
	if(numRIDs == 0)
		firstElem = true;
	for(j = numRIDs - 1; j > i; j--)
	{
		setRID(j + 1, rids[j]);
	}

	if(i==j)
	{
		setKey(i + 1, newkey);
		setRID(i + 1, newrid);
		if(firstElem)
			setRID(i + 2, RID(0,0));
		//setNumKeys(getNumKeys()+1);
	}
	else
	{
		// shouldn't be there
		return -1;
	}

	assert(numKeys + 1 == numRIDs);
	// Write back node to page
	//writeBackPage();

	assert(isSorted());
	delete []currKey;
	currKey = NULL;
	return 0;
}

// return 0 if remove was successful
// return -1 if key does not exist
// kpos is optional - will remove from that position if specified
// if kpos is specified newkey can be NULL
int BtreeNode::removeKey(const void* newkey, int kpos)
{
	assert(isValid() == 0);
	int pos = -1;
	int oldNumRIDs = getNumRIDs();
	if (kpos != -1)
	{
		if (kpos < 0 || kpos >= numKeys)
			return -2;
		pos = kpos;
	}
	else
	{
		pos = findKey(newkey);
		if (pos < 0)
			return -2;
		// shift all keys after this pos
	}
	char *p = new char[PAGE_SIZE];
	int numByteChanged = keyDir[pos+1] - keyDir[pos];
	for(int i = pos; i < numKeys-1; i++)
	{
		getKey(i+1, p);
		setKey(i, p);
	}

	for(int i = pos; i < numRIDs - 1; i++)
	{
		setRID(i, rids[i+1]);
	}

	if(numKeys == 1)
	{
		setNumByte(getNumByte() - sizeof(RID));
		oldNumRIDs = getNumRIDs();
		setNumRIDs(getNumRIDs()-1);
		memcpy(p, (char *)rids + oldNumRIDs * sizeof(RID), sizeof(int) * 4);
		memcpy((char *)rids + numRIDs * sizeof(RID), p, sizeof(int) * 4);
	}

	setNumKeys(getNumKeys()-1);
	oldNumRIDs = getNumRIDs();
	setNumRIDs(getNumRIDs()-1);

	memcpy(p, (char *)rids + oldNumRIDs * sizeof(RID), sizeof(int) * 4);
	memcpy((char *)rids + numRIDs * sizeof(RID), p, sizeof(int) * 4);
	setNumByte(getNumByte() - sizeof(RID) - numByteChanged);
	// Write back node to page
	//writeBackPage();

	if(numKeys == 0)
	{
		delete []p;
		p = NULL;
		return -1;
	}
	delete []p;
	p = NULL;
	return 0;
}

// exact match functions

// return position if key already exists at position
// if optional RID is specified, will only return a position if both
// key and RID match.
// return -1 if there was an error or if key does not exist
int BtreeNode::findKey(const void* key, const RID& r) const
{
	assert(isValid() == 0);

	char* k = new char[PAGE_SIZE];
	for(int i = numKeys-1; i >= 0; i--)
	{
		if(getKey(i, k) != 0)
			return -1;
		if (cmpKey(key, k) == 0)
		{
			if(r == RID(-1,-1))
			{
				delete []k;
				k = NULL;
				return i;
			}
			else
			{ // match RID as well
				if (rids[i] == r)
				{
					delete []k;
					k = NULL;
					return i;
				}
			}
		}
	}
	delete []k;
	k = NULL;
	return -1;
}

RID BtreeNode::findRID(const void* key) const
{
	assert(isValid() == 0);
	int pos = findKey(key);
	if (pos == -1) return RID(-1,-1);
	return rids[pos];
}

// get rid for given position
// return (-1, -1) if there was an error or pos was not found
RID BtreeNode::getRID(const int pos) const
{
	assert(isValid() == 0);
	if(pos < 0 || pos > numRIDs)
		return RID(-1, -1);
	return rids[pos];
}

int BtreeNode::setRID(int pos, const RID& rid)
{
	assert(isValid() == 0);
	if(pos == numRIDs)
	{
		RID *temp = new RID[numRIDs + 3];
		memcpy(temp, rids, numRIDs * sizeof(RID));
		temp[numRIDs] = rid;
		memcpy(&temp[numRIDs + 1], rids + numRIDs, sizeof(int) * 4);
		delete []rids;
		rids = temp;
		setNumRIDs(getNumRIDs()+1);
		setNumByte(getNumByte() + sizeof(RID));
	}
	else if (pos < numRIDs)
	{
		rids[pos] = rid;
	}
	else
	{
		// shouldn't be there
		return -1;
	}
	return 0;
}

int BtreeNode::findKeyPosition(const void* key) const
{
	assert(isValid() == 0);
	char* k = new char[PAGE_SIZE];
	for(int i = numKeys-1; i >=0; i--)
	{
		if(getKey(i, k) != 0)
			return -1;
		// added == condition so that FindLeaf can return exact match and not
		// the position to the right upon matches. this affects where inserts
		// will happen during dups.
		if (cmpKey(key, k) == 0)
		{
			delete []k;
			k = NULL;
			return i;
		}
		if (cmpKey(key, k) > 0)
		{
			delete []k;
			k = NULL;
			return i+1;
		}
	}
	delete []k;
	k = NULL;
	return 0; // key is smaller than anything currently
}

RID BtreeNode::findRIDAtPosition(const void* key) const
{
	assert(isValid() == 0);
	int pos = findKeyPosition(key);
	if (pos == -1 || pos >= numRIDs) return RID(-1,-1);
	return rids[pos];
}

// split or merge this node with rhs node
RC BtreeNode::split(BtreeNode* rhs)
{
	assert(isValid() == 0);
	assert(rhs->isValid() == 0);

	int firstMovedPos = (numKeys+1)/2;
	int moveCount = (numKeys - firstMovedPos);
	// Ensure that rhs wont overflow
	if(rhs->getNumByte() + keyDir[numKeys] - keyDir[firstMovedPos] > PAGE_SIZE)
		return -1;

	char * k = new char[PAGE_SIZE];
	for (int pos = firstMovedPos; pos < numKeys; pos++)
	{
		RID r = rids[pos];
		this->getKey(pos, k);
		RC rc = rhs->insertKey(k, r);
		if(rc != 0)
		{
			delete []k;
			k = NULL;
			return rc;
		}
	}

	// TODO use range remove - faster
	for (int i = 0; i < moveCount; i++)
	{
		RC rc = this->removeKey(NULL, firstMovedPos);
		if(rc < -1)
		{
			delete []k;
			k = NULL;
			return rc;
		}
	}

	// other side will have to be set up on the outside
	rhs->setRight(this->getRight());

	this->setRight(rhs->getPageNum());
	rhs->setLeft(this->getPageNum());

	// TODO think about whether we need write back node to page here
	this->writeBackPage();
	rhs->writeBackPage();

	assert(isSorted());
	assert(rhs->isSorted());

	assert(isValid() == 0);
	assert(rhs->isValid() == 0);
	delete []k;
	k = NULL;
	return 0;
}

RC BtreeNode::merge(BtreeNode* rhs)
{
	assert(isValid() == 0);
	assert(rhs->isValid() == 0);

	// Overflow will result from merge
	if (numByte + rhs->getNumByte() - sizeof(RID) - 4 * sizeof(int) > PAGE_SIZE)
	    return -1;

	char * k = new char[PAGE_SIZE];
	for (int pos = 0; pos < rhs->getNumKeys(); pos++)
	{
		rhs->getKey(pos, k);
		RID r = rhs->getRID(pos);
		RC rc = this->insertKey(k, r);
		if(rc != 0)
			return rc;
	}

	int moveCount = rhs->getNumKeys();
	for (int i = 0; i < moveCount; i++)
	{
		RC rc = rhs->removeKey(NULL, 0);
		if(rc < -1)
			return rc;
	}

	if(this->getPageNum() == rhs->getLeft())
		this->setRight(rhs->getRight());
	else
		this->setLeft(rhs->getLeft());

	// TODO think about whether we need write back node to page here
	this->writeBackPage();
	rhs->writeBackPage();

	assert(isValid() == 0);
	assert(rhs->isValid() == 0);
	delete []k;
	k = NULL;
	return 0;
}

// get/set pageRID
int BtreeNode::getPageNum() const
{
	return pgNum;
}

void BtreeNode::setPageNum(const int& pageNum)
{
	pgNum =pageNum;
}

int BtreeNode::cmpKey(const void * k1, const void * k2) const
{
	if (attrType == TypeVarChar)
	{
		int k1Length;
		int k2Length;
		memcpy(&k1Length, k1, sizeof(int));
		memcpy(&k2Length, k2, sizeof(int));
		char *k1string = new char[k1Length+1];
		char *k2string = new char[k2Length+1];
		memcpy(k1string, (char *)k1 + sizeof(int), k1Length);
		memcpy(k2string, (char *)k2 + sizeof(int), k2Length);
		k1string[k1Length] = '\0';
		k2string[k2Length] = '\0';
		int result = strcmp(k1string, k2string);
		delete []k1string;
		delete []k2string;
		k1string = NULL;
		k2string = NULL;
		return result;
	}

	if (attrType == TypeReal)
	{
		typedef float MyType;
		if ( *(MyType*)k1 >  *(MyType*)k2 ) return 1;
		if ( *(MyType*)k1 == *(MyType*)k2 ) return 0;
		if ( *(MyType*)k1 <  *(MyType*)k2 ) return -1;
	}

	if (attrType == TypeInt)
	{
		typedef int MyType;
		if ( *(MyType*)k1 >  *(MyType*)k2 ) return 1;
		if ( *(MyType*)k1 == *(MyType*)k2 ) return 0;
		if ( *(MyType*)k1 <  *(MyType*)k2 ) return -1;
	}

	assert("should never get here - bad attrtype");
	return 0; // to satisfy gcc warnings
}

bool BtreeNode::isSorted() const
{
	assert(isValid() == 0);

	char* k1 = new char[PAGE_SIZE];
	char* k2 = new char[PAGE_SIZE];
	for(int i = 0; i < numKeys-1; i++)
	{
		getKey(i, k1);
		getKey(i+1, k2);
		if (cmpKey(k1, k2) > 0)
		{
			delete []k1;
			delete []k2;
			k1 = NULL;
			k2 = NULL;
			return false;
		}
	}
	delete []k1;
	delete []k2;
	k1 = NULL;
	k2 = NULL;
	return true;
}

void* BtreeNode::largestKey() const
{
	assert(isValid() == 0);
	// TODO the key pointer only have 100 bytes and
	if (numKeys > 0)
	{
		char * key = new char[100];
		getKey(numKeys-1, key);
		return (void*)key;
	}
	else
	{
		assert("Largest Key called when numKey <= 0");
		return NULL;
	}
}
