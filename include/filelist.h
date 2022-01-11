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
	int lock;
	pid_t lock_pid;
	struct FILE_NODE *next;

} FileNode;

typedef struct FILE_LIST
{
	FileNode* head;
	FileNode* last;
	size_t size;

} FileList;

//AGGIORNARE I PUNTATORI TESTACODA

static inline void list_print (FileNode* n) 
{

	if(n==NULL) {
		//fprintf(stdout, "NULL\n");
		//printf("\n");
		return;
	}
	
	printf("Name: %s\n", n->nameFile);
	//printf("\n");

	list_print(n->next);	
}

static inline void filecpy(FileNode* destination, FileNode* source)
{
	destination->frequency = source->frequency;
	destination->FileSize = source->FileSize;
	destination->status = source->status;
	//fprintf(stderr, "source:%s\n", source->nameFile);
	//fprintf(stderr, "%zu\n", strlen(source->nameFile));
	destination->nameFile = safe_malloc((strlen(source->nameFile) + 1));
	strncpy(destination->nameFile, source->nameFile, (strlen(source->nameFile)) + 1);

	size_t size = source->FileSize;
	

	//destination->textFile = realloc(destination->textFile, size);
	fprintf(stderr, "%zu\n%zu\n", source->FileSize, size);
	destination->textFile = safe_malloc(size + 1);
	memset(destination->textFile, '\0', size + 1);
	memcpy(destination->textFile, source->textFile, size);
	
	destination->lock = source->lock;
	destination->lock_pid = source->lock_pid;
	destination->next = NULL;
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

//ATTENTION non aggiorni mai la testa
static inline FileNode* list_pop_return(FileList* list)
{
	if(list->head == NULL)	return NULL;

	FileNode* current = list->head;
	FileNode* toReturn = safe_malloc(sizeof(FileNode));

	if(current->next == NULL) {
		fprintf(stderr, "current:%s\n", current->textFile);
		filecpy(toReturn, current);
		fprintf(stderr, "toreturn:%s\n", toReturn->textFile);
		free(current);
		list->head = NULL;
		list->size = list->size - 1;
		return toReturn;
	}

	while (current->next->next != NULL)
	{
		current = current->next;
	}

	filecpy(toReturn, current->next);
	free(current->next);
	current->next = NULL;
	list->last = current;
	list->size = list->size - 1;

	return toReturn;
	
}

//ok
static inline void list_destroy(FileList* list)
{
	FileNode* current = list->head;
	FileNode* next;

	while (current != NULL)
	{
		next = current->next;
		free(current->nameFile);
		free(current->textFile);
		free(current);
		current = next;
	}

	list->head = NULL;
	list->last = NULL;

	free(list);

	return;
}

//NODE functions

static inline void node_push(FileList* list, int frq, char* fName, int fStat)
{
	FileNode* newNode = safe_malloc(sizeof(FileNode));

	newNode->frequency = frq;
	newNode->status = fStat;

	newNode->nameFile = safe_malloc(strlen(fName) + 1);
	strncpy(newNode->nameFile, fName, strlen(fName) + 1);

	newNode->FileSize = 0;

	if(list->size == 0) list->head = newNode;
	else list->last->next = newNode;
	list->last = newNode;
	list->size++;
}

static inline void node_insert(FileNode** node, FileNode* ins)
{
	if(*node == NULL) {
		*node = safe_malloc(sizeof(FileNode));
		filecpy(*node, ins);
		return;
	}

	FileNode* current = *node;

	while(current->next !=  NULL) {
		current = current->next;
	}

	current->next =  safe_malloc(sizeof(FileNode));
	filecpy(current->next, ins);

	return;
}

//ok
static inline void node_update(FileNode* node, int frq, char* fText, long fSize) 
{

	node->frequency = frq;
	node->textFile = safe_malloc(fSize);
	memcpy(node->textFile, fText, fSize);
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
	return;
}

static inline void node_lock (FileNode* file, pid_t pid)
{
	//fprintf(stderr, "Node is locked\n");
	file->lock = 1;
	file->lock_pid = pid;
	return;
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
	{
		if(strncmp(current->nameFile, fileName, len) == 0)
		{
			free(current->nameFile);
			free(current->textFile);
			free(current->next);
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
			free(current->next->nameFile);
			free(current->next->textFile);
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
		free(current->next->nameFile);
		free(current->next->textFile);
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