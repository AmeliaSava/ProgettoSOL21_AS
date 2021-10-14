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

static inline void print_list (FileNode* n){

	if(n==NULL) {
		fprintf(stdout, "NULL\n");
		return;
	}
	
	printf("Nome: %s\n", n->nameFile);
	printf("\n");

	print_list(n->next);	
}

//ok
static inline void list_init(FileList* list) 
{
	list->head = NULL;
	list->last = NULL;
	list->size = 0;
}

//ok
//ATTENTION! Text is useless here
static inline void push_node(FileList* list, int frq, char* fName, char* fText, int fStat, long fSize)
{

	FileNode* newNode = safe_malloc(sizeof(FileNode));

	newNode->frequency = frq;
	newNode->status = fStat;
	newNode->FileSize = fSize;
	newNode->nameFile = safe_malloc(strlen(fName)*sizeof(char));
	strncpy(newNode->nameFile, fName, strlen(fName));
	newNode->nameFile[strlen(fName)] = '\0';

	if(list->size == 0) list->head = newNode;
	else list->last->next = newNode;
	list->last = newNode;
	list->size++;
}

//ok
static inline void update_node(FileNode* node, int frq, char* fText, long fSize) 
{

	node->frequency = frq;
	node->textFile = safe_malloc(strlen(fText)*sizeof(char));
	strncpy(node->textFile, fText, strlen(fText));
	node->FileSize = fSize;

}

//ok
static inline FileNode* search_node(FileNode* node, char* filename) 
{
	
	if(node == NULL) return NULL;
	
	if(strncmp(filename, node->nameFile, MAX_SIZE) == 0)
	{
		return node;
	}
	
	return search_node(node->next, filename);
	
}

//ok
static inline void append_node(FileNode* node, int freq, char* append, long newSize) 
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
static inline void increaseF (FileNode* file) {
	file->frequency = file->frequency + 1;
}

static inline void list_destroy(FileList* list)
{
	FileNode* current = list->head;
	FileNode* next;
	while (current != NULL) {
		next = current->next;
		free(current);
		current = next;
	}
	list->head = NULL;
	list->last = NULL;
	free(list);
}

#endif /* FILELIST_H */