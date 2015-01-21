#include "indexHandle.h"
#include <assert.h>

indexHandle::indexHandle() {
	fileHandle = NULL;
	bHdrChanged = false;
	root = NULL;
	path = NULL;
	pathPos = NULL;
	//treeLargest = NULL;
	hdr.treeHeight = 0;
}

indexHandle::~indexHandle() {
	if (root != NULL) {
		delete root;
		root = NULL;
	}

	if (pathPos != NULL) {
		delete []pathPos;
		pathPos = NULL;
	}

	if (path != NULL) {
		delete []path;
		path = NULL;
	}

	if (fileHandle != NULL) {
		//delete fileHandle;
		fileHandle = NULL;
	}

	/*
	if (treeLargest != NULL) {
		// diff
		delete (char *)treeLargest;
		treeLargest = NULL;
	}
	*/
}

RC indexHandle::openIndex(FileHandle &fHandle) {
	//fileHandle = new FileHandle();
	fileHandle	= &fHandle;
	bHdrChanged = true;
	pathPos = NULL;

	// init hdr
	char *tempPage = new char[PAGE_SIZE];

	RC rc = fileHandle->readPage(0, tempPage);
	if (rc == -1)
		return rc;
	memcpy(&hdr, tempPage, sizeof(hdr));

	// init root
	if (hdr.treeHeight > 0) { // root page exists
		//root = new BtreeNode(hdr.attrType, *fileHandle, hdr.rootPgNum);

		setHeight(hdr.treeHeight);
		path[0] = root;
	}
	else { // create root page
		//PageNum pageNum;
		memset(tempPage, 0, PAGE_SIZE);
		rc = fileHandle->appendPage(tempPage);

		hdr.numPages = fileHandle->getNumberOfPages();
		hdr.rootPgNum = hdr.numPages - 1;

		root = new BtreeNode(hdr.attrType, *fileHandle, hdr.rootPgNum);

		setHeight(1);
		path[0] = root;
	}

	delete tempPage;
	tempPage = NULL;

	return rc;

}

RC indexHandle::closeIndex() {
	RC rc;
	if (bHdrChanged == true) { // write hdr page to disk
		char *tempPage = new char[PAGE_SIZE];

		rc = fileHandle->readPage(0, tempPage);
		if (rc == -1) {
			return rc;
		}

		memcpy(tempPage, &hdr, sizeof(hdr));
		rc = fileHandle->writePage(0, tempPage);
		if (rc == -1) {
			return rc;
		}

	}

	for (int i = 0; i < hdr.treeHeight; i++) {
		if (path[i] != NULL) {
			delete path[i];
			path[i] = NULL;
		}
	}

	if (pathPos != NULL){
		delete pathPos;
		pathPos = NULL;
	}

	free(root);
	root = NULL;

	// do not set fileHandle to NULL
	return rc;
}

RC indexHandle::getFileHeader(FileHandle &fileHandle) {
	RC rc = 0;
	void *temp = malloc(PAGE_SIZE);
	rc = fileHandle.readPage(root->getPageNum(), temp);
	if (rc == -1)
		return rc;

	memcpy(&hdr, temp, sizeof(hdr));
	free(temp);
	temp = NULL;
	return rc;
}

BtreeNode *indexHandle::fetchNode(PageNum pageNum) {
	BtreeNode *btreeNode = new BtreeNode(hdr.attrType, *fileHandle, pageNum);
	return btreeNode;
}

BtreeNode *indexHandle::fetchNode(RID rid) {
	BtreeNode *btreeNode = new BtreeNode(hdr.attrType, *fileHandle, rid.pageNum);
	return btreeNode;
}

BtreeNode *indexHandle::findLfNode(const void *key) {
	// zero level
	if (root == NULL)
		return NULL;

	// one level
	if (hdr.treeHeight == 1) {
		return root;
	}

	for (int i = 0; i < hdr.treeHeight - 1; i++) {
		RID rid = path[i]->findRIDAtPosition(key);
		int pos = path[i]->findKeyPosition(key);

		pathPos[i] = pos;
		path[i + 1] = fetchNode(rid);
	}
	return path[hdr.treeHeight - 1];
}

BtreeNode* indexHandle::findLeftmostNode() {

	if (root == NULL)
		return NULL;

	if (hdr.treeHeight == 1) {
		return root;
	}

	for (int i = 0; i < hdr.treeHeight - 1; i++) {
		RID rid = path[i]->getRID(0);

		if (path[i + 1] != NULL) {
			delete (path[i + 1]);
			path[i + 1] = NULL;
		}
		path[i + 1] = fetchNode(rid);
	}

	return path[hdr.treeHeight - 1];
}

