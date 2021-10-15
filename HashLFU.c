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
	
	node_push(&(t->bucket[index]), frq, fName, fText, fStat, fSize);
	
}

//ok
FileNode* Hash_Search(Table* tab, char* fName)
{
	int index = Hash_Function(tab, fName);

	FileNode* found = node_search(tab->bucket[index].head, fName);

	return found;
}

//ok
void Hash_LFUremove (Table* tab)
{
	//find tail with min freq
	int min = MAX_INT;
	FileList* minFile = NULL;

	for(size_t i = 0; i < tab->maxSize; i++)
	{
		if( (tab->bucket[i].last != NULL) && ((tab->bucket[i].last->frequency) <= min) )
		{
			min = tab->bucket[i].last->frequency;
			minFile = &(tab->bucket[i]);
		
		}
	}
	printf("Min '%s': %d\n", minFile->last->nameFile, minFile->last->frequency);
	printf("\n");
	print_list(minFile->head);
	//last delete
	list_pop(minFile);
	printf("dopo min\n");
	return;
}

//ok
void Hash_Remove(Table* tab, char* Vfile) 
{

	int index = Hash_Function(tab, Vfile);

	node_delete(&(tab->bucket[index]), Vfile, strlen(Vfile));

	return;

}

//ok
void Hash_Destroy(Table* t)
{
	for(size_t i = 0; i < t->maxSize; i++)
	{
		list_destroy(&(t->bucket[i]));
	}
}


