#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ops.h>

typedef struct NODE {
	struct NODE* next;
	long client_fd;
} node;

void push_head(node** head, long fd) {
	node* newNode = (node*) malloc(sizeof(node));
	if(newNode==NULL){exit(EXIT_FAILURE);}
	newNode->client_fd = fd;
	newNode->next = *head;
	*head = newNode;
}

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