// in: fileHandle
// out: btreeNode, pageNum
RC indexHandle::allocateNode(FileHandle &fileHandle, BtreeNode *&btreeNode, int &pageNum) {
	char *tempPage = new char[PAGE_SIZE];
	memset(tempPage, 0 , PAGE_SIZE);
	RC rc = 0;
	rc = fileHandle.appendPage(tempPage);
	if (rc == -1)
		return rc;

	pageNum = fileHandle.getNumberOfPages() - 1;
	btreeNode = new BtreeNode(hdr.attrType, fileHandle, pageNum);

	hdr.numPages++;
	assert(hdr.numPages > 1);
	bHdrChanged = true;

	free(tempPage);
	tempPage = NULL;

	return rc;
}

RC indexHandle::getRIDbyKey(void *key, RID &rid) {
	if (key == NULL) {
		return -1;
	}

	BtreeNode *leaf = findLfNode(key);

	rid = leaf->findRID(key);
	return 0;
}

int indexHandle::getHeight() {
	return hdr.treeHeight;
}

void indexHandle::setHeight(const int &treeHeight) {

	for (int i = 1; i < hdr.treeHeight; i++) {
		if (path != NULL && path[i] != NULL) {
			delete path[i];
			path[i] = NULL;
		}
	}

	if (path != NULL) {
		delete []path;
		path = NULL;
	}

	if (pathPos != NULL) {
		delete []pathPos;
		pathPos = NULL;
	}

	hdr.treeHeight = treeHeight;
	path = new BtreeNode* [treeHeight];
	pathPos = new int [treeHeight - 1];

	if(root != NULL)
		path[0] = root;
	else
	{
		root = new BtreeNode(hdr.attrType, *fileHandle, hdr.rootPgNum);
		path[0] = root;
	}
	for (int i = 1; i < hdr.treeHeight; i++) {
		path[i] = NULL;
	}

	for (int i = 0; i < hdr.treeHeight - 1; i++) {
		pathPos[i] = -1;
	}

	bHdrChanged = true;
}

RC indexHandle::disposeNode(FileHandle &fileHandle, BtreeNode *btreeNode) {
	RC rc = 0;

	hdr.numPages--;
	bHdrChanged = true;

	return rc;
}

