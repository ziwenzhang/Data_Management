
#include "rm.h"
#include <sys/stat.h>



void insertTableData(const string &tableName, const int &tableType, const int &numCol, RecordBasedFileManager *rbfm, FileHandle &fileHandle) {

	vector<Attribute> tableDescriptor;
    Attribute attr;
    attr.name = "tableName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)30;
    tableDescriptor.push_back(attr);

    attr.name = "tableType";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    tableDescriptor.push_back(attr);

    attr.name = "numCol";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    tableDescriptor.push_back(attr);


	int nameLength = tableName.length();
	int length = (nameLength + sizeof(int)) + 2 * sizeof(int);

	void *data = malloc(length);
	int offset = 0;
	memcpy((char *)data + offset, &nameLength, sizeof(int));
	offset += sizeof(int);
	memcpy((char *)data + offset, tableName.c_str(), nameLength);
	offset += nameLength;
	memcpy((char *)data + offset, &tableType, sizeof(int));
	offset += sizeof(int);
	memcpy((char *)data + offset, &numCol, sizeof(int));
	offset += sizeof(int);

	RID rid;
	rbfm->insertRecord(fileHandle, tableDescriptor, data, rid);

	free(data);

}


void insertColData(const string &tableName, const string &colName, const int &colType, const int &colPosition, const int &colSize, RecordBasedFileManager *rbfm, FileHandle &fileHandle) {

	vector<Attribute> colDescriptor;
    Attribute attr;
    attr.name = "tableName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)30;
    colDescriptor.push_back(attr);

    attr.name = "colName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)30;
    colDescriptor.push_back(attr);

    attr.name = "colType";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    colDescriptor.push_back(attr);

    attr.name = "colPosition";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    colDescriptor.push_back(attr);

    attr.name = "colSize";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    colDescriptor.push_back(attr);



	int tableLength = tableName.length();
	int colLength = colName.length();
	int length = (tableLength + colLength + 2 * sizeof(int)) + 3 * sizeof(int);

	void *data = malloc(length);
	int offset = 0;

	memcpy((char *)data + offset, &tableLength, sizeof(int));
	offset += sizeof(int);
	memcpy((char *)data + offset, tableName.c_str(), tableLength);
	offset += tableLength;

	memcpy((char *)data + offset, &colLength, sizeof(int));
	offset += sizeof(int);
	memcpy((char *)data + offset, colName.c_str(), colLength);
	offset += colLength;

	memcpy((char *)data + offset, &colType, sizeof(int));
	offset += sizeof(int);

	memcpy((char *)data + offset, &colPosition, sizeof(int));
	offset += sizeof(int);

	memcpy((char *)data + offset, &colSize, sizeof(int));
	offset += sizeof(int);

	RID rid;
	rbfm->insertRecord(fileHandle, colDescriptor, data, rid);

	free(data);

}

vector<Attribute> getAttrsOfRelToIndex() {
	vector<Attribute> descriptor;
	Attribute attr;
	attr.name = "relName";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)30;
	descriptor.push_back(attr);

	attr.name = "indexName";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)30;
	descriptor.push_back(attr);

	attr.name = "attrName";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)30;
	descriptor.push_back(attr);

	return descriptor;
}


void insertRelToIndex(const string &relName, const string &indexName, const string &attrName, RecordBasedFileManager *rbfm, FileHandle &fileHandle) {
	vector<Attribute> descriptor = getAttrsOfRelToIndex();


	int relLeng = relName.size();
	int indexLeng = indexName.size();
	int attrLeng = attrName.size();
	int length = relLeng + indexLeng + attrLeng + 3 * sizeof(int);

	char *data = new char[length];
	int offset = 0;

	memcpy(data + offset, &relLeng, sizeof(int));
	offset += sizeof(int);
	memcpy(data + offset, relName.c_str(), relLeng);
	offset += relLeng;

	memcpy(data + offset, &indexLeng, sizeof(int));
	offset += sizeof(int);
	memcpy(data + offset, indexName.c_str(), indexLeng);
	offset += indexLeng;

	memcpy(data + offset, &attrLeng, sizeof(int));
	offset += sizeof(int);
	memcpy(data + offset, attrName.c_str(), attrLeng);
	offset += attrLeng;


	RID rid;
	rbfm->insertRecord(fileHandle, descriptor, data, rid);

	delete data;
}

RelationManager* RelationManager::_rm = 0;

RelationManager* RelationManager::instance()
{
    if(!_rm)
        _rm = new RelationManager();

    return _rm;
}

