#include "pfm.h"
#include "rbfm.h"
#include "string.h"

#include <iostream>
using namespace std;

//Get data size of a record ****typo corrected. Changed 1 to sizeof(int)
unsigned getDataSize (const void * record, const vector<Attribute> &recordDescriptor)
{
	unsigned offset = 0;
	unsigned size = 0;
	for (unsigned i = 0; i < recordDescriptor.size(); i++)
	{
		if (recordDescriptor.at(i).type == TypeVarChar)
		{
			memcpy(&size, (char *) record + offset, sizeof(int));
			offset = offset +  sizeof(int) + size;
		}
		else if(recordDescriptor.at(i).type == TypeInt)
			offset += sizeof(int);
		else
			offset += sizeof(float);
	}
	return offset;
}


/* Insert the record directory into a page for a given record
 * The format of record directory is
 * ------------------------------------------------------------------------------
 * |              |  pointer to the  |  pointer to the |       |  pointer to the|
 * |# of attribute|   head of 1st    |  head of 2nd    |  ...  |  end of this   |
 * |              |   attribute         attribute      |       |  record        |
 * ------------------------------------------------------------------------------
 * param
 * offset- the place where we would like to insert the record
 * page - the page we want to insert
 * record - the data
 * recordDescriptor
 *
*/
void insertFields (const int &offset, void *page, const void *record, const vector<Attribute> &recordDescriptor) {
	unsigned attrNum = recordDescriptor.size();
	//field offset in front of data part
	unsigned fields[attrNum + 2];
	unsigned dataSize = 0;
	unsigned sizeVarChar = 0;

	for (unsigned i = 0; i < attrNum; i++)
	{
		if (recordDescriptor.at(i).type == TypeVarChar)
		{
			memcpy(&sizeVarChar, (char *) record + dataSize, sizeof(int));
			dataSize +=  sizeof(int) + sizeVarChar;
			fields[i + 2] = sizeof(int) + sizeVarChar;
		}
		else if(recordDescriptor.at(i).type == TypeInt) {
			dataSize += sizeof(int);
			fields[i + 2] = sizeof(int);
		}
		else {
			dataSize += sizeof(float);
			fields[i + 2] = sizeof(float);
		}
	}

	fields[0] = attrNum;
	fields[1] = offset + (attrNum + 2) * sizeof(int);
	memcpy((char *) page + offset, &fields[0], sizeof(int));
	memcpy((char *) page + offset + sizeof(int), &fields[1], sizeof(int));
	for (unsigned i = 2; i < attrNum + 2; i++) {
		fields[i] += fields[i - 1];
		memcpy((char *) page + offset + i * sizeof(int), &fields[i], sizeof(int));
	}

}

//Get slot directory of a page
void getSlotDirectory (const void *pageContent, SlotDirectory &slotDirectory)
{
	//freeSpace
	memcpy(&slotDirectory.freeSpace, (char *) pageContent + FREE_SPACE, sizeof(int));
	//slotNum
	memcpy(&slotDirectory.slotNum, (char *) pageContent + SLOT_NUM, sizeof(int));
	//slots
	slotDirectory.slots = new SlotEntry[slotDirectory.slotNum];
	for (unsigned i = 0; i < slotDirectory.slotNum; i++) {
		//offset of slot
		memcpy(&slotDirectory.slots[i].offset, (char *) pageContent + (OFFSET_ENTRY - 8 * i), sizeof(int));
		//length of slot
		memcpy(&slotDirectory.slots[i].length, (char *) pageContent + (LENGTH_ENTRY - 8 * i), sizeof(int));
	}
}

//Get the slot index in slotDirectory of a given offset
int findIndex(unsigned x, SlotDirectory &slotDirectory)
{
	unsigned i;

	for (i=0; i < slotDirectory.slotNum; i++)
	{
        if(x==slotDirectory.slots[i].offset)
        	return i;
	}
	return -1;
}

//Get the size of a given slotDirectory
unsigned sizeOfSlotDirectory (SlotDirectory &slotDirectory)
{
	return PAGE_SIZE - (SLOT_NUM - 8 * slotDirectory.slotNum);
}


RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
	pfm = PagedFileManager::instance();
}

RecordBasedFileManager::~RecordBasedFileManager()
{
	delete _rbf_manager;
	_rbf_manager = 0;
}



/*in: spaceNeeded,
 * out: pageInsert,
 *
 */

int RecordBasedFileManager::findPgToInsert (int spaceNeeded, FileHandle &fileHandle) {
	RC rc;

	void *dirPage = malloc(PAGE_SIZE);
	int pageInsert = -1;

	for (unsigned i = 0; i < fileHandle.totalDirectoryPageNum; i++) {

		rc = fileHandle.readDirectoryPage(i, dirPage);
		if (rc == -1)
			return rc;
		int indexedPg;
		memcpy(&indexedPg, (char *) dirPage + PAGE_SIZE - sizeof(int), sizeof(int));
		int nFreeSpace;

		for (int j = 0; j < indexedPg; j++) {
			memcpy(&nFreeSpace, (char *) dirPage + j * sizeof(int), sizeof(int));
			if (spaceNeeded <= nFreeSpace) {
				pageInsert = i * PAGE_DIRECTORY_MAX_SIZE + j;
				break;
			}
		}

		if (pageInsert != -1)
			break;
	}

	free(dirPage);
	return pageInsert;
}
RC RecordBasedFileManager::createFile(const string &fileName)
{
	RC rc = pfm->createFile(fileName.c_str());
    return rc;
}

