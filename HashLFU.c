#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <HashLFU.h>

//ok
void Hash_Init(Table* tab, int size)
{
	tab->bucket = malloc(sizeof(FileList) * size);
	tab->maxSize = size;
	
	for(int i = 0; i < size; i++)
	{
		list_init(&(tab->bucket[i]));
	}
		
}

//ok
size_t Hash_Function(Table* t, char* key)
{
	int hash_key = 0;

	for(int i = 0; i < strlen(key); i++)
	{
		hash_key += key[i];
	}

	return hash_key % t->maxSize;
}

//ok
void Hash_Insert(Table* t, int frq, char* fName, char* fText, int fStat, long fSize)
{
	int index = Hash_Function(t, fName);
	
	push_node(&(t->bucket[index]), frq, fName, fText, fStat, fSize);
	
}

//ok
FileNode* Hash_Search(Table* tab, char* fName)
{
	int index = Hash_Function(tab, fName);

	FileNode* found = search_node(tab->bucket[index].head, fName);

	return found;
}

/*
void LFU_Remove (Table* t) {
	//find tail with min freq
	//tail delete
	return;
}

void RemoveFile(NodeFile* toDel, NodeFile* tree, char* Vfile) {
	//search
	//remove
}
*/

void Hash_Destroy(Table* t)
{
	for(size_t i = 0; i < t->maxSize; i++)
	{
		list_destroy(&(t->bucket[i]));
	}
}