RelationManager::RelationManager()
{
	rbfm = RecordBasedFileManager::instance();
	ixm = IndexManager::instance();

	FileHandle fileHandle;
	if (!fileExists("table")) {

		rbfm->createFile("table");
		rbfm->openFile("table", fileHandle);

		insertTableData("table", System, 3, rbfm, fileHandle);
		insertTableData("column", System, 5, rbfm, fileHandle);

		rbfm->closeFile(fileHandle);
	}

	if (!fileExists("column")) {

		rbfm->createFile("column");
		rbfm->openFile("column", fileHandle);

		insertColData("table", "tableName", TypeVarChar, 0, 50, rbfm, fileHandle);
		insertColData("table", "tableType", TypeInt, 1, 4, rbfm, fileHandle);
		insertColData("table", "numCol", TypeInt, 2, 4, rbfm, fileHandle);

		insertColData("column", "tableName", TypeVarChar, 0, 50, rbfm, fileHandle);
		insertColData("column", "colName", TypeVarChar, 1, 50, rbfm, fileHandle);
		insertColData("column", "colType", TypeInt, 2, 4, rbfm, fileHandle);
		insertColData("column", "colPosition", TypeInt, 3, 4, rbfm, fileHandle);
		insertColData("column", "colSize", TypeInt, 4, 4, rbfm, fileHandle);

		rbfm->closeFile(fileHandle);
	}

	if (!fileExists("relToIndex")) {

		rbfm->createFile("relToIndex");

	}

}

RelationManager::~RelationManager()
{
	free(rbfm);
	free(ixm);
}

bool RelationManager::isSystemTbl(const string &tableName)
{
	FileHandle fileHandle;
	rbfm->openFile("table", fileHandle);

	vector<string> attrNames;
	attrNames.push_back("tableName");

	vector<Attribute> tableDescriptor;
	Attribute attr;
	attr.name = "tableName";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)30;
	tableDescriptor.push_back(attr);

	attr.name = "tableType";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	tableDescriptor.push_back(attr);

	attr.name = "numCol";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	tableDescriptor.push_back(attr);

	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	TableType type = System;
	rbfm->scan(fileHandle, tableDescriptor, "tableType", EQ_OP, &type, attrNames, rbfm_ScanIterator);

	RID rid;
	void *data = malloc(PAGE_SIZE);
	char *name = (char *) malloc(PAGE_SIZE);
	while (rbfm_ScanIterator.getNextRecord(rid, data) != RBFM_EOF)
	{
		unsigned length;

		memcpy(&length, (char *)data, sizeof(int));

		memcpy((char *)name, (char *)data + sizeof(int), length);
		name[length] = '\0';
		if (strcmp(name, tableName.c_str()) == 0)
		{
			break;
		}
	}

	rbfm->closeFile(fileHandle);

	if (strcmp(name, tableName.c_str()) == 0)
	{
		free(data);
		free(name);
		return true;
	}
	else
	{
		free(data);
		free(name);
		return false;
	}
}


RC RelationManager::createTable(const string &tableName, const vector<Attribute> &attrs)
{
	RC rc;
	rc = rbfm->createFile(tableName);
	if (rc == -1)
		return rc;
	FileHandle fileHandle;
	rc = rbfm->openFile("table", fileHandle);
	if (rc == -1)
		return rc;
	//const string &tableName, const int &tableType, const int &numCol, RecordBasedFileManager *rbfm, FileHandle &fileHandle) {
	insertTableData(tableName, User, attrs.size(), rbfm, fileHandle);
	rc = rbfm->closeFile(fileHandle);
	if (rc == -1)
		return rc;


	rc = rbfm->openFile("column", fileHandle);
	if (rc == -1)
		return rc;
	for (unsigned i = 0; i < attrs.size(); i++) {

		insertColData(tableName, attrs.at(i).name, attrs.at(i).type, i, attrs.at(i).length, rbfm, fileHandle);
	}

	rc = rbfm->closeFile(fileHandle);
	if (rc == -1)
		return rc;
	rc = 0;
	return rc;
}

RC RelationManager::deleteTable(const string &tableName)
{
	RC rc = -1;

	if (isSystemTbl(tableName) == true) {
		return -1;
	}

	rc = rbfm->destroyFile(tableName);
	if (rc == -1)
		return rc;

	rc = 0;
	return rc;
}