RC RecordBasedFileManager::destroyFile(const string &fileName)
{
    RC rc = pfm->destroyFile(fileName.c_str());
	return rc;
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
	RC rc = pfm->openFile(fileName.c_str(), fileHandle);
	return rc;
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle)
{
    RC rc = pfm->closeFile(fileHandle);
	return rc;
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid)
{
	RC rc = -1;

	void *tempPage = malloc(PAGE_SIZE);
	unsigned attrNum = recordDescriptor.size();
	//field offset in front of data part
	unsigned dataSize = getDataSize(data, recordDescriptor);
	//Number of fields is added in front of fields[]
	unsigned recordSize = (attrNum + 2) * sizeof(int) + dataSize;

	//Find a page to insert record
	int pageInsert = -1;
	unsigned spaceNeeded = recordSize + 2 * sizeof(int);
	pageInsert = findPgToInsert(spaceNeeded, fileHandle);

	//Append a new data page
	if (pageInsert == -1)
	{
		// Don't forget to initiate tempPage before append
		memset(tempPage, 0, PAGE_SIZE);
		if (fileHandle.appendPage(tempPage) != 0)
			return rc;
		pageInsert = fileHandle.totalDataPageNum - 1;
	}
	rid.pageNum = pageInsert;

	if (fileHandle.readPage(pageInsert, tempPage) != 0)
		return rc;
	SlotDirectory slotDirectory;
	getSlotDirectory(tempPage, slotDirectory);

	// Check if this page needs to be reorganized first
	unsigned spaceAtEnd = PAGE_SIZE - slotDirectory.freeSpace - sizeOfSlotDirectory(slotDirectory);
	if (spaceAtEnd < spaceNeeded)
	{
		if (reorganizePage(fileHandle, recordDescriptor, pageInsert) != 0)
			return rc;
		if (fileHandle.readPage(pageInsert, tempPage) != 0)
			return rc;
		getSlotDirectory(tempPage, slotDirectory);
	}

	// Assign a slot for the record that we want to insert
	// Find a slot with deleted record first
	unsigned i;
	for (i = 0; i < slotDirectory.slotNum; i++)
	{
		if (slotDirectory.slots[i].offset >= PAGE_SIZE && slotDirectory.slots[i].offset < 2 * PAGE_SIZE)
		{
			rid.slotNum = i;
			break;
		}
	}
	// When we cannot find a slot with deleted record, we try to assign a new slot
	if (i == slotDirectory.slotNum)
	{
		slotDirectory.slotNum++;
		rid.slotNum = slotDirectory.slotNum - 1;
	}

	// Insert record directory first
	insertFields(slotDirectory.freeSpace, tempPage, data, recordDescriptor);

	// Insert data after we have inserted the record directory
	memcpy((char *) tempPage + slotDirectory.freeSpace + (attrNum + 2) * sizeof(int), data, dataSize);
	// Change this slot's offset, make it point to the beginning of the record directory
	memcpy((char *) tempPage + OFFSET_ENTRY - 2 * sizeof(int) * rid.slotNum, &slotDirectory.freeSpace, sizeof(int));
	// Change this slot's length to pure data length (do not include record directory)
	memcpy((char *) tempPage + LENGTH_ENTRY - 2 * sizeof(int) * rid.slotNum, &dataSize, sizeof(int));
	// Change the pointer to freeSpace in this page
	slotDirectory.freeSpace += recordSize;
	memcpy((char *) tempPage + FREE_SPACE, &slotDirectory.freeSpace, sizeof(int));
	// Change the number of slots in this page
	memcpy((char *) tempPage + SLOT_NUM, &slotDirectory.slotNum, sizeof(int));

	//Write back to disk
	if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
		return rc;
	//Save the changes of number of free space in the corresponding page directory entry
	int change = 0 - (recordSize + 2 * sizeof(int));
	if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
		return rc;

	delete []slotDirectory.slots;
	free(tempPage);
	rc = 0;
	return rc;
}

