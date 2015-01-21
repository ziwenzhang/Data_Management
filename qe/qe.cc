
#include "qe.h"

// Class Filter
Filter::Filter(Iterator* input, const Condition &condition)
{
	iterator = 0;
	input->getAttributes(attrs);

	char *data = new char[PAGE_SIZE];
	int flag;
	int offset;
	unsigned i;

	while(input->getNextTuple(data) != QE_EOF)
	{
		flag = 1;
		offset = 0;
		for(i = 0; i < attrs.size() && flag != 0; i++)
		{
			if(attrs.at(i).name == condition.lhsAttr)
			{
				// Check if the tuple satisfying the given condition
				if(attrs[i].type == TypeVarChar)
				{
					int lLength, rLength;
					memcpy(&lLength, data + offset, sizeof(int));
					memcpy(&rLength, (char*)condition.rhsValue.data, sizeof(int));

					char *lstring = new char[lLength + 1];
					char *rstring = new char[rLength + 1];
					memcpy(lstring, data + offset + sizeof(int), lLength);
					memcpy(rstring, (char*)condition.rhsValue.data + sizeof(int), rLength);
					lstring[lLength] = '\0';
					rstring[rLength] = '\0';

					int varcharcmp = strcmp(lstring, rstring);

					offset += sizeof(int) + lLength;

					switch(condition.op)
					{
						case EQ_OP: flag = (varcharcmp == 0); break;
						case LT_OP: flag = (varcharcmp < 0); break;
						case GT_OP: flag = (varcharcmp > 0); break;
						case LE_OP: flag = (varcharcmp <= 0); break;
						case GE_OP: flag = (varcharcmp >= 0); break;
						case NE_OP: flag = (varcharcmp != 0); break;
						case NO_OP: flag = 1;
					}

					delete []lstring;
					lstring = NULL;
					delete []rstring;
					rstring = NULL;
				}
				else if(attrs[i].type == TypeInt)
				{
					int lhsAttr, rhsValue;
					memcpy(&rhsValue, (char *)condition.rhsValue.data, sizeof(int));
					memcpy(&lhsAttr, data + offset, sizeof(int));
					switch(condition.op)
					{
						case EQ_OP: flag = (lhsAttr == rhsValue); break;
						case LT_OP: flag = (lhsAttr < rhsValue); break;
						case GT_OP: flag = (lhsAttr > rhsValue); break;
						case LE_OP: flag = (lhsAttr <= rhsValue); break;
						case GE_OP: flag = (lhsAttr >= rhsValue); break;
						case NE_OP: flag = (lhsAttr != rhsValue); break;
						case NO_OP: flag = 1;
					}
					offset += sizeof(int);
				}
				else
				{
					// Case for real type
					float lhsAttr, rhsValue;
					memcpy(&rhsValue, (char *)condition.rhsValue.data, sizeof(float));
					memcpy(&lhsAttr, data + offset, sizeof(float));
					switch(condition.op)
					{
						case EQ_OP: flag = (lhsAttr == rhsValue); break;
						case LT_OP: flag = (lhsAttr < rhsValue); break;
						case GT_OP: flag = (lhsAttr > rhsValue); break;
						case LE_OP: flag = (lhsAttr <= rhsValue); break;
						case GE_OP: flag = (lhsAttr >= rhsValue); break;
						case NE_OP: flag = (lhsAttr != rhsValue); break;
						case NO_OP: flag = 1;
					}
					offset += sizeof(float);
				}
			}
			else
			{
				if(attrs[i].type == TypeVarChar)
				{
					int varcharLength;
					memcpy(&varcharLength, data + offset, sizeof(int));
					offset += sizeof(int) + varcharLength;
				}
				else
				{
					offset += sizeof(int); //sizeof(int)=sizeof(real)
				}
			}
		}

		if(flag == 1)
		{
			// This tuple satisfies the given condition
			// Write it to our result
			char *temp = new char[offset];
			memset(temp, 0, offset);
			memcpy(temp, data, offset);

			dataSelect.push_back(temp);
			len.push_back(offset);
		}
	}
	delete []data;
	data = NULL;
}

