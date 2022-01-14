#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <HashLFU.h>

void Hash_Init(Table* tab, int size)
{
	//initialize the table 
	tab->bucket = safe_malloc(sizeof(FileList) * size);
	tab->maxSize = size;
	tab->curSize = 0;
	
	//creates all the lists
	for(int i = 0; i < size; i++)
	{
		list_init(&(tab->bucket[i]));
	}
		
}

size_t Hash_Function(Table* t, char* key)
{
	int hash_key = 0;
	
	//sums all the letters to obtain key value
	for(int i = 0; i < strlen(key); i++)
	{
		hash_key += key[i];
	}

	return hash_key % t->maxSize;
}

void Hash_Insert(Table* t, int frq, char* fName, int fStat)
{
	//get the list index
	int index = Hash_Function(t, fName);
	
	//insert it
	node_push(&(t->bucket[index]), frq, fName, fStat);

	//increase current size
	t->curSize++;

	return;
	
}

FileNode* Hash_SearchNode(Table* tab, char* fName)
{
	int index = Hash_Function(tab, fName);

	//use a list function to find and save the node
	FileNode* found = node_search(tab->bucket[index].head, fName);

	return found;
}

void Hash_Inc(Table* tab, FileNode* node)
{
	int index = Hash_Function(tab, node->nameFile);
	
	//increases frequnecy
	node_incrfreq(node);
	//moves the node to the front
	node_movetofront(&(tab->bucket[index]), node->nameFile, strlen(node->nameFile));

	return;
}

FileNode* Hash_LFUremove (Table* tab)
{
	//find tail with min freq
	int min = MAX_INT;
	FileList* minFile = NULL;

	//search the table
	for(size_t i = 0; i < tab->maxSize; i++)
	{
		//if there is a last element and its frequency is <= of the min found until now
		if( (tab->bucket[i].last != NULL) && ((tab->bucket[i].last->frequency) <= min) )
		{
			//node is new min
			min = tab->bucket[i].last->frequency;
			//save also the bucket list
			minFile = &(tab->bucket[i]);
		
		}
	}

	//printf("Min '%s': %d\n", minFile->last->nameFile, minFile->last->frequency);
	//printf("\n");
	//list_print(minFile->head);

	//delete and return the last node
	FileNode* expelled = list_pop_return(minFile);
	//fprintf(stderr, "%s\n", expelled->nameFile);

	//decrease tab size
	tab->curSize--;

	return expelled;
}

void Hash_Remove(Table* tab, char* Vfile) 
{
	//in which list is the element?
	int index = Hash_Function(tab, Vfile);
	//delete it
	node_delete(&(tab->bucket[index]), Vfile, strlen(Vfile));
	//decrease tab size
	tab->curSize--;
	return;
}

void Hash_Read (Table* tab, int n, FileNode** to_send, int* tot)
{
	//if n=o read all files
	if(n == 0) n = tab->curSize;

	//fprintf(stderr, "%d\n", n);
	//fprintf(stderr, "%d\n", tab->maxSize);

	//look in all lists
	for(int i = 0; i < tab->maxSize && n != 0; i++)
	{
		//start reading list
		FileNode* current = tab->bucket[i].head;
		
		//if empty continue
		if(current == NULL) continue;

		//while there are files, read them, decreas n, and increas total of read files
		while (current != NULL && n != 0)
		{
			node_insert(to_send, current);
			current = current->next;
			n--;
			(*tot)++;
		}
	}
	return;
}

void Hash_Print (Table* tab)
{
	for(size_t i = 0; i < tab->maxSize; i++)
	{
		//printf("Printing hash bucket[%zu]\n", i);
		//printf("\n");
		list_print(tab->bucket[i].head);
	}
}

void Hash_Destroy(Table* t)
{
	for(size_t i = 0; i < t->maxSize; i++)
	{
		list_destroy(&(t->bucket[i]));
	}

	free(t->bucket);

	return;
}