RC indexHandle::insertEntry(FileHandle &fileHandle, const Attribute &attribute, const void *key, const RID &rid) {
	RC rc = 0;

	BtreeNode *node = findLfNode(key);

	// error: entry already exists
	int pos = node->findKey(key, rid);
	if (pos != -1)
		return -1;

	// result: -1 overflow
	//			0 successful
	int result = node->insertKey(key, rid);

	const void *tempKey = key;
	RID tempRID = rid;
	RID nodeRID(node->getPageNum(),0);
	RID splitRID;
	BtreeNode *splitNode = NULL;

	int level = hdr.treeHeight - 1;
	level--;

	// recursively deal with overflow
	while (result == -1)
	{
		rc = allocateNode(fileHandle, splitNode, splitRID.pageNum);


		//cout << "**************a new node is created, page number is " << splitRID.pageNum << endl;

		if (rc == -1)
			return rc;

		// Split entries also set left/right pointers between them
		node->split(splitNode);
		// Set left pointer of the node that is on the right of the newly splited node
		if(splitNode->getRight() != -1)
		{
			BtreeNode *tempNode = fetchNode(splitNode->getRight());
			tempNode->setLeft(splitNode->getPageNum());
			delete tempNode;
			tempNode = NULL;
		}

		// Insert entry into node or splitNode
		if (node->cmpKey(node->largestKey(), tempKey) >= 0)
			node->insertKey(tempKey, tempRID);
		else
			splitNode->insertKey(tempKey, tempRID);

		// Root node was split
		if (level < 0)
			break;

		BtreeNode *parent = path[level];
		int parentPos = pathPos[level];

		if (parentPos == (parent->getNumRIDs() - 1)) { // last RID

			result = parent->insertKey(node->largestKey(), nodeRID);

			if (result == -1) {
				cout << "abnormal situation" << endl;
				return -1;
			}

			bool flag = false;
			int k1, k2;
			switch (hdr.attrType) {
			case TypeInt:
				k1 = *(int *)(node->largestKey());
				k2 = *(int *)(splitNode->largestKey());

				//cout << "node largest key is " << k1 << endl;
				//cout << "splitnode largest key is " << k2 << endl << endl;

				if (k1 == k2) {
					flag = true;
				}
				break;
			default:
				break;
			}

			if (flag == true) {
				parent->setRID(parent->findKeyPosition(splitNode->largestKey()) + 1, splitRID);
			}
			else {
				parent->setRID(parent->findKeyPosition(splitNode->largestKey()), splitRID);
			}

		}
		else {
			/*
			RID rid2;
			rid2.pageNum = node->getPageNum();
			rid2.slotNum = 0;

			parent->removeKey(NULL, parentPos);
			parent->insertKey(node->largestKey(), rid2);

			// insert new key/rid
			result = parent->insertKey(splitNode->largestKey(), splitRID); // overflow may happen here
			*/

			if (parent->getNumKeys() == 1) {

				RID lastRID = parent->getRID(1);

				parent->removeKey(NULL, parentPos);
				result = parent->insertKey(node->largestKey(), nodeRID);

				parent->setRID(1, lastRID);

				result = parent->insertKey(splitNode->largestKey(), splitRID); // overflow may happen here

			}
			else {


				parent->removeKey(NULL, parentPos);
				result = parent->insertKey(node->largestKey(), nodeRID);
				result = parent->insertKey(splitNode->largestKey(), splitRID); // overflow may happen here

			}


		}


		tempKey = splitNode->largestKey();
		tempRID = splitRID;

		// Write node and splitNode to disk
		if(path[level+1] == node)
			path[level+1]=NULL;
		delete node;
		node = NULL;
		delete splitNode;
		splitNode = NULL;

		// Change node to parent, prepare for one level up
		node = parent;
		nodeRID = RID(node->getPageNum(), 0);

		// Move one level up
		level--;

	} // while

	if (level >= 0)
	{
		//TODO write path to disk
		if(path[level+1] == node)
			path[level+1]=NULL;
		delete node;
		node = NULL;

		if (bHdrChanged)
			writeHdrPage(fileHandle);
		return 0;
	}
	if (result != -1 && level < 0) { // no root split
		//cout << "node's pageNum: " << node->getPageNum() << endl;
		//root should not be freed
		//free(node);
		//node = NULL;


		if (bHdrChanged)
			writeHdrPage(fileHandle);

		return 0;

	}
	if (result == -1 && level < 0) { // root split
		// save root
		BtreeNode *oldRoot = root;
		tempRID.pageNum = oldRoot->getPageNum();
		tempRID.slotNum = 0;
		RID ridRoot;
		rc = allocateNode(fileHandle, root, ridRoot.pageNum);
		root->insertKey(node->largestKey(), tempRID);


		bool flag = false;
		int k1, k2;
		switch (hdr.attrType) {
		case TypeInt:
			k1 = *(int *)(node->largestKey());
			k2 = *(int *)(splitNode->largestKey());

			//cout << "node largest key is " << k1 << endl;
			//cout << "splitnode largest key is " << k2 << endl << endl;

			if (k1 == k2) {
				flag = true;
			}
			break;
		default:
			break;
		}


		if (flag == true) {
			root->setRID(root->findKeyPosition(splitNode->largestKey()) + 1,splitRID);

		}
		else {
			root->setRID(root->findKeyPosition(splitNode->largestKey()),splitRID);
		}

		//root->insertKey(splitNode->largestKey(), splitRID);
		hdr.rootPgNum = ridRoot.pageNum;
		//oldRoot->split(splitNode);

		// TODO write path to disk
		delete oldRoot;
		oldRoot = NULL;
		delete splitNode;
		splitNode = NULL;

		setHeight(hdr.treeHeight + 1);


		if (bHdrChanged)
			writeHdrPage(fileHandle);

		return 0;
	}
	// TODO shouldn't be here satisfy gcc warning
	return -1;
}