RC RelationManager::getAttributes(const string &tableName, vector<Attribute> &attrs)
{
	RC rc = -1;

	FileHandle fileHandle;
	if (rbfm->openFile("column", fileHandle) != 0)
		return rc;

	vector<string> attrNames;
	attrNames.push_back("colName");
	attrNames.push_back("colType");
	attrNames.push_back("colSize");

	vector<Attribute> colDescriptor;
	Attribute attr;
	attr.name = "tableName";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)30;
	colDescriptor.push_back(attr);

	attr.name = "colName";
	attr.type = TypeVarChar;
	attr.length = (AttrLength)30;
	colDescriptor.push_back(attr);

	attr.name = "colType";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	colDescriptor.push_back(attr);

	attr.name = "colPosition";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	colDescriptor.push_back(attr);

	attr.name = "colSize";
	attr.type = TypeInt;
	attr.length = (AttrLength)4;
	colDescriptor.push_back(attr);

	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	rbfm->scan(fileHandle, colDescriptor, "tableName", EQ_OP, tableName.c_str(), attrNames, rbfm_ScanIterator);

	RID rid;
	Attribute tempAttr;
	void *data = malloc(PAGE_SIZE);
	memset(data, 0, PAGE_SIZE);
	char *name= (char *)malloc(PAGE_SIZE);
	while (rbfm_ScanIterator.getNextRecord(rid, data) != RBFM_EOF)
	{

		unsigned offset = 0;
		unsigned length;

		memcpy(&length, (char *) data + offset, sizeof(int));
		offset += sizeof(int);
		memcpy((char *)name, (char *) data + offset, length);
		name[length] = '\0';
		tempAttr.name = name;
		offset += length;

		memcpy(&tempAttr.type, (char *) data + offset, sizeof(int));
		offset += sizeof(int);

		memcpy(&tempAttr.length, (char *) data + offset, sizeof(int));
		offset += sizeof(int);

		attrs.push_back(tempAttr);
	}

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	free(data);
	free(name);
	rc = 0;
	return rc;
}

