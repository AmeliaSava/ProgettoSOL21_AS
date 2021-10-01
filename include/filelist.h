#if !defined(FILELIST_H)
#define FILELIST_H

#include <ops.h>

typedef struct FILE_NODE {
	int frequency;
	char* textFile;
	char* nameFile;
	int status;
	long FileSize;
	struct FILE_NODE *next;
} FileList;

FileList* push_node(FileList* head, int frq, char* fName, char* fText, int fStat, long fSize)
{
	FileList* newNode = safe_malloc(sizeof(FileList));

	newNode->frequency = frq;
	newNode->status = fStat;
	newNode->FileSize = fSize;
	newNode->nameFile = safe_malloc(strlen(fName)*sizeof(char));
	strncpy(newNode->nameFile, fName, strlen(fName));
	newNode->nameFile[strlen(fName)] = '\0';

	head->next = head;
	head = newNode;

	return head;
}

void update_node(FileList* node, int frq, char* fText, long fSize) {
	node->frequency = frq;
	node->textFile = safe_malloc(strlen(fText)*sizeof(char));
	strncpy(node->textFile, fText, strlen(fText));
	node->FileSize = fSize;
}

FileList* search_node(FileList* head, char* filename) {
	
	if(head == NULL) return NULL;
	
	if(strncmp(filename, head->nameFile, MAX_SIZE) == 0){
		return head;
	}
	
	FileList* found = NULL;
	search_node(head->next, filename);
	
	return found;
}

/*
long pop_tail(node* head) {
	long return_fd;
	if(head->next == NULL) {
		return_fd = head->client_fd;
		free(head);
		return return_fd;
	}

	node* ptr = head;
	while(ptr->next->next != NULL) {
		ptr = ptr->next;
	}
	return_fd = ptr->next->client_fd;
	free(ptr->next);
	ptr->next = NULL;
	return return_fd;
}
*/

#endif /* FILELIST_H */