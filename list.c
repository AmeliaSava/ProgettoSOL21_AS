#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ops.h>

/*typedef struct LIST {
	node* head;
	node* tail;
} linked_list;*/

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
void print_list(node* head){
	node* ptr = head;
	while(ptr->next != NULL) {
		printf("%ld -> ", ptr->client_fd);
		ptr = ptr->next;
	}
	printf("NULL\n");
}

int main(){
	node* list = NULL;
	push_head(&list, 0);
	push_head(&list, 1);
	push_head(&list, 2);
	push_head(&list, 3);
	print_list(list);
	pop_tail(list);
	printf("\n");
	print_list(list);
}