Filter::~Filter()
{
	for(vector< void * > ::iterator iter = dataSelect.begin(); iter != dataSelect.end(); ++iter )
	{
		if(*iter != NULL)
		{
			delete ((char*)*iter);
			*iter = NULL;
		}
	}
}

RC Filter::getNextTuple(void *data)
{
	int size = dataSelect.size();
	if(iterator >= size)
		return QE_EOF;

	memcpy((char *)data, dataSelect[iterator], len[iterator]);
	iterator++;

	return 0;
}

void Filter::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->attrs;
}

// Class Project
Project::Project(Iterator *input, const vector<string> &attrNames)
{
	vector<Attribute> attrsOld;
	iterator = 0;
	input->getAttributes(attrsOld);

	this->attrs.clear();
	for(unsigned i = 0; i < attrsOld.size(); i++)
	{
		for(unsigned j = 0; j < attrNames.size(); j++)
		{
			if(attrsOld.at(i).name == attrNames.at(j))
				this->attrs.push_back(attrsOld.at(i));
		}
	}

	char *data = new char[PAGE_SIZE];
	unsigned i, j;

	while(input->getNextTuple(data) != QE_EOF)
	{
		char *newTuple = new char[PAGE_SIZE];
		int newTupleOffset = 0;
		int offset = 0;
		// Do projection
		for(i = 0; i < attrsOld.size(); i++)
		{
			for(j = 0; j < attrNames.size(); j++)
			{
				if(attrsOld.at(i).name == attrNames.at(j))
				{
					// project
					if(attrsOld.at(i).type == TypeVarChar)
					{
						int varcharLength;
						memcpy(&varcharLength, data + offset, sizeof(int));
						memcpy(newTuple + newTupleOffset, data + offset, varcharLength);
						newTupleOffset += sizeof(int) + varcharLength;
					}
					else
					{
						memcpy(newTuple + newTupleOffset, data + offset, sizeof(int));
						newTupleOffset += sizeof(int);
					}
					// not sure if this break can move us out
					break;
				}
			}

			if(attrsOld.at(i).type == TypeVarChar)
			{
				int varcharLength;
				memcpy(&varcharLength, data + offset, sizeof(int));
				offset += sizeof(int) + varcharLength;
			}
			else
			{
				offset += sizeof(int);
			}
		}

		// Write result
		dataProject.push_back(newTuple);
		len.push_back(newTupleOffset);
	}

	delete []data;
	data = NULL;
}

Project::~Project()
{
	for(vector< void * > ::iterator iter = dataProject.begin(); iter != dataProject.end(); ++iter )
	{
		if(*iter != NULL)
		{
			delete ((char*)*iter);
			*iter = NULL;
		}
	}
}

RC Project::getNextTuple(void *data)
{
	int size = dataProject.size();
	if(iterator >= size)
		return QE_EOF;

	memcpy((char *)data, this->dataProject.at(iterator), this->len.at(iterator));
	iterator++;

	return 0;
}

void Project::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->attrs;
}

