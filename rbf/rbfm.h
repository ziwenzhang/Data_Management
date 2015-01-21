#ifndef _rbfm_h_
#define _rbfm_h_

#include <string>
#include <vector>
#include "stdlib.h"

#include "pfm.h"

using namespace std;

#define RECORD_HEADER_SIZE 9 //rid forward address

#define FREE_SPACE (PAGE_SIZE - 4) //freeSpace
#define SLOT_NUM (PAGE_SIZE - 8) //slotNum
#define LENGTH_ENTRY (PAGE_SIZE - 12) //length of slot
#define OFFSET_ENTRY (PAGE_SIZE - 16) //offset of slot

// slot entry
typedef struct {
	unsigned offset;
	unsigned length;
} SlotEntry;

// slot directory in each page
typedef struct {
	unsigned freeSpace;
	unsigned slotNum;
	SlotEntry* slots;
} SlotDirectory;


// Record ID
typedef struct recordID
{
	int pageNum;
	int slotNum;
	recordID(int p, int s)
	{
		pageNum = p;
		slotNum = s;
	}
	recordID()
	{
		pageNum = 0;
		slotNum = 0;
	}
	recordID& operator = (const recordID &rhs)
	{
		this->pageNum = rhs.pageNum;
		this->slotNum = rhs.slotNum;
		return *this;
	}
	bool operator==(const recordID & rhs) const
	{
		return (rhs.pageNum == this->pageNum && rhs.slotNum == this->slotNum);
	}
}RID;

//record header
typedef struct {
	char fwdFlag;
	RID fwdRID;
} RecordHeader;


// Attribute
typedef enum { TypeInt = 0, TypeReal, TypeVarChar } AttrType;

typedef unsigned AttrLength;

struct Attribute {
    string   name;     // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length
};

// Comparison Operator (NOT needed for part 1 of the project)
typedef enum { EQ_OP = 0,  // =
           LT_OP,      // <
           GT_OP,      // >
           LE_OP,      // <=
           GE_OP,      // >=
           NE_OP,      // !=
           NO_OP       // no condition
} CompOp;



/****************************************************************************
The scan iterator is NOT required to be implemented for part 1 of the project 
*****************************************************************************/

# define RBFM_EOF (-1)  // end of a scan operator

// RBFM_ScanIterator is an iteratr to go through records
// The way to use it is like the following:
//  RBFM_ScanIterator rbfmScanIterator;
//  rbfm.open(..., rbfmScanIterator);
//  while (rbfmScanIterator(rid, data) != RBFM_EOF) {
//    process the data;
//  }
//  rbfmScanIterator.close();

class RecordBasedFileManager;

class RBFM_ScanIterator {
public:
	RBFM_ScanIterator()
	{
		iterator = 0;
	};

	~RBFM_ScanIterator()
	{
		for (vector< void * >::iterator iter = dataProjection.begin(); iter != dataProjection.end(); ++iter )
		{
			free( ( void * )( *iter ) );
		}
	};

	void addAttribute(const Attribute &attr);
	void addRid(const RID &rid);
	void addData(void *projection, unsigned length);
	void resetState();


	// "data" follows the same format as RecordBasedFileManager::insertRecord()
	RC getNextRecord(RID &rid, void *data);
	RC close(){return -1;};
private:
	vector<RID> rids;
	vector<int> lengths;
	vector<Attribute> recordDescriptor;
	vector<void *> dataProjection;
	unsigned iterator;
};


class RecordBasedFileManager
{
public:
	PagedFileManager *pfm;

  static RecordBasedFileManager* instance();

  int findPgToInsert (int spaceNeeded, FileHandle &fileHandle);

  RC createFile(const string &fileName);
  
  RC destroyFile(const string &fileName);
  
  RC openFile(const string &fileName, FileHandle &fileHandle);
  
  RC closeFile(FileHandle &fileHandle);


  //  Format of the data passed into the function is the following:
  //  1) data is a concatenation of values of the attributes
  //  2) For int and real: use 4 bytes to store the value;
  //     For varchar: use 4 bytes to store the length of characters, then store the actual characters.
  //  !!!The same format is used for updateRecord(), the returned data of readRecord(), and readAttribute()
  RC insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid);
  RC insertFwdedRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid, const RID oldRID);

  RC readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data);
  
  // This method will be mainly used for debugging/testing
  RC printRecord(const vector<Attribute> &recordDescriptor, const void *data);


/**************************************************************************************************************************************************************
***************************************************************************************************************************************************************
IMPORTANT, PLEASE READ: All methods below this comment (other than the constructor and destructor) are NOT required to be implemented for part 1 of the project
***************************************************************************************************************************************************************
***************************************************************************************************************************************************************/
  RC deleteRecords(FileHandle &fileHandle);

  RC deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid);

  // Assume the rid does not change after update
  RC updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid);

  RC readAttribute(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, const string attributeName, void *data);

  RC reorganizePage(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const unsigned pageNumber);

  // scan returns an iterator to allow the caller to go through the results one by one. 
  RC scan(FileHandle &fileHandle,
      const vector<Attribute> &recordDescriptor,
      const string &conditionAttribute,
      const CompOp compOp,                  // comparision type such as "<" and "="
      const void *value,                    // used in the comparison
      const vector<string> &attributeNames, // a list of projected attributes
      RBFM_ScanIterator &rbfm_ScanIterator);


// Extra credit for part 2 of the project, please ignore for part 1 of the project
  	  RC reorganizeFile(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor);


protected:
  RecordBasedFileManager();
  ~RecordBasedFileManager();

private:
  static RecordBasedFileManager *_rbf_manager;
};

#endif
