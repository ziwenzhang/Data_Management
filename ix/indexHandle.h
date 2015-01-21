//
// ix_file_handle.h
//
//   Index Manager Component Interface
//


#include "../rbf/pfm.h"
#include "../rbf/rbfm.h"
#include "btree_node.h"
//#include "ix_error.h"

//
// IX_FileHdr: Header structure for files
//
struct IX_FileHdr {
  int numPages;      // # of pages in the file
  PageNum rootPgNum;  // addr of root page
  int treeHeight;        // height of btree
  AttrType attrType;
};

const int IX_PAGE_LIST_END = -1;

//
// IX_IndexHandle: IX Index File interface
//
class indexHandle {
  //friend class IX_Manager;

 public:
  indexHandle();
  ~indexHandle();
  
  // TODO
  RC openIndex(FileHandle &fHandle);
  RC closeIndex();

  void setFileHandle(FileHandle *fHandle) {this->fileHandle = fHandle; };

  // Insert a new index entry
  RC insertEntry(FileHandle &fileHandle, const Attribute &attribute, const void *key, const RID &rid);
  
  // Delete a new index entry
  RC deleteEntry(FileHandle &fileHandle, const Attribute &attribute, const void *key, const RID &rid);
  
  RC getRIDbyKey(void *key, RID &rid);

  RC getFileHeader(FileHandle &fileHandle);

  bool hdrChanged()  { return bHdrChanged; }
  int getNumPages()  { return hdr.numPages; }
  AttrType getAttrType()  { return hdr.attrType; }
  //int GetAttrLength() const { return hdr.attrLength; }

  // In: fileHandle
  // Out: btreeNode, pageNum
  RC allocateNode(FileHandle &fileHandle, BtreeNode *&btreeNode, int &pageNum);
  RC disposeNode(FileHandle &fileHandle, BtreeNode *btreeNode);

  // return NULL if the key is bad
  // otherwise return a pointer to the leaf node where key might go
  // also populates the path member variable with the path
  BtreeNode* findLfNode(const void *key);
  BtreeNode* findLeftmostNode();
  //BtreeNode* findLargestLfNode();

  BtreeNode* fetchNode(PageNum pageNum);
  BtreeNode* fetchNode(RID rid);

  // get/set height
  int getHeight();
  void setHeight(const int&);

  BtreeNode* getRoot() ;

  RC writeHdrPage(FileHandle &fileHandle);
  BtreeNode ** getPath();

 private:

  IX_FileHdr hdr;
  FileHandle *fileHandle;
  bool bHdrChanged;
  BtreeNode *root; // root in turn points to the other nodes
  BtreeNode **path; // list of nodes that is the path to leaf as a
                     // result of a search.
  int* pathPos; // the position in the parent in the path that points to
              // the child node.

  //void * treeLargest; // largest key in the entire tree
};

