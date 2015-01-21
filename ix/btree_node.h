#ifndef BTREE_NODE_H
#define BTREE_NODE_H

#include "../rbf/pfm.h"
#include "../rbf/rbfm.h"
#include "ix_error.h"
#include <vector>

// Key has to be a single attribute of type attrType and length attrLength

class BtreeNode
{
public:
	// if newPage is false then the page ph is expected to contain an
	// existing btree node, otherwise a fresh node is assumed.
	BtreeNode(AttrType attrType, FileHandle& fileHandle, PageNum pageNum);
	//RC ResetBtreeNode(PF_PageHandle& ph, const BtreeNode& rhs);
	~BtreeNode();
	int destroy();

	friend class IX_IndexHandle;
	RC isValid() const;

	RC writeBackPage();

	// structural setters/getters - affect PF_page composition
	int getNumKeys();
	int setNumKeys(int newNumKeys);
	int getNumByte();
	int setNumByte(int newNumByte);
	int getNumRIDs();
	int setNumRIDs(int newNumRIDs);
	int getLeft();
	int setLeft(int p);
	int getRight();
	int setRight(int p);

	RC getKey(int pos, void* key) const;
	int setKey(int pos, const void* newkey);
	int copyKey(int pos, void* toKey) const;


	// return 0 if insert was successful
	// return -1 if there is no space
	int insertKey(const void* newkey, const RID& newrid);

	// return 0 if remove was successful
	// return -1 if key does not exist
	// kpos is optional - will remove from that position if specified
	// if kpos is specified newkey can be NULL
	int removeKey(const void* newkey, int kpos = -1);

	// exact match functions

	// return position if key already exists at position
	// if there are dups - returns rightmost position unless an RID is
	// specified.
	// if optional RID is specified, will only return a position if both
	// key and RID match.
	// return -1 if there was an error or if key does not exist
	int findKey(const void* key, const RID& r = RID(-1,-1)) const;

	RID findRID(const void* key) const;
	// get rid for given position
	// return (-1, -1) if there was an error or pos was not found
	RID getRID(const int pos) const;
	int setRID(int pos, const RID& rid);

	// find a position instead of exact match
	int findKeyPosition(const void* key) const;
	RID findRIDAtPosition(const void* key) const;

	// split or merge this node with rhs node
	RC split(BtreeNode* rhs);
	RC merge(BtreeNode* rhs);


	// get/set pageRID
	int getPageNum() const;
	void setPageNum(const int& pageNum);

	int cmpKey(const void * k1, const void * k2) const;
	bool isSorted() const;
	void* largestKey() const;

private:
	RC getKeyDir();
	FileHandle *fileHandle;

	int numKeys;
	int numRIDs;
	AttrType attrType;
	int numByte;
	PageNum pgNum;

	int* keyDir;
	char * keys; // should not be accessed directly as keys[] but with SetKey()
	RID * rids;
};


#endif // BTREE_NODE_H