// Class NLJoin
NLJoin::NLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const unsigned numPages)
{
	if(condition.bRhsIsAttr != true)
		return;

	vector<Attribute> attrs1;
	unsigned size1, size2;

	char *tupleLeft = new char[PAGE_SIZE];
	char *tupleRight = new char[PAGE_SIZE];


	leftIn->getAttributes(attrs);
	size1 = attrs.size();
	rightIn->getAttributes(attrs1);
	size2 = attrs1.size();

	attrs.insert(attrs.end(),attrs1.begin(),attrs1.end());
	iterator=0;

	while(leftIn->getNextTuple(tupleLeft) != QE_EOF)
	{
		// Get the attr in left
		int offsetLeft = 0;
		int attrLengthLeft = 0;
		int lengthLeft = 0;
		AttrType typeLeft;
		for(unsigned i = 0; i < size1; i++)
		{
			if(attrs.at(i).name == condition.lhsAttr)
			{
				if(attrs.at(i).type == TypeVarChar)
				{
					int varcharLength;
					memcpy(&varcharLength, tupleLeft + lengthLeft, sizeof(int));
					offsetLeft = lengthLeft;
					lengthLeft += sizeof(int) + varcharLength;
					attrLengthLeft = sizeof(int) + varcharLength;
					typeLeft = attrs.at(i).type;
				}
				else
				{
					offsetLeft = lengthLeft;
					lengthLeft += sizeof(int);
					attrLengthLeft = sizeof(int);
					typeLeft = attrs.at(i).type;
				}
			}
			else
			{
				if(attrs.at(i).type == TypeVarChar)
				{
					int varcharLength;
					memcpy(&varcharLength, tupleLeft + lengthLeft, sizeof(int));
					lengthLeft += sizeof(int) + varcharLength;
				}
				else
				{
					lengthLeft += sizeof(int);
				}
			}
		}

		// Scan the right relation to join all satisfying tuples
		rightIn->setIterator();
		while(rightIn->getNextTuple(tupleRight)!=QE_EOF)
		{
			// Get the attr in right
			int offsetRight = 0;
			int attrLengthRight = 0;
			int lengthRight = 0;
			AttrType typeRight;
			int flag = 0;

			for(unsigned i = 0; i < size2; i++)
			{
				if(attrs1.at(i).name == condition.rhsAttr)
				{
					if(attrs1.at(i).type == TypeVarChar)
					{
						int varcharLength;
						memcpy(&varcharLength, tupleRight + lengthRight, sizeof(int));
						offsetRight = lengthRight;
						lengthRight += sizeof(int) + varcharLength;
						attrLengthRight = sizeof(int) + varcharLength;
						typeRight = attrs1.at(i).type;
					}
					else
					{
						offsetRight = lengthRight;
						lengthRight += sizeof(int);
						attrLengthRight = sizeof(int);
						typeRight = attrs1.at(i).type;
					}
				}
				else
				{
					if(attrs1.at(i).type == TypeVarChar)
					{
						int varcharLength;
						memcpy(&varcharLength, tupleRight + lengthRight, sizeof(int));
						lengthRight += sizeof(int) + varcharLength;
					}
					else
					{
						lengthRight += sizeof(int);
					}
				}
			}

			if(typeLeft != typeRight)
				return;

			if(typeLeft == TypeVarChar)
			{
				char *attrLeft = new char[attrLengthLeft + 1];
				char *attrRight = new char[attrLengthRight + 1];
				memcpy(attrLeft, tupleLeft + offsetLeft, attrLengthLeft);
				memcpy(attrRight, tupleRight + offsetRight, attrLengthRight);
				attrLeft[attrLengthLeft] = '\0';
				attrRight[attrLengthRight] = '\0';

				int varcharcmp = strcmp(attrLeft, attrRight);

				switch(condition.op)
				{
					case EQ_OP: flag = (varcharcmp == 0); break;
					case LT_OP: flag = (varcharcmp < 0); break;
					case GT_OP: flag = (varcharcmp > 0); break;
					case LE_OP: flag = (varcharcmp <= 0); break;
					case GE_OP: flag = (varcharcmp >= 0); break;
					case NE_OP: flag = (varcharcmp != 0); break;
					case NO_OP: flag = 1;
				}

				delete []attrLeft;
				attrLeft = NULL;
				delete []attrRight;
				attrRight = NULL;
			}
			else if(typeLeft == TypeInt)
			{
				int attrLeft, attrRight;
				memcpy(&attrLeft, tupleLeft + offsetLeft, attrLengthLeft);
				memcpy(&attrRight, tupleRight + offsetRight, attrLengthRight);
				switch(condition.op)
				{
					case EQ_OP: flag = (attrLeft == attrRight); break;
					case LT_OP: flag = (attrLeft < attrRight); break;
					case GT_OP: flag = (attrLeft > attrRight); break;
					case LE_OP: flag = (attrLeft <= attrRight); break;
					case GE_OP: flag = (attrLeft >= attrRight); break;
					case NE_OP: flag = (attrLeft != attrRight); break;
					case NO_OP: flag = 1;
				}
			}
			else
			{
				float attrLeft, attrRight;
				memcpy(&attrLeft, tupleLeft + offsetLeft, attrLengthLeft);
				memcpy(&attrRight, tupleRight + offsetRight, attrLengthRight);
				switch(condition.op)
				{
					case EQ_OP: flag = (attrLeft == attrRight); break;
					case LT_OP: flag = (attrLeft < attrRight); break;
					case GT_OP: flag = (attrLeft > attrRight); break;
					case LE_OP: flag = (attrLeft <= attrRight); break;
					case GE_OP: flag = (attrLeft >= attrRight); break;
					case NE_OP: flag = (attrLeft != attrRight); break;
					case NO_OP: flag = 1;
				}
			}

			if(flag == 1)
			{
				char *joinedTuple = new char[lengthLeft + lengthRight];
				memcpy(joinedTuple, tupleLeft, lengthLeft);
				memcpy(joinedTuple + lengthLeft, tupleRight, lengthRight);

				dataNLJoin.push_back(joinedTuple);
				len.push_back(lengthLeft + lengthRight);
			}
		}
	}

	delete []tupleLeft;
	tupleLeft = NULL;
	delete []tupleRight;
	tupleRight = NULL;
}

