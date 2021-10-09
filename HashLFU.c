#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <HashLFU.h>


void Hash_Init(Table* tab, int size)
{
	tab->bucket = malloc(sizeof(FileList)*size);
	tab->maxSize = size;

	for(int i = 0; i < size; i++) 
		list_init(&(tab->bucket[i]));
}

size_t Hash_Function(Table* t, char* key)
{
	int hash_key = 0;

	for(int i = 0; i < strlen(key); i++)
	{
		hash_key += key[i];
	}

	return hash_key % t->maxSize;
}

void Hash_Insert(Table* t, int frq, char* fName, char* fText, int fStat, long fSize)
{
	int index = Hash_Function(t, fName);
	push_node(&(t->bucket[index]), frq, fName, fText, fStat, fSize);
}

/*
void minfreq (Table* t) {
	return;
}


void LFU_Remove(NodeFile* root, long* rem) {
}

void RemoveFile(NodeFile* toDel, NodeFile* tree, char* Vfile) {
}
*/

void Hash_Destroy(Table* t)
{
	for(size_t i = 0; i < t->maxSize; i++)
	{
		list_destroy(&(t->bucket[i]));
	}
}


