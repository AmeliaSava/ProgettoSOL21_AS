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
	tab->curSize = 0;
	
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
void Hash_Insert(Table* t, int frq, char* fName, int fStat)
{
	int index = Hash_Function(t, fName);
	
	node_push(&(t->bucket[index]), frq, fName, fStat);

	t->curSize++;

	return;
	
}

//ok
FileNode* Hash_SearchNode(Table* tab, char* fName)
{
	int index = Hash_Function(tab, fName);

	FileNode* found = node_search(tab->bucket[index].head, fName);

	return found;
}

void Hash_Inc(Table* tab, FileNode* node)
{
	int index = Hash_Function(tab, node->nameFile);
	
	node_incrfreq(node);
	node_movetofront(&(tab->bucket[index]), node->nameFile, strlen(node->nameFile));

	return;
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
	
	tab->curSize--;

	return;
}

//ok
void Hash_Remove(Table* tab, char* Vfile) 
{
	fprintf(stderr, "dentro hash remove\n");
	int index = Hash_Function(tab, Vfile);
	print_list((tab->bucket[index].head));
	node_delete(&(tab->bucket[index]), Vfile, strlen(Vfile));
	tab->curSize--;
	return;
}

void Hash_Read (Table* tab, int n, FileNode** to_send, int* tot)
{
	if(n == 0) n = tab->curSize;

	fprintf(stderr, "%d\n", n);
	fprintf(stderr, "%d\n", tab->maxSize);

	for(int i = 0; i < tab->maxSize && n != 0; i++)
	{
		FileNode* current = tab->bucket[i].head;
		fprintf(stderr, "index: %d\n", i);
			
		if(current == NULL) continue;
		//Sono tutte null???
		while (current != NULL && n != 0)
		{
			fprintf(stderr, "N: %d\n", n);
			node_insert(to_send, current);
			current = current->next;
			n--;
			(*tot)++;
		}
	}
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