NLJoin::~NLJoin()
{
	for(vector< void * > ::iterator iter = dataNLJoin.begin(); iter != dataNLJoin.end(); ++iter )
	{
		if(*iter != NULL)
		{
			delete ((char*)*iter);
			*iter = NULL;
		}
	}
}

RC NLJoin::getNextTuple(void *data)
{
	int size = dataNLJoin.size();
	if(iterator >= size)
		return QE_EOF;

	memcpy((char *)data, this->dataNLJoin.at(iterator), this->len.at(iterator));
	iterator++;

	return 0;
}

void NLJoin::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->attrs;
}

//Class INLJoin
INLJoin::INLJoin(Iterator *leftIn, IndexScan *rightIn, const Condition &condition, const unsigned numPages)
{
	vector<Attribute> attrs1;
	unsigned size1,size2;

	leftIn->getAttributes(attrs);
	size1 = attrs.size();
	rightIn->getAttributes(attrs1);
	size2 = attrs1.size();

	attrs.insert(attrs.end(), attrs1.begin(), attrs1.end());
	iterator = 0;
	char *tupleLeft = new char[PAGE_SIZE];
	char *tupleRight = new char[PAGE_SIZE];

	while(leftIn->getNextTuple(tupleLeft) != QE_EOF)
	{
		// Get the attr in left
		int offsetLeft = 0;
		int attrLengthLeft = 0;
		int lengthLeft = 0;
		AttrType typeLeft;
		for(unsigned i = 0; i < size1; i++)
		{
			if(attrs.at(i).name == condition.lhsAttr)
			{
				if(attrs.at(i).type == TypeVarChar)
				{
					int varcharLength;
					memcpy(&varcharLength, tupleLeft + lengthLeft, sizeof(int));
					offsetLeft = lengthLeft;
					lengthLeft += sizeof(int) + varcharLength;
					attrLengthLeft = sizeof(int) + varcharLength;
					typeLeft = attrs.at(i).type;
				}
				else
				{
					offsetLeft = lengthLeft;
					lengthLeft += sizeof(int);
					attrLengthLeft = sizeof(int);
					typeLeft = attrs.at(i).type;
				}
			}
			else
			{
				if(attrs.at(i).type == TypeVarChar)
				{
					int varcharLength;
					memcpy(&varcharLength, tupleLeft + lengthLeft, sizeof(int));
					lengthLeft += sizeof(int) + varcharLength;
				}
				else
				{
					lengthLeft += sizeof(int);
				}
			}
		}

		char *highKey = NULL;
		char *lowKey = NULL;
		bool lowKeyInclusive = false;
		bool highKeyInclusive = false;

		switch(condition.op)
		{
			case EQ_OP:
				highKey = new char[attrLengthLeft];
				lowKey = new char[attrLengthLeft];
				memcpy(highKey, tupleLeft + offsetLeft, lengthLeft);
				memcpy(lowKey, tupleLeft + offsetLeft, lengthLeft);
				lowKeyInclusive = true;
				highKeyInclusive = true;
				break;
			case LT_OP:
				lowKey = new char[attrLengthLeft];
				memcpy(lowKey, tupleLeft + offsetLeft, lengthLeft);
				break;
			case GT_OP:
				highKey = new char[attrLengthLeft];
				memcpy(highKey, tupleLeft + offsetLeft, lengthLeft);
				break;
			case LE_OP:
				lowKey = new char[attrLengthLeft];
				memcpy(lowKey, tupleLeft + offsetLeft, lengthLeft);
				lowKeyInclusive = true;
				break;
			case GE_OP:
				highKey = new char[attrLengthLeft];
				memcpy(highKey, tupleLeft + offsetLeft, lengthLeft);
				highKeyInclusive = true;
				break;
			case NE_OP:
				break;
			case NO_OP:
				break;
		}

		rightIn->setIterator(lowKey, highKey, lowKeyInclusive, highKeyInclusive);

		if(highKey != NULL)
		{
			delete []highKey;
			highKey = NULL;
		}
		if(lowKey != NULL)
		{
			delete []lowKey;
			lowKey = NULL;
		}

		while(rightIn->getNextTuple(tupleRight) != QE_EOF)
		{
			int lengthRight = 0;

			for(unsigned i=0; i < size2; i++)
			{
				if(attrs1[i].type == TypeVarChar)
				{
					int varcharLength;
					memcpy(&varcharLength,(char*)tupleRight + lengthRight, sizeof(int));
					lengthRight += sizeof(int) + varcharLength;
				}
				else
				{
					lengthRight += sizeof(int);
				}
			}

			char *joinedTuple = new char[lengthLeft + lengthRight];
			memcpy(joinedTuple, tupleLeft, lengthLeft);
			memcpy(joinedTuple + lengthLeft, tupleRight, lengthRight);

			dataINLJoin.push_back(joinedTuple);
			len.push_back(lengthLeft + lengthRight);
		}

	}

	delete []tupleLeft;
	tupleLeft = NULL;
	delete []tupleRight;
	tupleRight = NULL;
}

