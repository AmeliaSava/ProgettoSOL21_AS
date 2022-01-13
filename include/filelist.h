#if !defined(FILELIST_H)
#define FILELIST_H

#include <ops.h>

/**
 * \brief: a struct that defines the files saved on the server side
 * \param frequency: how many times the file was utilized
 * \param textFile: the file contenets
 * \param nameFile: the file name
 * \param status: == 1 if the file is closed, ==0 if the file is open
 * \param FileSize: the size of the file
 * \param lock: == 1 if the file is locked, == 0 if the file is unlocked
 * \param lock_pid: the pid of the process that locked the file
 * \param next: pointer to the next file
 */
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

/**
 * \brief: struct for a list of files
 * \param head: pointer to the head of the list
 * \param last: pointer to the last element
 * \param size: size of the list 
 */
typedef struct FILE_LIST
{
	FileNode* head;
	FileNode* last;
	size_t size;

} FileList;

/**
 * \brief: recursive function that prints the list
 * \param n: pointer to the head of the list
 */
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

/**
 * \brief: a fuction that copies a file into an other
 * \param destination: the destination file
 * \param source: the source file
 */
static inline void filecpy(FileNode* destination, FileNode* source)
{
	destination->frequency = source->frequency;
	destination->FileSize = source->FileSize;
	destination->status = source->status;

	destination->nameFile = safe_malloc((strlen(source->nameFile) + 1));
	strncpy(destination->nameFile, source->nameFile, (strlen(source->nameFile)) + 1);

	size_t size = source->FileSize;

	destination->textFile = safe_malloc(size + 1);
	memset(destination->textFile, '\0', size + 1);
	memcpy(destination->textFile, source->textFile, size);
	
	destination->lock = source->lock;
	destination->lock_pid = source->lock_pid;
	destination->next = NULL;
}

/**
 * \brief: a function to initialize the list
 * \param list: the list that needs to be initialized
 */
static inline void list_init(FileList* list) 
{
	list->head = NULL;
	list->last = NULL;
	list->size = 0;
}

/**
 * \brief: a function that deletes the last element of a list
 * \param list: the list that needs to be popped
 */
static inline void list_pop(FileList* list)
{
	//no list, return
	if(list->head == NULL) return;

	FileNode* current = list->head;

	//one element list
	if(current->next == NULL) {
		free(current);
		list->head = NULL;
		list->last = NULL;
		return;
	}

	//cycling to the last
	while (current->next->next != NULL)
	{
		current = current->next;
	}

	//free the element and update the list
	free(current->next);
	current->next = NULL;
	list->last = current;

	return;
	
}

/**
 * \brief: pops the last element of the lists and returns it
 * \param list: the list the element is taken from
 */
static inline FileNode* list_pop_return(FileList* list)
{
	//list is empty, return
	if(list->head == NULL)	return NULL;

	FileNode* current = list->head;
	FileNode* toReturn = safe_malloc(sizeof(FileNode));

	//list has only one element
	if(current->next == NULL) {
		
		filecpy(toReturn, current);
		free(current->nameFile);
		free(current->textFile);
		free(current);
		list->head = NULL;
		list->last = NULL;
		list->size = list->size - 1;
		return toReturn;
	}

	while (current->next->next != NULL)
	{
		current = current->next;
	}

	filecpy(toReturn, current->next);
	free(current->next->nameFile);
	free(current->next->textFile);
	free(current->next);
	current->next = NULL;
	list->last = current;
	list->size = list->size - 1;

	return toReturn;
	
}

/**
 * \brief: a function to destroys the list
 * \param list: the list that needs to be destroyed
 */
static inline void list_destroy(FileList* list)
{
	FileNode* current = list->head;
	FileNode* next;

	while (current != NULL)
	{
		next = current->next;
		if(current->nameFile) free(current->nameFile);
		if(current->textFile) free(current->textFile);
		free(current);
		current = next;
	}

	return;
}

/**
 * \brief: function that pushes a node without saving file's contents
 * \param list: the list where the node will be pushed
 * \param frq: the starting frequency
 * \param fName: the file's name
 * \param fStat: the initial status of the file
 */
static inline void node_push(FileList* list, int frq, char* fName, int fStat)
{
	//allocates a node
	FileNode* newNode = safe_malloc(sizeof(FileNode));

	newNode->frequency = frq;
	newNode->status = fStat;

	newNode->nameFile = safe_malloc(strlen(fName) + 1);
	strncpy(newNode->nameFile, fName, strlen(fName) + 1);

	newNode->FileSize = 0;

	//if the list is empty insert it as head
	if(list->size == 0) list->head = newNode;
	//else as last
	else list->last->next = newNode;
	list->last = newNode;
	list->size++;
}

/**
 * \brief: inserts a whole node in a list
 * \param node: pointer to list
 * \param ins: the node that needs to be inserted
 */
static inline void node_insert(FileNode** node, FileNode* ins)
{
	//if node is empty
	if(*node == NULL) {
		//allcates the node and copies the values
		*node = safe_malloc(sizeof(FileNode));
		filecpy(*node, ins);
		return;
	}

	FileNode* current = *node;

	//go to the last
	while(current->next !=  NULL) {
		current = current->next;
	}

	//save the node
	current->next =  safe_malloc(sizeof(FileNode));
	filecpy(current->next, ins);

	return;
}

/**
 * \brief: updates an already existing note
 * \param node: the node to be updated
 * \param frq: the new frequency
 * \param fText: the file's contents
 * \param fSize: the size
 */
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
	//the list is empty
	if(list->head == NULL) return;

	FileNode* current = list->head;
	FileNode* next;

	//the list has only one element
	if(current->next == NULL) 
	{
		//and it is the element I need to delete
		if(strncmp(current->nameFile, fileName, len) == 0)
		{
			free(current->nameFile);
			if(current->textFile) free(current->textFile);
			free(current);
			//freed the element now the list head is back to NULL, decrease size
			list->head = NULL;
			list->size = list->size - 1;
			return;
		}
	}

	//search for the element
	while (current->next->next != NULL)
	{
		//fount it
		if(strncmp(current->next->nameFile, fileName, len) == 0)
		{
			//save the next one
			next = current->next->next;

			//free the element
			free(current->next->nameFile);
			if(current->next->textFile) free(current->next->textFile);
			free(current->next);

			//update the list
			current->next = next;
			list->size = list->size - 1;

			return;
		} else 
		{
			//go on searching
			current = current->next;
		}
	}

	//element is the last one
	if(strncmp(current->next->nameFile, fileName, len) == 0)
	{
		//ATTENTION actually just null
		next = current->next->next;

		free(current->next->nameFile);
		if(current->next->textFile) free(current->next->textFile);
		free(current->next);

		current->next = next;
		list->last = current;
		list->size = list->size - 1;
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