RC indexHandle::deleteEntry(FileHandle &fileHandle, const Attribute &attr, const void *key, const RID &rid) {
	if (key == NULL) {
		return -1;
	}

	BtreeNode *node = findLfNode(key);
	assert(node != NULL);

	int pos = node->findKey(key, rid);
	if (pos == -1) {
		return 1;	// positive error code
	}

	int result = node->removeKey(key, pos);
	int level = hdr.treeHeight - 1;
	if (hdr.treeHeight == 1) {
		// root node, do not free it
		//free(node);
		//node = NULL;

		if (bHdrChanged)
			writeHdrPage(fileHandle);

		// root node removes a key, result always is good
		//return result;
		return 0;
	}

	else { // tree height >= 2
		// leaf node underflow
		if (node->getNumKeys() == 0) {

			// link left and right nodes together
			BtreeNode *leftNode = fetchNode(node->getLeft());
			BtreeNode *rightNode = fetchNode(node->getRight());

			if (leftNode != NULL) {
				if (rightNode != NULL) {
					leftNode->setRight(node->getRight());
				}
				else {
					leftNode->setRight(-1);
				}
			}

			if (rightNode != NULL) {
				if (leftNode != NULL) {
					rightNode->setLeft(node->getLeft());
				}
				else {
					rightNode->setLeft(-1);
				}
			}

			disposeNode(fileHandle, node);

			if (path[level] == node)
				path[level] = NULL;
			delete node;
			node = NULL;

			level--;
			bool underFlowFlag = true;
			BtreeNode *parent = path[level];

			while (level >= 1 && underFlowFlag == true) { // non-root nodes

				int parentPos = pathPos[level];
				parent = path[level];

				result = parent->removeKey(NULL, parentPos);
				if (result == -1) {
					return result;
				}

				if (parent->getNumByte() < (PAGE_SIZE / 2)) { // underflow happens
					underFlowFlag = true;

					bool mergeSuccess = false;
					BtreeNode *neighborNode = NULL;
					BtreeNode *upperNode = path[level - 1];
					int upperPos = pathPos[level - 1];

					int neighborLeft = parent->getLeft();
					int neighborRight = parent->getRight();

					if (neighborLeft != -1) { // left neighbor exists

						neighborNode = new BtreeNode(hdr.attrType, fileHandle, neighborLeft);
						RC rc = neighborNode->merge(parent);
						if (rc == -1) {
							mergeSuccess = false;
						}
						mergeSuccess = true;

						upperNode->setKey(upperPos - 1, neighborNode->largestKey());
					}
					else if ((neighborRight != -1) && mergeSuccess == false) { //right neighbor exists

						neighborNode = new BtreeNode(hdr.attrType, fileHandle, neighborRight);
						RC rc = neighborNode->merge(parent);
						if (rc == -1) {
							mergeSuccess = false;
						}
						mergeSuccess = true;

						upperNode->setKey(upperPos + 1, neighborNode->largestKey());
					}
					else { // no neighbor exists
						mergeSuccess = false;
					}

					if (mergeSuccess == false) {
						return -1;
					}

					// write parent and neighborNode to disk
					if(path[level] == parent)
						path[level] = NULL;
					free(parent);
					parent = NULL;
					free(neighborNode);
					neighborNode = NULL;
					free(upperNode);
					upperNode = NULL;

					// one level up
					level--;

				}
				else {
					underFlowFlag = false;

				}
			} // non-root nodes


			if (level == 0 && underFlowFlag == true) { // root node
				if (root->getNumKeys() == 1) { // tree height decreases
					int parentPos = pathPos[level]; // could be 0 or 1
					RID rid;
					if (parentPos == 0) {
						rid = root->getRID(1);
					}
					else {
						rid = root->getRID(0);
					}


					if (path[0] == root)
						path[0] = NULL;
					delete root;
					root = NULL;


					BtreeNode *tempNode = fetchNode(rid);
					disposeNode(fileHandle, root);

					root = tempNode;
					hdr.rootPgNum = root->getPageNum();
					setHeight(hdr.treeHeight - 1);
					bHdrChanged = true;
					//TODO bHdrCHanged == true write to disk
					if (bHdrChanged)
						writeHdrPage(fileHandle);

					return 0;
				}
				else {
					int parentPos = pathPos[level];

					result = root->removeKey(NULL, parentPos);
					if (result == -1) {
						return result;
					}

					if (bHdrChanged)
						writeHdrPage(fileHandle);

					return 0;
				}
			}
		}
		else { // leaf node no underflow
			// write node to disk
			if(path[level] == node)
				path[level] = NULL;

			free(node);
			node = NULL;

			if (bHdrChanged)
				writeHdrPage(fileHandle);

			return 0;
		}
	}
	// TODO shouldn't be here satisfy gcc warning
	return -1;
}

RC indexHandle::writeHdrPage(FileHandle &fileHandle) {

	RC rc = -1;

	char *tempPage = new char[PAGE_SIZE];
	rc = fileHandle.readPage(0, tempPage);
	if (rc != 0)
		return rc;

	memcpy(tempPage, &hdr, sizeof(hdr));

	rc = fileHandle.writePage(0, tempPage);
	if (rc != 0)
		return rc;

	delete tempPage;
	tempPage = NULL;

	return rc;

}

BtreeNode ** indexHandle::getPath()
{
	return path;
}