INLJoin::~INLJoin()
{
	for(vector< void * > ::iterator iter = dataINLJoin.begin(); iter != dataINLJoin.end(); ++iter )
	{
		if(*iter != NULL)
		{
			delete ((char*)*iter);
			*iter = NULL;
		}
	}
}

RC INLJoin::getNextTuple(void *data)
{
	int size = dataINLJoin.size();
	if(iterator >= size)
		return QE_EOF;

	memcpy((char *)data, this->dataINLJoin.at(iterator), this->len.at(iterator));
	iterator++;

	return 0;
}

void INLJoin::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->attrs;
}

// Class Aggregate
Aggregate::Aggregate(Iterator *input, Attribute aggAttr, AggregateOp opp)
{
	unsigned i;

	bool first = true;
	vector<Attribute> attrs;

	input->getAttributes(attrs);

	char *data = new char[500];

	float fmin=0.0,fmax=0.0,fsum=0.0,favg=0.0,ftmp,aggcount=0;
	int itmp,tmp;

	iterator =0;

	int offset;

	while(input->getNextTuple(data)!=QE_EOF)
	{
		offset=0;

		for(i=0;i<attrs.size();i++)
		{
			if(attrs[i].name == aggAttr.name)
			{
				if(aggAttr.type == TypeInt)
				{
                    memcpy(&itmp, data+offset, sizeof(int));
					ftmp = (float)(itmp);
				}
				else if(aggAttr.type == TypeReal)
				{
					memcpy(&ftmp, data+offset, sizeof(int));
				}

				if(first)
				{
					fmin = ftmp;
					fmax = ftmp;
					first = false;
				}
				else
				{
					fmin = (fmin<ftmp)? fmin:ftmp;
					fmax = (fmax>ftmp)? fmax:ftmp;
				}
				fsum += ftmp;
				aggcount++;

				offset+=sizeof(int);
			}
			else
			{
				if(attrs[i].type == TypeVarChar)
				{
					memcpy(&tmp, data+offset, sizeof(int));
					offset += sizeof(int)+tmp;
				}
				else
					offset += sizeof(int);
			}
		}
	}

	favg = fsum/aggcount;

	char *temp = new char[4];
	int tmp1;

	switch(opp)
	{
		case MIN:
			if(aggAttr.type == TypeInt)
			{
				tmp1 = (int)fmin;
				memcpy(temp, &tmp1, sizeof(int));
			}
			else
				memcpy(temp, &fmin, sizeof(float));
			break;
		case MAX:
			if(aggAttr.type == TypeInt)
			{
				tmp1 = (int)fmax;
				memcpy(temp, &tmp1, sizeof(int));
			}
			else
				memcpy(temp, &fmax, sizeof(float));
			break;
		case SUM:
			if(aggAttr.type == TypeInt)
			{
				tmp1 = (int)fsum;
				memcpy(temp, &tmp1, sizeof(int));
			}
			else
				memcpy(temp, &fsum, sizeof(float));
			break;
		case AVG: memcpy(temp, &favg, sizeof(float));break;
		case COUNT:
			tmp1 = (int)aggcount;
			memcpy(temp, &tmp1, sizeof(float));
	}

	dataAgg.push_back(temp);
	len.push_back(4);

	string name;

	Attribute dummy;

	switch(opp)
	{
		case MIN: name = "MIN";break;
		case MAX: name = "MAX";break;
		case SUM: name = "SUM";break;
		case AVG: name = "AVG";break;
		case COUNT: name = "COUNT";
	}

	name+="("+aggAttr.name+")";

	dummy.name = name;

	switch(opp)
	{
		case MIN:dummy.type = aggAttr.type;break;
		case MAX:dummy.type = aggAttr.type;break;
		case SUM:dummy.type = aggAttr.type;break;
		case COUNT: dummy.type = TypeInt;break;
		case AVG: dummy.type = TypeReal;
	}

	dummy.length = 4;

	this->attrs.push_back(dummy);

	delete []data;
	data = NULL;
}

