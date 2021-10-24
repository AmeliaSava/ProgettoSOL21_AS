#if !defined(FILELIST_H)
#define FILELIST_H

#include <ops.h>

typedef struct FILE_NODE
{
	int frequency;
	char* textFile;
	char* nameFile;
	int status;
	long FileSize;
	struct FILE_NODE *next;

} FileNode;

typedef struct FILE_LIST
{
	FileNode* head;
	FileNode* last;
	size_t size;

} FileList;

//AGGIORNARE I PUNTATORI TESTACODA

static inline void print_list (FileNode* n) 
{

	if(n==NULL) {
		fprintf(stdout, "NULL\n");
		printf("\n");
		return;
	}
	
	printf("Nome: %s\n", n->nameFile);
	printf("\n");

	print_list(n->next);	
}

//LIST functions

//ok
static inline void list_init(FileList* list) 
{
	list->head = NULL;
	list->last = NULL;
	list->size = 0;
}

//ok
//ATTENZIONE
static inline void list_pop(FileList* list)
{
	if(list->head == NULL) return;

	FileNode* current = list->head;

	if(current->next == NULL) {
		free(current);
		list->head = NULL;
		return;
	}

	while (current->next->next != NULL)
	{
		current = current->next;
	}

	free(current->next);
	current->next = NULL;
	list->last = current;

	return;
	
}

//ok
static inline void list_destroy(FileList* list)
{
	FileNode* current = list->head;
	FileNode* next;

	while (current != NULL)
	{
		next = current->next;
		free(current);
		current = next;
	}

	list->head = NULL;
	list->last = NULL;

	free(list);

	return;
}

//NODE functions
//ok
//ATTENTION! Text is useless here...
static inline void node_push(FileList* list, int frq, char* fName, int fStat)
{
	FileNode* newNode = safe_malloc(sizeof(FileNode));

	newNode->frequency = frq;
	newNode->status = fStat;
	newNode->nameFile = safe_malloc(strlen(fName)*sizeof(char));
	strncpy(newNode->nameFile, fName, strlen(fName));
	newNode->nameFile[strlen(fName)] = '\0';
	newNode->FileSize = 0;

	if(list->size == 0) list->head = newNode;
	else list->last->next = newNode;
	list->last = newNode;
	list->size++;
}

//ok
static inline void node_update(FileNode* node, int frq, char* fText, long fSize) 
{

	node->frequency = frq;
	node->textFile = safe_malloc(strlen(fText)*sizeof(char));
	strncpy(node->textFile, fText, strlen(fText));
	node->FileSize = fSize;

}

//ok
static inline FileNode* node_search(FileNode* node, char* filename) 
{
	
	if(node == NULL) return NULL;
	
	if(strncmp(filename, node->nameFile, MAX_SIZE) == 0)
	{
		return node;
	}
	
	return node_search(node->next, filename);
	
}

//ok
static inline void node_append(FileNode* node, int freq, char* append, long newSize) 
{
	node->frequency = freq;

	if ((node->textFile = (char*)realloc((node->textFile), ((node->FileSize)+newSize))) == NULL)
	{ 
		perror("ERROR: realloc AppendNode");
		free(node->textFile);
		exit(EXIT_FAILURE);
	}

	strncat(node->textFile, append, strlen(append));
	node->FileSize += newSize;
}

//ok
static inline void node_incrfreq (FileNode* file)
{
	file->frequency = file->frequency + 1;
}

//ok
//ATTENTION element not in list fail? return int to check errors?
static inline void node_delete(FileList* list, char* fileName, size_t len) 
{
	fprintf(stderr, "dentro list remove\n");

	if(list->head == NULL) return;
	fprintf(stderr, "%s\n", list->head->nameFile);
	FileNode* current = list->head;
	FileNode* next;

	if(current->next == NULL) 
	{fprintf(stderr, "dentro head remove\n");
		if(strncmp(current->nameFile, fileName, len) == 0)
		{
			free(current);
			list->head = NULL;
			return;
		}
	}

	while (current->next->next != NULL)
	{
		if(strncmp(current->next->nameFile, fileName, len) == 0)
		{
			next = current->next->next;
			free(current->next);
			current->next = next;
			return;
		} else 
		{
			current = current->next;
		}
	}

	if(strncmp(current->next->nameFile, fileName, len) == 0)
	{
		next = current->next->next;
		free(current->next);
		current->next = next;
		list->last = current;
	}

	return;
}

//ATTENTION better use delete???
static inline void node_movetofront(FileList* list, char* fileName, size_t len)
{
	if(list->head == NULL) return;

	FileNode* current = list->head;
	FileNode* next;
	
	if(strncmp(current->nameFile, fileName, len) == 0) return; //element is already at the top
	
	while (current->next->next != NULL)
	{
		if(strncmp(current->next->nameFile, fileName, len) == 0)
		{
			next = current->next->next;
			current->next->next = list->head;
			list->head = current->next;
			current->next = next;
			return;
		} else 
		{
			current = current->next;
		}
	}

	if(strncmp(current->next->nameFile, fileName, len) == 0)
	{
		next = current->next->next;
		current->next->next = list->head;
		list->head = current->next;
		current->next = next;
		list->last = current;
	}

	return;
}


#endif /* FILELIST_H */