RC RecordBasedFileManager::insertFwdedRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid, const RID oldRID)
{
	void *tempPage = malloc(PAGE_SIZE);

	RC rc = -1;
	unsigned attrNum = recordDescriptor.size();
	// field offset in front of data part
	unsigned dataSize = getDataSize(data, recordDescriptor);
	// oldRID and record directory
	unsigned recordSize = (attrNum + 4) * sizeof(int) + dataSize;

	// Find a page to insert record
	int pageInsert = -1;
	unsigned spaceNeeded = recordSize + 2 * sizeof(int);
	pageInsert = findPgToInsert(spaceNeeded, fileHandle);

	// Append a new data page
	if (pageInsert == -1)
	{
		// Don't forget to initiate tempPage before append
		memset(tempPage, 0, PAGE_SIZE);
		if (fileHandle.appendPage(tempPage) != 0)
			return rc;
		pageInsert = fileHandle.totalDataPageNum - 1;
	}

	rid.pageNum = pageInsert;

	if (fileHandle.readPage(pageInsert, tempPage) != 0)
		return rc;
	SlotDirectory slotDirectory;
	getSlotDirectory(tempPage, slotDirectory);

	// Check if this page needs to be reorganized first
	unsigned spaceAtEnd = PAGE_SIZE - slotDirectory.freeSpace - sizeOfSlotDirectory(slotDirectory);
	if (spaceAtEnd < spaceNeeded)
	{
		if (reorganizePage(fileHandle, recordDescriptor, pageInsert) != 0)
			return rc;
		if (fileHandle.readPage(pageInsert, tempPage) != 0)
			return rc;
		getSlotDirectory(tempPage, slotDirectory);
	}

	// Assign a slot for the record that we want to insert
	// Find a slot with deleted record first
	unsigned i;
	for (i = 0; i < slotDirectory.slotNum; i++)
	{
		if (slotDirectory.slots[i].offset >= PAGE_SIZE && slotDirectory.slots[i].offset < 2 * PAGE_SIZE)
		{
			rid.slotNum = i;
			break;
		}
	}
	// When we cannot find a slot with deleted record, we try to assign a new slot
	if (i == slotDirectory.slotNum)
	{
		slotDirectory.slotNum++;
		rid.slotNum = slotDirectory.slotNum - 1;
	}

	// Insert oldRID into the free space first
	memcpy((char *) tempPage + slotDirectory.freeSpace, &oldRID.pageNum, sizeof(int));
	memcpy((char *) tempPage + slotDirectory.freeSpace + sizeof(int), &oldRID.slotNum, sizeof(int));

	// Insert record directory
	insertFields(slotDirectory.freeSpace + 2 * sizeof(int), tempPage, data, recordDescriptor);

	// Insert data
	memcpy((char *) tempPage + slotDirectory.freeSpace + (attrNum + 4) * sizeof(int), data, dataSize);
	// Change this slot's offset, make it point to the beginning of the record (before oldRID)
	unsigned  fwdOffset = slotDirectory.freeSpace + 3 * PAGE_SIZE;
	memcpy((char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, &fwdOffset, sizeof(int));
	// Change this slot's length to pure data length (do not include record directory and oldRID)
	memcpy((char *) tempPage + LENGTH_ENTRY - 8 * rid.slotNum, &dataSize, sizeof(int));
	// Change the pointer to freeSpace in this page
	slotDirectory.freeSpace += recordSize;
	memcpy((char *) tempPage + FREE_SPACE, &slotDirectory.freeSpace, sizeof(int));
	// Change the number of slots in this page
	memcpy((char *) tempPage + SLOT_NUM, &slotDirectory.slotNum, sizeof(int));

	// Write back to disk
	fileHandle.writePage(rid.pageNum, tempPage);

	int change = 0 - (recordSize + 2 * sizeof(int));
	if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
		return rc;

	delete []slotDirectory.slots;
	free(tempPage);
	rc = 0;
	return rc;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data)
{
	RC rc = -1;
	void *page = malloc(PAGE_SIZE);
	if (fileHandle.readPage(rid.pageNum, page) != 0)
	{
		free(page);
		return rc;
	}

	SlotDirectory slotDirectory;
	getSlotDirectory(page, slotDirectory);
	if ((unsigned)rid.slotNum >= slotDirectory.slotNum)
		return rc;
	int offset, length;
	memcpy(&offset, (char *)page + OFFSET_ENTRY - rid.slotNum * 2 * sizeof(int), sizeof(int));
	memcpy(&length, (char *)page + LENGTH_ENTRY - rid.slotNum * 2 * sizeof(int), sizeof(int));

	// Read normal record
	if (offset < PAGE_SIZE)
		memcpy((char *)data, (char *)page + offset + (recordDescriptor.size() + 2) * sizeof(int), length);
	// Read a deleted record
	else if (offset >= PAGE_SIZE && offset < 2 * PAGE_SIZE)
		return rc;
	// Read a RID that refers to a tombstone
	else if(offset >= 2 * PAGE_SIZE && offset < 3 * PAGE_SIZE)
	{
		RID fwdRID;
		memcpy(&fwdRID.pageNum, (char *)page + (offset - 2 * PAGE_SIZE), sizeof(int));
		memcpy(&fwdRID.slotNum, (char *)page + (offset - 2 * PAGE_SIZE) + sizeof(int), sizeof(int));
		readRecord(fileHandle, recordDescriptor, fwdRID, data);
	}

	else if (offset >= 3 * PAGE_SIZE && offset < 4 * PAGE_SIZE)
	{
		//oldRID (trace back), record directory, data
		memcpy((char *)data, (char *)page + (offset - 3 * PAGE_SIZE) + (recordDescriptor.size() + 4) * sizeof(int), length);
	}

	free(page);
	rc = 0;
	return rc;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
	RC rc = 0;
	int offset = 0;
	for (unsigned i = 0; i < recordDescriptor.size(); i++)
	{
		if (recordDescriptor.at(i).type == TypeVarChar)
		{
			int size;
			memcpy(&size, (char *)data + offset, sizeof(int));
			offset += sizeof(int);
			char temp[size];
			memcpy(&temp, (char *)data + offset, size);
			offset += size;
			temp[size] = '\0';
			cout << recordDescriptor.at(i).name + ":" + temp + "\t";
		}
		else if (recordDescriptor.at(i).type == TypeInt)
		{
			int temp1;
			memcpy(&temp1, (char *)data + offset, sizeof(int));
			offset += sizeof(int);
			cout << recordDescriptor.at(i).name + ":" << temp1 << "\t";
		}
		else
		{
			float temp2;
			memcpy(&temp2, (char *)data + offset, sizeof(float));
			offset += sizeof(float);
			cout << recordDescriptor.at(i).name + ":" << temp2 << "\t";
		}
	}
	cout << endl;
    return rc;
}

RC RecordBasedFileManager::deleteRecords(FileHandle &fileHandle) {
	RC rc = -1;

	void *tempPage = malloc(PAGE_SIZE);
	void *dirTempPg = malloc(PAGE_SIZE);

	unsigned freeSpace = 0;
	unsigned slotNum = 0;
	memcpy((char *) tempPage + FREE_SPACE, &freeSpace, sizeof(int));
	memcpy((char *) tempPage + SLOT_NUM, &slotNum, sizeof(int));


	for (unsigned i = 0; i < fileHandle.totalDataPageNum; i++)
	{
		if (fileHandle.writePage(i, tempPage) != 0)
			return rc;
	}

	unsigned dirFreeSpace = PAGE_SIZE - 2 * sizeof(int);

	for (unsigned i = 0; i < fileHandle.totalDirectoryPageNum; i ++)
	{

		if (fileHandle.readDirectoryPage(i, dirTempPg) != 0)
			return rc;

		unsigned dataPgNum;
		memcpy(&dataPgNum, (char *)dirTempPg + PAGE_SIZE - sizeof(int), sizeof(int));

		for (unsigned j = 0; j < dataPgNum; j++)
			memcpy((char *)dirTempPg + j * sizeof(int), &dirFreeSpace, sizeof(int));

		if (fileHandle.writeDirectoryPage(i, dirTempPg) != 0)
			return rc;
	}

	fileHandle.totalDataPageNum = 0;
	fileHandle.totalDirectoryPageNum = 1;

	free(tempPage);
	free(dirTempPg);
	rc = 0;
	return rc;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid) {
	RC rc = -1;

	void *tempPage = malloc(PAGE_SIZE);
	if (fileHandle.readPage(rid.pageNum, tempPage) != 0)
		return rc;
	SlotDirectory slotDirectory;
	getSlotDirectory(tempPage, slotDirectory);

	//If the rid slotNum greater than the total number of slots in that page, then return an error
	if ((unsigned)rid.slotNum >= slotDirectory.slotNum)
		return rc;

	int offset = slotDirectory.slots[rid.slotNum].offset;
	int length = slotDirectory.slots[rid.slotNum].length;

	//Normal record to be deleted
	if (offset < PAGE_SIZE)
	{
		//Just need to change the offset of the slot to its original offset + PAGE_SIZE
		unsigned newOffset = offset + PAGE_SIZE;
		memcpy((char *) tempPage + OFFSET_ENTRY - 2 * sizeof(int) * rid.slotNum, &newOffset, sizeof(int));
		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return rc;

		//Update the number of free space in the page
		int change = length + sizeof(int) * (recordDescriptor.size() + 2);
		if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
			return rc;
	}

	//If it is an already deleted record, then return an error
	else if (offset >= PAGE_SIZE && offset < 2 * PAGE_SIZE)
	{
		free(tempPage);
		delete[] slotDirectory.slots;
		return -1;
	}

	//If the rid refers to a tombstone, then delete both of the tombstone itself and the record it pointing to
	else if (offset >= 2 * PAGE_SIZE && offset < 3 * PAGE_SIZE)
	{
		RID fwdRID;
		memcpy(&fwdRID.pageNum, (char *) tempPage + (offset - 2 * PAGE_SIZE), sizeof(int));
		memcpy(&fwdRID.slotNum, (char *) tempPage + (offset - 2 * PAGE_SIZE) + sizeof(int), sizeof(int));

		//Update the offset of tombstone slot to the delete range
		unsigned newOffset = offset - PAGE_SIZE;
		memcpy((char *) tempPage + OFFSET_ENTRY - 2 * sizeof(int) * rid.slotNum, &newOffset, sizeof(int));
		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return -1;
		//Update the number of free space in the page that was used to contain the tombstone
		int change = 2 * sizeof(int);
		if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
			return -1;

		//Delete new spot record
		//deleteRecord(fileHandle, recordDescriptor, fwdRID);
		if (fileHandle.readPage(fwdRID.pageNum, tempPage) != 0)
			return rc;

		memcpy(&offset, (char*)tempPage + OFFSET_ENTRY - fwdRID.slotNum * 2 * sizeof(int), sizeof(int));
		memcpy(&length, (char*)tempPage + LENGTH_ENTRY - fwdRID.slotNum * 2 * sizeof(int), sizeof(int));
		//Update the offset of new record slot to the delete range
		int newOffset1 = offset - 2 * PAGE_SIZE;
		memcpy((char *) tempPage + OFFSET_ENTRY - 2 * sizeof(int) * fwdRID.slotNum, &newOffset1, sizeof(int));
		if (fileHandle.writePage(fwdRID.pageNum, tempPage) != 0)
			return rc;
		//Update the number of free space in this page
		change = length + sizeof(int) * (recordDescriptor.size() + 4);
		if (fileHandle.updateNumOfFreeSpace(fwdRID.pageNum, change) != 0)
			return rc;

	}
	else
	{
		free(tempPage);
		delete[] slotDirectory.slots;
		return -1;
	}

	//If this is a new spot record
	/*else
	{
		//Get the rid for the tombstone of this new spot record
		RID oldRID;
		memcpy(&oldRID.pageNum, (char *) tempPage + (offset - 3 * PAGE_SIZE), sizeof(int));
		memcpy(&oldRID.slotNum, (char *) tempPage + (offset - 3 * PAGE_SIZE) + sizeof(int), sizeof(int));

		//Update the offset of new record slot to the delete range
		int newOffset = offset - 2 * PAGE_SIZE;
		memcpy((char *) tempPage + OFFSET_ENTRY - 2 * sizeof(int) * rid.slotNum, &newOffset, sizeof(int));
		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return rc;
		//Update the number of free space in this page
		int change = length + sizeof(int) * (recordDescriptor.size() + 4);
		if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
			return rc;

		//Delete tombstone record
		deleteRecord(fileHandle, recordDescriptor, oldRID);
	}*/

	free(tempPage);
	delete[] slotDirectory.slots;
	rc = 0;
	return rc;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid)
{
	RC rc = -1;
	int change = 0;

	// Fetch the data page that contains the record which we are going to update
	void *tempPage = malloc(PAGE_SIZE);
	if (fileHandle.readPage(rid.pageNum, tempPage) != 0)
		return rc;
	SlotDirectory slotDirectory;
	getSlotDirectory(tempPage, slotDirectory);

	bool isNewSpot;

	// Old record parameter
	int offset;
	memcpy(&offset, (char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, sizeof(int));

	// When updating deleted records, error
	if (offset < PAGE_SIZE)
	{
		isNewSpot = false;
	}
	else if (offset >= PAGE_SIZE && offset < 2 * PAGE_SIZE)
	{
		return -1;
	}
	// When updating a tombstone
	// Get the fwdRID, and update the new spot record
	else if (offset >= 2 * PAGE_SIZE && offset < 3 * PAGE_SIZE)
	{
		RID fwdRID;
		memcpy(&fwdRID.pageNum, (char *) tempPage + offset, sizeof(int));
		memcpy(&fwdRID.slotNum, (char *) tempPage + offset + sizeof(int), sizeof(int));

		return updateRecord(fileHandle, recordDescriptor, data, fwdRID);
	}
	//When updating new spot
	else
	{
		isNewSpot = true;
	}

	int end;
	unsigned fieldsNum = recordDescriptor.size() + 2;
	// Get the end of the record that we need to update
	unsigned oldRecordSize;
	if (isNewSpot == false)
	{
		memcpy(&end, (char *) tempPage + offset + (fieldsNum - 1) * sizeof(int), sizeof(int));
		oldRecordSize = end - offset;
	}
	else
	{
		memcpy(&end, (char *) tempPage + offset + (fieldsNum + 1) * sizeof(int), sizeof(int));
		oldRecordSize = end - (offset - 3 * PAGE_SIZE);
	}


	// New record parameter
	unsigned newDataSize = getDataSize(data, recordDescriptor);
	unsigned newRecordSize =  newDataSize + sizeof(int) * fieldsNum;

	unsigned freeSpace = PAGE_SIZE - slotDirectory.freeSpace - sizeOfSlotDirectory(slotDirectory);

	// Free space in page rid.pageNum
	void *dirPage = malloc(PAGE_SIZE);
	if (fileHandle.readDirectoryPage((rid.pageNum / PAGE_DIRECTORY_MAX_SIZE), dirPage) != 0)
		return rc;
	unsigned nFreeSpace;
	memcpy(&nFreeSpace, (char *) dirPage + sizeof(int) * (rid.pageNum % PAGE_DIRECTORY_MAX_SIZE), sizeof(int));

	// Data shrinks, insert at original position
	if (oldRecordSize >= newRecordSize)
	{
		change = oldRecordSize - newRecordSize;
		if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
			return rc;

		memcpy((char *) tempPage + LENGTH_ENTRY - 8 * rid.slotNum, &newDataSize, sizeof(int));

		insertFields(offset, tempPage, data, recordDescriptor);
		memcpy((char *) tempPage + offset + sizeof(int) * fieldsNum, data, newDataSize);
		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return rc;
	}
	// Insert at the end
	else if ((newRecordSize + 2 * sizeof(int)) <= freeSpace)
	{
		change = oldRecordSize - newRecordSize;
		if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
			return rc;

		insertFields(slotDirectory.freeSpace, tempPage, data, recordDescriptor);
		memcpy((char *) tempPage + slotDirectory.freeSpace + sizeof(int) * fieldsNum, data, newDataSize);

		unsigned newFreeSpace = slotDirectory.freeSpace + newRecordSize;
		memcpy((char *) tempPage + FREE_SPACE, &newFreeSpace, sizeof(int));
		memcpy((char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, &slotDirectory.freeSpace, sizeof(int));
		memcpy((char *) tempPage + LENGTH_ENTRY - 8 * rid.slotNum, &newDataSize, sizeof(int));

		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return rc;
	}
	// Reorganize page, insert at the end
	else if (newRecordSize <= (oldRecordSize + nFreeSpace)) {

		//add PAGE_SIZE to original offset, for reorganizePage
		int offset;
		memcpy(&offset, (char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, sizeof(int));
		offset += PAGE_SIZE;
		memcpy((char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, &offset, sizeof(int));

		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return rc;
		// Leave update of the number of free space to the end of insert

		if (reorganizePage(fileHandle, recordDescriptor, rid.pageNum) != 0)
			return rc;

		if (fileHandle.readPage(rid.pageNum, tempPage) != 0)
			return rc;

		int pFreeSpace;
		memcpy(&pFreeSpace, (char *) tempPage + FREE_SPACE, sizeof(int));

		memcpy((char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, &pFreeSpace, sizeof(int));
		memcpy((char *) tempPage + LENGTH_ENTRY - 8 * rid.slotNum, &newDataSize, sizeof(int));

		insertFields(pFreeSpace, tempPage, data, recordDescriptor);
		memcpy((char *) tempPage + pFreeSpace + sizeof(int) * fieldsNum, data, newDataSize);

		pFreeSpace += newRecordSize;
		memcpy((char *) tempPage + FREE_SPACE, &pFreeSpace, sizeof(int));

		change = 0 - newRecordSize;
		if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
			return rc;

		if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
			return rc;

	}
	//Leave a tombstone, start insertion of the record
	//Update a tombstone
	else
	{
		if (!isNewSpot)
		{

			RID newRid;
			if (insertFwdedRecord(fileHandle, recordDescriptor, data, newRid, rid) != 0)
				return rc;

			// Leave a tombstone
			if (fileHandle.readPage(rid.pageNum, tempPage) != 0)
				return rc;
			// Length
			int oldLength;
			memcpy(&oldLength, (char *) tempPage + LENGTH_ENTRY - 8 * rid.slotNum, sizeof(int));
			int length = 2 * sizeof(int);
			memcpy((char *) tempPage + LENGTH_ENTRY - 8 * rid.slotNum, &length, sizeof(int));
			// Offset
			int oldOffset;
			memcpy(&oldOffset, (char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, sizeof(int));

			// Tombstone
			memcpy((char *) tempPage + oldOffset, &newRid.pageNum, sizeof(int));
			memcpy((char *) tempPage + oldOffset + sizeof(int), &newRid.slotNum, sizeof(int));

			oldOffset += 2 * PAGE_SIZE;
			memcpy((char *) tempPage + OFFSET_ENTRY - 8 * rid.slotNum, &oldOffset, sizeof(int));

			if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
				return rc;
			// Changes in old page
			change = oldLength - length;
			if (fileHandle.updateNumOfFreeSpace(rid.pageNum, change) != 0)
				return rc;
		}
		//update a new spot
		else {
			RID tombRID, newRID;
			memcpy(&tombRID.pageNum, (char *) tempPage + (offset - 3 * PAGE_SIZE), sizeof(int));
			memcpy(&tombRID.slotNum, (char *) tempPage + (offset - 3 * PAGE_SIZE) + sizeof(int), sizeof(int));

			if (insertFwdedRecord(fileHandle, recordDescriptor, data, newRID, tombRID) != 0)
				return rc;

			if (deleteRecord(fileHandle, recordDescriptor, rid) != 0)
				return rc;

			// Change original tombstone value to newRID
			if (fileHandle.readPage(tombRID.pageNum, tempPage) != 0)
				return rc;

			// Offset
			int temp;
			memcpy(&temp, (char *) tempPage + OFFSET_ENTRY - 8 * tombRID.slotNum, sizeof(int));
			temp -= 2 * PAGE_SIZE;
			// Update the tombstone
			memcpy((char *) tempPage + temp, &newRID.pageNum, sizeof(int));
			memcpy((char *) tempPage + temp + sizeof(int), &newRID.slotNum, sizeof(int));

			if (fileHandle.writePage(rid.pageNum, tempPage) != 0)
				return rc;
			//no change in nFreeSpace
		}
	}

	free(dirPage);
	free(tempPage);
	rc = 0;
	return rc;
}


RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, const string attributeName, void *data)
{
	RC rc = -1;

	unsigned attrNum = recordDescriptor.size();
	unsigned offset;
	unsigned length;

	void *record = malloc(PAGE_SIZE);
	void *tempPage = malloc(PAGE_SIZE);

	if (fileHandle.readPage(rid.pageNum, tempPage) != 0)
		return rc;

	memcpy(&offset, (char *)tempPage + OFFSET_ENTRY - rid.slotNum * 2 * sizeof(int), sizeof(int));
	memcpy(&length, (char *)tempPage + LENGTH_ENTRY - rid.slotNum * 2 * sizeof(int), sizeof(int));

	if (offset < PAGE_SIZE)
	{
		// Normal record
		memcpy((char*)record, (char*)tempPage + offset, length + (attrNum + 2) * sizeof(int));
	}
	else if (offset >= PAGE_SIZE && offset < 2 * PAGE_SIZE)
	{
		// Deleted record
		free(record);
		free(tempPage);
		return rc;
	}
	else if (offset >= 2 * PAGE_SIZE && offset < 3 * PAGE_SIZE)
	{
		// Tombstone record
		RID fwdRID;
		memcpy(&fwdRID.pageNum, (char *) tempPage + (offset - 2 * PAGE_SIZE), sizeof(int));
		memcpy(&fwdRID.slotNum, (char *) tempPage + (offset - 2 * PAGE_SIZE) + sizeof(int), sizeof(int));

		if (fileHandle.readPage(fwdRID.pageNum, tempPage) != 0)
			return rc;
		// Get the offset and length of that forward record
		memcpy(&offset, (char *)tempPage + OFFSET_ENTRY - fwdRID.slotNum * 2 * sizeof(int), sizeof(int));
		memcpy(&length, (char *)tempPage + LENGTH_ENTRY - fwdRID.slotNum * 2 * sizeof(int), sizeof(int));
		offset -= 3 * PAGE_SIZE;

		memcpy((char*)record, (char*)tempPage + offset + sizeof(int) * 2, length + (attrNum + 2) * sizeof(int));
	}
	else
	{
		// New spot record
		offset -= 3 * PAGE_SIZE;
		memcpy((char*)record, (char*)tempPage + offset  + sizeof(int) * 2, length + (attrNum + 2) * sizeof(int));
	}

	unsigned attrStart;
	unsigned attrEnd;
	unsigned i;

	// Try to search if there is an attribute that has the given name
	for (i =0; i < attrNum; i++)
	{
		if (recordDescriptor.at(i).name == attributeName)
		{
			memcpy(&attrStart, (char *)record + (i + 1) * sizeof(int), sizeof(int));
			memcpy(&attrEnd, (char *)record + (i + 2) * sizeof(int), sizeof(int));
			break;
		}
	}

	// If not find such an attribute
	if (i >= attrNum)
	{
		free(record);
		free(tempPage);
		return rc;
	}

	if (recordDescriptor.at(i).type == TypeVarChar)
		memcpy((char *)data, (char *)record + attrStart - offset + sizeof(int), (attrEnd - attrStart - sizeof(int)));
	else
		memcpy((char *)data, (char *)record + attrStart - offset, (attrEnd - attrStart));

	rc = 0;
	free(record);
	free(tempPage);
	return rc;
}

RC RecordBasedFileManager::reorganizePage(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const unsigned pageNumber)
{
	RC rc = -1;

	void *tempPage = malloc(PAGE_SIZE);
	if (fileHandle.readPage(pageNumber, tempPage) != 0)
		return rc;
	SlotDirectory slotDirectory;
	getSlotDirectory(tempPage, slotDirectory);

	void *oldPg = malloc(PAGE_SIZE);
	if (fileHandle.readPage(pageNumber, oldPg) != 0)
		return rc;
	SlotDirectory oldSlotDirectory;
	getSlotDirectory(oldPg, oldSlotDirectory);

	unsigned *offsets = new unsigned[slotDirectory.slotNum];
	int *flags = new int[slotDirectory.slotNum];
	for (unsigned i = 0; i < slotDirectory.slotNum; i++)
	{
		offsets[i] = slotDirectory.slots[i].offset;
		if (offsets[i] < PAGE_SIZE)
		{
			flags[i] = 0;
		}
		else if (offsets[i] >= PAGE_SIZE && offsets[i] < 2 * PAGE_SIZE)
		{
			offsets[i] -= PAGE_SIZE;
			flags[i] = 1;
		}
		else if (offsets[i] >= 2 * PAGE_SIZE && offsets[i] < 3 * PAGE_SIZE)
		{
			offsets[i] -= 2 * PAGE_SIZE;
			flags[i] = 2;
		}
		else
		{
			offsets[i] -= 3 * PAGE_SIZE;
			flags[i] = 3;
		}
	}

	// Sort offsets, keep offsets[] and flags[] in order
	for (unsigned i = 1; i < slotDirectory.slotNum; i++)
	{
		unsigned temp = offsets[i];
		int tempFlag = flags[i];
		int j;
		for (j = i - 1; j >= 0 && offsets[j] > temp; j--)
		{
			offsets[j + 1] = offsets[j];
			flags[j + 1] = flags[j];
		}
		offsets[j + 1] = temp;
		flags[j + 1] = tempFlag;
	}

	int pFreeSpace = 0;
	for (unsigned i = 0; i < slotDirectory.slotNum; i++)
	{
		int index;
		if (flags[i] == 0)
			index = findIndex(offsets[i], slotDirectory);
		else if (flags[i] == 1)
			continue;
		else if (flags[i] == 2)
			index = findIndex(offsets[i] + 2 * PAGE_SIZE, slotDirectory);
		else
			index = findIndex(offsets[i] + 3 * PAGE_SIZE, slotDirectory);

		int size = recordDescriptor.size() + 2;

		// Change slot's offset to new position
		if(flags[i] == 0)
		{
			memcpy((char *) tempPage + OFFSET_ENTRY - 8 * index, &pFreeSpace, sizeof(int));
		}
		else if (flags[i] == 2)
		{
			unsigned temp = pFreeSpace + 2 * PAGE_SIZE;
			memcpy((char *) tempPage + OFFSET_ENTRY - 8 * index, &temp, sizeof(int));
		}
		else
		{
			unsigned temp = pFreeSpace + 3 * PAGE_SIZE;
			memcpy((char *) tempPage + OFFSET_ENTRY - 8 * index, &temp, sizeof(int));
		}

		// We should know if the record contains a RID of its tombstone
		if (flags[i] == 0)
		{
			// Fill in the record directory
			insertFields(pFreeSpace, (char*)tempPage, (char*)oldPg + offsets[i] + sizeof(int) * size, recordDescriptor);
			pFreeSpace += sizeof(int) * size;
			// Fill in data
			memcpy((char *) tempPage + pFreeSpace, (char *) oldPg + offsets[i] + sizeof(int) * size, oldSlotDirectory.slots[index].length);
			pFreeSpace += slotDirectory.slots[index].length;
		}
		else if (flags[i] == 2)
		{
			// Fill in tombstone message
			memcpy((char *) tempPage + pFreeSpace, (char *) oldPg + offsets[i], oldSlotDirectory.slots[index].length);
			pFreeSpace += slotDirectory.slots[index].length;
		}
		else
		{
			// Fill in the record directory
			insertFields(pFreeSpace, (char*)tempPage, (char*)oldPg + offsets[i] + sizeof(int) * (size + 2), recordDescriptor);
			pFreeSpace += sizeof(int) * (size + 2);
			// Fill in data
			memcpy((char *) tempPage + pFreeSpace, (char *) oldPg + offsets[i] + sizeof(int) * (size + 2), oldSlotDirectory.slots[index].length);
			pFreeSpace += slotDirectory.slots[index].length;
		}
	}

	// Fill in the pointer to free space and number of slots in the new page
	memcpy((char *) tempPage + FREE_SPACE, &pFreeSpace, sizeof(int));
	memcpy((char *) tempPage + SLOT_NUM, (char *) oldPg + SLOT_NUM, sizeof(int));
	if (fileHandle.writePage(pageNumber, tempPage) != 0)
		return rc;

	int nFreeSpace = PAGE_SIZE - pFreeSpace - sizeOfSlotDirectory(slotDirectory);
	if (fileHandle.setNumOfFreeSpace(pageNumber, nFreeSpace) != 0)
		return rc;

	free(tempPage);
	free(oldPg);
	delete(slotDirectory.slots);
	delete(offsets);
	delete(flags);
	rc = 0;
	return rc;
}

void RBFM_ScanIterator::addAttribute(const Attribute &attr)
{
	recordDescriptor.push_back(attr);
}

void RBFM_ScanIterator::addRid(const RID &rid)
{
	rids.push_back(rid);
}

void RBFM_ScanIterator::addData(void *projection, unsigned length)
{
	void * temp = malloc( length );
	memcpy((char *)temp, (const char *)projection, length);
	dataProjection.push_back(temp);
	lengths.push_back(length);
}


void RBFM_ScanIterator::resetState()
{
	iterator=0;

	for(vector< void * > ::iterator iter = dataProjection.begin(); iter != dataProjection.end(); ++iter )
	{
		free( ( void * )( *iter ) );
	}

	recordDescriptor.clear();
	rids.clear();
	dataProjection.clear();
}

RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data)
{
	// When there is no more qualified data, just return a RBFM_EOF

	unsigned end = lengths.size();
	if (iterator >= end)
		return RBFM_EOF;
	// Return RID
	rid.pageNum = rids.at(iterator).pageNum;
	rid.slotNum = rids.at(iterator).slotNum;
	// Return required projects on qualified data
	memcpy((char *)data, (char *)dataProjection.at(iterator), lengths.at(iterator));
	// Increase iterator
	iterator++;
	return 0;
}

  // scan returns an iterator to allow the caller to go through the results one by one.
RC RecordBasedFileManager::scan(FileHandle &fileHandle,
								const vector<Attribute> &recordDescriptor,
								const string &conditionAttribute,
								const CompOp compOp,                  // comparision type such as "<" and "="
								const void *value,                    // used in the comparison
								const vector<string> &attributeNames, // a list of projected attributes
								RBFM_ScanIterator &rbfm_ScanIterator)
{
	RC rc = -1;

	// Initialize the Iterator that we are going to return
	rbfm_ScanIterator.resetState();

	// Fill in the recordDescriptor in rbfm_ScanIterator
	// Save the field number of the attribute that we need to project in a vector, for easy projection
	unsigned i = 0, j = 0;
	vector<int> projectedAttributeOffset; 	// Save the field number
	while (i < recordDescriptor.size() && j < attributeNames.size())
	{
		if (recordDescriptor.at(i).name == attributeNames.at(j))
		{
			rbfm_ScanIterator.addAttribute(recordDescriptor.at(i));
			projectedAttributeOffset.push_back(i);
			i++;
			j++;
		}
		else
			i++;
	}

	unsigned isFwdRecord = 0;	// Indicate if a record is a forward record
	unsigned isQualified = 0;	// Indicate if a record satisfies the given condition

	void *tempPage = malloc(PAGE_SIZE);	// The page we get when scan sequentially
	void *tempFwdPage = malloc(PAGE_SIZE);	// The page we get when there is a forward
	void *projection = malloc(PAGE_SIZE);	// Store the projection of a qualified record

	SlotDirectory slotDirectory;
	unsigned offset;	// Contains the offset in a slot that we are processing
	unsigned length;	// Contains the length in a slot that we are processing

	// Iterate all data pages sequentially
	for(unsigned i =0; i < fileHandle.totalDataPageNum; i++)
	{
		if (fileHandle.readPage(i, tempPage) != 0)
			return rc;
		getSlotDirectory(tempPage, slotDirectory);

		// Iterate all slots in this data page sequentially
		for(unsigned j = 0; j < slotDirectory.slotNum; j++)
		{
			offset = slotDirectory.slots[j].offset;
			length = slotDirectory.slots[j].length;
			// Get the record whose rid is (i,j)
			if (offset < PAGE_SIZE)
			{
				// Normal record, just read its data and record directory in to void* record
				isFwdRecord = 0;
			}
			else if (offset >= PAGE_SIZE && offset < 2 * PAGE_SIZE)
			{
				// Deleted record, ignore it
				continue;
			}
			else if (offset >= 2 * PAGE_SIZE && offset < 3 * PAGE_SIZE)
			{
				// Record with a tombstone
				// Get forward record's RID first
				RID fwdRID;
				memcpy(&fwdRID.pageNum, (char *) tempPage + (offset - 2 * PAGE_SIZE), sizeof(int));
				memcpy(&fwdRID.slotNum, (char *) tempPage + (offset - 2 * PAGE_SIZE) + sizeof(int), sizeof(int));

				if (fileHandle.readPage(fwdRID.pageNum, tempFwdPage) != 0)
					return rc;
				// Get the offset and length of that forward record
				memcpy(&offset, (char *)tempFwdPage + OFFSET_ENTRY - fwdRID.slotNum * 2 * sizeof(int), sizeof(int));
				memcpy(&length, (char *)tempFwdPage + LENGTH_ENTRY - fwdRID.slotNum * 2 * sizeof(int), sizeof(int));
				offset -= 3 * PAGE_SIZE;

				isFwdRecord = 1;
			}
			else
			{
				// Record in a new spot, ignore it
				continue;
			}

			isQualified = 0;

			// Get conditionAttribute value and check if this record is qualified
			for (unsigned k = 0; k < recordDescriptor.size(); k++)
			{
				unsigned attrStart;
				if (conditionAttribute.length() == 0)
				{
					isQualified = 1;
					break;
				}
				if (recordDescriptor.at(k).name == conditionAttribute)
				{
					if (recordDescriptor.at(k).type == TypeInt)
					{
						int recordValue, conditionValue;
						if (!isFwdRecord)
						{
							memcpy(&attrStart, (char *)tempPage + offset + (k + 1) * sizeof(int), sizeof(int));
							memcpy(&recordValue, (char *)tempPage + attrStart, sizeof(int));
						}
						else
						{
							memcpy(&attrStart, (char *)tempFwdPage + offset + (k + 3) * sizeof(int), sizeof(int));
							memcpy(&recordValue, (char *)tempFwdPage + attrStart, sizeof(int));
						}
						memcpy(&conditionValue, (char *)value, sizeof(int));
						switch(compOp)
						{
							case EQ_OP:
								if(recordValue == conditionValue)	isQualified = 1;	break;
							case LT_OP:
								if(recordValue < conditionValue)	isQualified = 1;	break;
							case GT_OP:
								if(recordValue > conditionValue)	isQualified = 1;	break;
							case LE_OP:
								if(recordValue <= conditionValue)	isQualified = 1;	break;
							case GE_OP:
								if(recordValue >= conditionValue)	isQualified = 1;	break;
							case NE_OP:
								if(recordValue != conditionValue)	isQualified = 1;	break;
							case NO_OP:
								isQualified = 1;
						}
						break;
					}
					else if (recordDescriptor.at(k).type == TypeReal)
					{
						float recordValue, conditionValue;
						if (!isFwdRecord)
						{
							memcpy(&attrStart, (char *)tempPage + offset + (k + 1) * sizeof(int), sizeof(int));
							memcpy(&recordValue, (char *)tempPage + attrStart, sizeof(int));
						}
						else
						{
							memcpy(&attrStart, (char *)tempFwdPage + offset + (k + 3) * sizeof(int), sizeof(int));
							memcpy(&recordValue, (char *)tempFwdPage + attrStart, sizeof(int));
						}
						memcpy(&conditionValue, (char *)value, sizeof(float));
						switch(compOp)
						{
							case EQ_OP:
								if(recordValue == conditionValue)	isQualified = 1;	break;
							case LT_OP:
								if(recordValue < conditionValue)	isQualified = 1;	break;
							case GT_OP:
								if(recordValue > conditionValue)	isQualified = 1;	break;
							case LE_OP:
								if(recordValue <= conditionValue)	isQualified = 1;	break;
							case GE_OP:
								if(recordValue >= conditionValue)	isQualified = 1;	break;
							case NE_OP:
								if(recordValue != conditionValue)	isQualified = 1;	break;
							case NO_OP:
								isQualified = 1;
						}
						break;
					}
					else
					{
						if (recordDescriptor.at(k).type == TypeVarChar)
						{
							unsigned charLength;
							int compareRes;
							char *recordValue = (char *) malloc(PAGE_SIZE);
							if (!isFwdRecord)
							{
								memcpy(&attrStart, (char *)tempPage + offset + (k + 1) * sizeof(int), sizeof(int));
								memcpy(&charLength, (char *)tempPage + attrStart, sizeof(int));
								memcpy((char *)recordValue, (char *)tempPage + attrStart + sizeof(int), charLength);
							}
							else
							{
								memcpy(&attrStart, (char *)tempFwdPage + offset + (k + 3) * sizeof(int), sizeof(int));
								memcpy(&charLength, (char *)tempFwdPage + attrStart, sizeof(int));
								memcpy((char *)recordValue, (char *)tempFwdPage + attrStart + sizeof(int), charLength);
							}
							recordValue[charLength] = '\0';
							compareRes = strcmp(recordValue, (char *)value);
							switch(compOp)
							{
								case EQ_OP:
									if(compareRes == 0)	isQualified = 1;	break;
								case LT_OP:
									if(compareRes < 0)	isQualified = 1;	break;
								case GT_OP:
									if(compareRes > 0)	isQualified = 1;	break;
								case LE_OP:
									if(compareRes <= 0)	isQualified = 1;	break;
								case GE_OP:
									if(compareRes >= 0)	isQualified = 1;	break;
								case NE_OP:
									if(compareRes != 0)	isQualified = 1;	break;
								case NO_OP:
									isQualified = 1;
							}
							break;
						}
					}
				}
			}

			// If this record is qualified, then save the projection
			if (!isQualified)
				continue;
			else
			{
				RID projectRID((int)i,(int)j);
				//projectRID.pageNum = i;
				//projectRID.slotNum = j;
				// Add RID in the scan
				rbfm_ScanIterator.addRid(projectRID);

				unsigned attrStart;
				unsigned attrEnd;
				unsigned projectionLength = 0;

				// Project each attribute of this qualified record
				for (unsigned k = 0; k < projectedAttributeOffset.size(); k++)
				{
					if(!isFwdRecord)
					{
						memcpy(&attrStart, (char *)tempPage + offset + (projectedAttributeOffset.at(k) + 1) * sizeof(int), sizeof(int));
						memcpy(&attrEnd, (char *)tempPage + offset + (projectedAttributeOffset.at(k) + 2) * sizeof(int), sizeof(int));
						memcpy((char *)projection + projectionLength, (char *)tempPage + attrStart, attrEnd- attrStart);
						projectionLength += (attrEnd- attrStart);
					}
					else
					{
						memcpy(&attrStart, (char *)tempFwdPage + offset + (projectedAttributeOffset.at(k) + 3) * sizeof(int), sizeof(int));
						memcpy(&attrEnd, (char *)tempFwdPage + offset + (projectedAttributeOffset.at(k) + 4) * sizeof(int), sizeof(int));
						memcpy((char *)projection + projectionLength, (char *)tempFwdPage + attrStart, attrEnd- attrStart);
						projectionLength += (attrEnd- attrStart);
					}
				}
				rbfm_ScanIterator.addData(projection, projectionLength);
			}
		}
	}

	free(tempPage);
	free(tempFwdPage);
	free(projection);
	rc = 0;
	return rc;
}

RC RecordBasedFileManager::reorganizeFile(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor)
{
	RC rc = -1;
	return rc;
}