struct running_info
{
	float fmin,fmax,fcount,fsum,favg;

	running_info()
	{
		fmin = fmax = fcount = fsum = favg = 0.0;
	}
};


float min(float a,float b)
{
	if(a<b)
		return a;

	return b;
}

template<class T>
void process(vector<T> &gVal,vector<running_info> &gInfo,T gvalue,float value)
{
	unsigned i;
	int flag=0;


	for(i=0;i<gVal.size() && flag==0;i++)
	{
		if(gVal[i]==gvalue)
			flag=1;
	}

	if(flag==0)
	{
		gVal.push_back(gvalue);
		running_info r;

		r.fmin = value;
		r.fmax = value;

		gInfo.push_back(r);
	}
	else
	{
		i--;
	}

	gInfo[i].fmin = min(gInfo[i].fmin,value);
	gInfo[i].fmax = min(gInfo[i].fmax,value);
	gInfo[i].fsum+=value;
	gInfo[i].fcount++;
	gInfo[i].favg = gInfo[i].fsum/gInfo[i].fcount;
}

Aggregate::Aggregate(Iterator *input, Attribute aggAttr, Attribute gAttr, AggregateOp op)
{
	int offset,length;
	unsigned i;
	vector<string> gVal1;
	vector<float> gVal2;
	vector<running_info> gInfo;
	float value;
	string gvalue1;
	float gvalue2;

	vector<Attribute> attrs;

	iterator = 0;

	input->getAttributes(attrs);

	char *data = new char[500];

	while(input->getNextTuple(data)!=QE_EOF)
	{
		offset = 0;

		for(i=0; i<attrs.size(); i++)
		{
			if(attrs[i].name == aggAttr.name)
			{
				if(attrs[i].type == TypeReal)
				{
					memcpy(&value, data+offset, sizeof(float));
				}
				else
				{
					int x;
					memcpy(&x, data+offset, sizeof(float));
					value = (int)x;
				}
			}

			if(attrs[i].name == gAttr.name)
			{
				if(attrs[i].type == TypeVarChar)
				{
					memcpy(&length, data+offset, sizeof(int));
					char *str =new char[length+1];

					memcpy(str, data+offset+sizeof(int), length);

					str[length]='\0';

					gvalue1 = str;

					delete []str;
					str = NULL;
				}
				else
				{
					if(attrs[i].type == TypeReal)
					{
						memcpy(&gvalue2, data+offset, sizeof(int));
					}
					else
					{
						int x;
						memcpy(&x, data+offset, sizeof(int));
						gvalue2 = (float)x;
					}
				}
			}

			if(attrs[i].type == TypeVarChar)
			{
				memcpy(&length, data+offset, sizeof(int));
				offset += sizeof(int) + length;
			}
			else
			{
				offset+=sizeof(int);
			}
		}

		if(gAttr.type == TypeVarChar)
			process(gVal1,gInfo,gvalue1,value);
		else
			process(gVal2,gInfo,gvalue2,value);

	}

	for(i=0; i<gInfo.size(); i++)
	{
		char *temp = new char[200];
		offset =0;

		if(gAttr.type == TypeVarChar)
		{
			length = gVal1[i].length();
			memcpy(temp+offset, &length, sizeof(int));
			offset += sizeof(int);
			memcpy(temp+offset, gVal1[i].c_str(), length);
			offset += length;
		}
		else if(gAttr.type == TypeReal)
		{
			memcpy(temp+offset, &gVal2[i], sizeof(float));
			offset += sizeof(float);
		}
		else
		{
			int tmp1 = (int)gVal2[i];
			memcpy(temp+offset, &tmp1, sizeof(int));
			offset += sizeof(int);
		}

		switch(op)
		{
			case MIN:
				if(gAttr.type == TypeInt)
				{
					int tmp1 = (int)gInfo[i].fmin;
					memcpy(temp+offset, &tmp1, sizeof(int));
				}
				else
					memcpy(temp+offset, &gInfo[i].fmin, sizeof(float));
				break;
			case MAX:
				if(gAttr.type == TypeInt)
				{
					int tmp1 = (int)gInfo[i].fmax;
					memcpy(temp+offset, &tmp1, sizeof(int));
				}
				else
					memcpy(temp+offset, &gInfo[i].fmax, sizeof(float));
				break;
			case AVG: memcpy(temp+offset, &gInfo[i].favg, sizeof(float));break;
			case SUM:
				if(gAttr.type == TypeInt)
				{
					int tmp1 = (int)gInfo[i].fsum;
					memcpy(temp+offset, &tmp1, sizeof(int));
				}
				else
					memcpy(temp+offset, &gInfo[i].fsum, sizeof(float));
				break;
			case COUNT:
				int tmp1 = (int)gInfo[i].fcount;
				memcpy(temp+offset, &tmp1, sizeof(float));
		}

		offset += sizeof(float);

		dataAgg.push_back(temp);
		len.push_back(offset);
	}

	this->attrs.push_back(gAttr);

	string name;

	Attribute dummy;

	switch(op)
	{
		case MIN: name = "MIN";break;
		case MAX: name = "MAX";break;
		case SUM: name = "SUM";break;
		case AVG: name = "AVG";break;
		case COUNT: name = "COUNT";
	}

	name+="("+aggAttr.name+")";

	dummy.name = name;

	switch(op)
	{
		case MIN:dummy.type = aggAttr.type;break;
		case MAX:dummy.type = aggAttr.type;break;
		case SUM:dummy.type = aggAttr.type;break;
		case COUNT: dummy.type = TypeInt;break;
		case AVG: dummy.type = TypeReal;
	}

	dummy.length = 4;

	this->attrs.push_back(dummy);



	delete []data;
	data = NULL;
}


Aggregate::~Aggregate()
{
	for (vector<void *>::iterator iter = dataAgg.begin(); iter != dataAgg.end(); ++iter)
	{
		if (*iter != NULL)
		{
			delete (char *)(*iter);
			*iter = NULL;
		}
	}
}

RC Aggregate::getNextTuple(void *data)
{
	int size = dataAgg.size();
	if(iterator >= size)
		return QE_EOF;

	memcpy((char *)data, dataAgg[iterator], len[iterator]);
	iterator++;

	return 0;
}

void Aggregate::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->attrs;
}