RC RelationManager::insertTuple(const string &tableName, const void *data, RID &rid)
{
	RC rc = -1;

	// If it is a system table, then return an error
	if (isSystemTbl(tableName) == true)
		return rc;

	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	// Get recordDescriptor for this table
	vector<Attribute> recordDescriptor;
	if (getAttributes(tableName, recordDescriptor) != 0)
		return rc;

	RID tempRID;
	// Insert tuple
	if (rbfm->insertRecord(fileHandle, recordDescriptor, data, tempRID) != 0)
		return rc;
	rid = tempRID;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	// get index files that need to be updated
	FileHandle fh1;
	rbfm->openFile("relToIndex", fh1);
	//string conditionAttr = "relName";
	vector<string> attrProj;
	attrProj.push_back("indexName");
	attrProj.push_back("attrName");

	vector<Attribute> indexDescriptor = getAttrsOfRelToIndex();
	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	rbfm->scan(fh1, indexDescriptor, "relName", EQ_OP, tableName.c_str(), attrProj, rbfm_ScanIterator);
	rbfm->closeFile(fh1);

	RID dumbRID;
	char *dataProj = new char[PAGE_SIZE];
	string indexName;
	string attrName;


	while (rbfm_ScanIterator.getNextRecord(dumbRID, dataProj) != RBFM_EOF) {
		int offset = 0;
		int length = 0;

		char *temp = new char[40];
		memcpy(&length, dataProj + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(temp, dataProj + offset, length);
		offset += length;
		temp[length] = '\0';
		indexName = temp;

		memcpy(&length, dataProj + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(temp, dataProj + offset, length);
		temp[length] = '\0';
		attrName = temp;

		Attribute attr;
		for (vector<Attribute>::iterator itr = recordDescriptor.begin(); itr != recordDescriptor.end(); ++itr) {
			if (itr->name == attrName) {
				attr = *itr;
				break;
			}
		}

		FileHandle indexFH;
		rc = ixm->openFile(indexName, indexFH);
		if (rc != 0)
			return rc;

		char *key = new char[PAGE_SIZE];
		rc = readAttribute(tableName, tempRID, attrName, key);
		if (rc != 0)
			return rc;

		int leng = 0;
		leng += sizeof(int);
		if (attr.type == TypeVarChar) {
			int tmp;
			memcpy(&tmp, key, sizeof(int));
			leng += tmp;
		}
		void *keyTemp = malloc(leng);
		memcpy(keyTemp, key, leng);


		rc = ixm->insertEntry(indexFH, attr, keyTemp, tempRID);
		rc = ixm->closeFile(indexFH);

		delete temp;
		delete key;
		free((char *)keyTemp);
	}


	delete dataProj;
	rc = 0;
	return rc;
}

RC RelationManager::deleteTuples(const string &tableName)
{
	RC rc = -1;

	// If it is a system table, then return an error
	if (isSystemTbl(tableName) == true)
		return rc;

	// get index files that need to be updated
	FileHandle fh1;
	rbfm->openFile("relToIndex", fh1);
	vector<string> attrProj;
	attrProj.push_back("indexName");

	vector<Attribute> indexDescriptor = getAttrsOfRelToIndex();
	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	rbfm->scan(fh1, indexDescriptor, "relName", EQ_OP, tableName.c_str(), attrProj, rbfm_ScanIterator);
	rbfm->closeFile(fh1);

	RID dumbRID;
	char *dataProj = new char[PAGE_SIZE];
	string indexName;
	string attrName;


	while (rbfm_ScanIterator.getNextRecord(dumbRID, dataProj) != RBFM_EOF) {
		int offset = 0;
		int length = 0;

		char *temp = new char[40];
		memcpy(&length, dataProj + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(temp, dataProj + offset, length);
		offset += length;
		temp[length] = '\0';
		indexName = temp;

		ixm->destroyFile(indexName);
		ixm->createFile(indexName);

		delete temp;
	}


	delete dataProj;















	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	if (rbfm->deleteRecords(fileHandle) != 0)
		return rc;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	rc = 0;
	return rc;
}

RC RelationManager::deleteTuple(const string &tableName, const RID &rid)
{
	RC rc = -1;

	// If it is a system table, then return an error
	if (isSystemTbl(tableName) == true)
		return -1;

	// Get recordDescriptor for this table
	vector<Attribute> recordDescriptor;
	if (getAttributes(tableName, recordDescriptor) != 0)
		return rc;


	// get index files that need to be updated
	FileHandle fh1;
	rbfm->openFile("relToIndex", fh1);
	vector<string> attrProj;
	attrProj.push_back("indexName");
	attrProj.push_back("attrName");

	vector<Attribute> indexDescriptor = getAttrsOfRelToIndex();
	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	rbfm->scan(fh1, indexDescriptor, "relName", EQ_OP, tableName.c_str(), attrProj, rbfm_ScanIterator);
	rbfm->closeFile(fh1);



	RID dumbRID;
	char *dataProj = new char[PAGE_SIZE];
	string indexName;
	string attrName;


	while (rbfm_ScanIterator.getNextRecord(dumbRID, dataProj) != RBFM_EOF) {
		int offset = 0;
		int length = 0;

		char *temp = new char[40];
		memcpy(&length, dataProj + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(temp, dataProj + offset, length);
		offset += length;
		temp[length] = '\0';
		indexName = temp;

		memcpy(&length, dataProj + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(temp, dataProj + offset, length);
		temp[length] = '\0';
		attrName = temp;


		Attribute attr;
		for (vector<Attribute>::iterator itr = recordDescriptor.begin(); itr != recordDescriptor.end(); ++itr) {
			if (itr->name == attrName) {
				attr = *itr;
				break;
			}
		}

		FileHandle indexFH;
		rc = ixm->openFile(indexName, indexFH);
		if (rc != 0)
			return rc;

		char *key = new char[PAGE_SIZE];
		rc = readAttribute(tableName, rid, attrName, key);
		if (rc != 0)
			return rc;

		int leng = 0;
		leng += sizeof(int);
		if (attr.type == TypeVarChar) {
			int tmp;
			memcpy(&tmp, key, sizeof(int));
			leng += tmp;
		}
		void *keyTemp = malloc(leng);
		memcpy(keyTemp, key, leng);


		rc = ixm->deleteEntry(indexFH, attr, keyTemp, rid);
		rc = ixm->closeFile(indexFH);

		delete temp;
		delete key;
		free((char *)keyTemp);
	}


	delete dataProj;



	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	// Delete tuple
	if (rbfm->deleteRecord(fileHandle, recordDescriptor, rid) != 0)
		return rc;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	rc = 0;
	return rc;
}


// todo: has not mixed with index
RC RelationManager::updateTuple(const string &tableName, const void *data, const RID &rid)
{
	RC rc = -1;

	// If it is a system table, then return an error
	if (isSystemTbl(tableName) == true)
		return rc;

	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	// Get recordDescriptor for this table
	vector<Attribute> recordDescriptor;
	if (getAttributes(tableName, recordDescriptor) != 0)
		return rc;

	// Update tuple
	if (rbfm->updateRecord(fileHandle, recordDescriptor, data, rid) != 0)
		return rc;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	rc = 0;
	return rc;
}

RC RelationManager::readTuple(const string &tableName, const RID &rid, void *data)
{
	RC rc = -1;

	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	// Get recordDescriptor for this table
	vector<Attribute> recordDescriptor;
	if (getAttributes(tableName, recordDescriptor) != 0)
		return rc;

	// Read tuple
	if (rbfm->readRecord(fileHandle, recordDescriptor, rid, data) != 0)
		return rc;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	rc = 0;
	return rc;
}

RC RelationManager::readAttribute(const string &tableName, const RID &rid, const string &attributeName, void *data)
{
	RC rc = -1;

	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) == -1)
		return rc;
		//return -1;

	// Get recordDescriptor for this table
	vector<Attribute> attrs;
	if (getAttributes(tableName, attrs) != 0)
		return rc;
		//return -2;

	// Read tuple
	if (rbfm->readAttribute(fileHandle, attrs, rid, attributeName, data) != 0)
		return rc;
		//return -3;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;
		//return -4;

	rc = 0;
	return rc;
}

RC RelationManager::reorganizePage(const string &tableName, const unsigned pageNumber)
{
	RC rc = -1;

	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	// Get recordDescriptor for this table
	vector<Attribute> recordDescriptor;
	if (getAttributes(tableName, recordDescriptor) != 0)
		return rc;

	// Reorganize page
	if (rbfm->reorganizePage(fileHandle, recordDescriptor, pageNumber) != 0)
		return rc;

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;

	rc = 0;
	return rc;
}

RC RM_ScanIterator::getNextTuple(RID &rid, void *data)
{
	return rbfm_ScanIterator.getNextRecord(rid, data);
};

RC RelationManager::scan(const string &tableName,
     const string &conditionAttribute,
     const CompOp compOp,                  // comparision type such as "<" and "="
     const void *value,                    // used in the comparison
     const vector<string> &attributeNames, // a list of projected attributes
     RM_ScanIterator &rm_ScanIterator)
{
	RC rc = -1;

	FileHandle fileHandle;
	if (rbfm->openFile(tableName, fileHandle) != 0)
		return rc;

	// Get recordDescriptor for this table
	vector<Attribute> recordDescriptor;
	if (getAttributes(tableName, recordDescriptor) != 0)
		return rc;

	//RBFM_ScanIterator rbfm_ScanIterator;
	//rbfm_ScanIterator.resetState();
	rm_ScanIterator.rbfm_ScanIterator.resetState();
	// Scan
	if (rbfm->scan(fileHandle, recordDescriptor, conditionAttribute, compOp, value, attributeNames, rm_ScanIterator.rbfm_ScanIterator) != 0)
		return rc;

	//rm_ScanIterator.initialize(rbfm_ScanIterator);

	if (rbfm->closeFile(fileHandle) != 0)
		return rc;
	rc = 0;
	return rc;
}

// Extra credit
RC RelationManager::dropAttribute(const string &tableName, const string &attributeName)
{
	RC rc = -1;
	return rc;
}

// Extra credit
RC RelationManager::addAttribute(const string &tableName, const Attribute &attr)
{
	RC rc = -1;
	return rc;
}

// Extra credit
RC RelationManager::reorganizeTable(const string &tableName)
{
	RC rc = -1;
	return rc;
}

/*
RM_IndexScanIterator& RM_IndexScanIterator::operator =(const RM_IndexScanIterator &rm_indexScanIterator) {
	if (this != &rm_indexScanIterator) {
		delete this;
		this->ix_ScanIterator = rm_indexScanIterator.ix_ScanIterator;
		return *this;
	}
}
*/

RC RM_IndexScanIterator::getNextEntry(RID &rid, void *key)
{
	if(ix_ScanIterator->getNextEntry(rid, key)!=0)
		return -1;
	else
		return 0;
}

RC RelationManager::createIndex(const string &tableName, const string &attributeName)
{
	RC rc = 0;
	string indexName = tableName + "." + attributeName;


	rc = ixm->createFile(indexName);
	if (rc != 0)
		return rc;

	// update catalog
	FileHandle fh;
	rc = rbfm->openFile("relToIndex", fh);
	if (rc != 0)
		return rc;
	insertRelToIndex(tableName, indexName, attributeName, rbfm, fh);

	rc = rbfm->closeFile(fh);
	if (rc != 0)
		return rc;

	FileHandle fh1;
	rbfm->openFile("table", fh1);
	insertTableData(indexName, User, 3, rbfm, fh1);
	rbfm->closeFile(fh1);

	FileHandle fh2;
	rbfm->openFile("column", fh2);
	insertColData(indexName, "relName", TypeVarChar, 0, 50, rbfm, fh2);
	insertColData(indexName, "indexName", TypeVarChar, 1, 50, rbfm, fh2);
	insertColData(indexName, "attrName", TypeVarChar, 2, 50, rbfm, fh2);
	rbfm->closeFile(fh2);



	FileHandle fileHandle;
	// scan table of tableName
	rc = rbfm->openFile(tableName, fileHandle);
	if (rc != 0)
		return rc;

	vector<Attribute> descriptor;
	rc = getAttributes(tableName, descriptor);
	if (rc != 0)
		return rc;

	Attribute attribute;
	for (vector<Attribute>::iterator iter = descriptor.begin(); iter != descriptor.end(); ++iter) {
		//if (iter->name == attributeName) {
		if (iter->name.compare(attributeName) == 0) {
			attribute = *iter;
			break;
		}
	}

	vector<string> attrName;
	attrName.push_back(attributeName);

	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	rc = rbfm->scan(fileHandle, descriptor, "", NO_OP, NULL, attrName, rbfm_ScanIterator);
	if (rc != 0)
		return rc;


	rc = rbfm->closeFile(fileHandle);
	if (rc != 0)
		return rc;

	FileHandle fhIndex;
	rc = ixm->openFile(indexName, fhIndex);
	if (rc != 0)
		return rc;

	RID rid;
	char *data = new char[PAGE_SIZE];
	while (rbfm_ScanIterator.getNextRecord(rid, data) != RBFM_EOF) {
		rc = ixm->insertEntry(fhIndex, attribute, data, rid);
		if (rc != 0)
			return rc;
	}

	rc = ixm->closeFile(fhIndex);
	if (rc != 0)
		return rc;

	delete data;
	return rc;
}

RC RelationManager::destroyIndex(const string &tableName, const string &attributeName) {
	RC rc;
	string indexName = tableName + "." + attributeName;
	rc = ixm->destroyFile(indexName);
	if (rc != 0)
		return rc;

	// update catalog
	FileHandle fileHandle;
	rc = rbfm->openFile("relToIndex", fileHandle);
	if (rc != 0)
		return rc;

	vector<Attribute> descriptor = getAttrsOfRelToIndex();

	vector<string> attrNames;
	attrNames.push_back("relName");
	attrNames.push_back("indexName");
	attrNames.push_back("attrName");

	RBFM_ScanIterator rbfm_ScanIterator;
	rbfm_ScanIterator.resetState();
	string conditionAttr = "indexName";
	rc = rbfm->scan(fileHandle, descriptor, conditionAttr, EQ_OP, indexName.c_str(), attrNames, rbfm_ScanIterator);
	if (rc != 0)
		return rc;

	RID rid;
	char *data = new char[PAGE_SIZE];
	while (rbfm_ScanIterator.getNextRecord(rid, data) != RBFM_EOF) {
		rc = rbfm->deleteRecord(fileHandle, descriptor, rid);
		if (rc != 0)
			return rc;
	}

	delete data;
	return rc;
}


RC RelationManager::indexScan(const string &tableName, const string &attrName, const void *lowKey, const void *highKey, bool lowKeyInclusive, bool highKeyInclusive, RM_IndexScanIterator &rm_IndexScanIterator) {
	RC rc;

	string indexName = tableName + "." + attrName;

	FileHandle fileHandle;
	ixm->openFile(indexName, fileHandle);

	vector<Attribute> attrs;
	rc = getAttributes(tableName, attrs);
	if (rc != 0)
		return rc;
	Attribute attr;
	for (vector<Attribute>::iterator iter = attrs.begin(); iter != attrs.end(); ++iter) {
		if (iter->name == attrName) {
			attr = *iter;
			break;
		}
	}


	IX_ScanIterator *ix_ScanIterator = new IX_ScanIterator();
	ixm->scan(fileHandle, attr, lowKey, highKey, lowKeyInclusive, highKeyInclusive, *ix_ScanIterator);
	//RM_IndexScanIterator *temp = new RM_IndexScanIterator(ix_ScanIterator);
	//rm_IndexScanIterator = *temp;
	rm_IndexScanIterator.setIterator(*ix_ScanIterator);
	//delete temp;
	return rc;
}
