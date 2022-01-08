#if !defined(CLIENTLIST_H)
#define CLIENTLIST_H

#include <ops.h>

typedef struct NODE {
	struct NODE* next;
	long client_fd;
} node;

typedef struct FILE_LIST
{
	node* head;
	size_t size;

} list;

void init_list_client(list** list)
{
	list->head = NULL;
	list->size = 0;
}

void push_head_client(node** head, long fd) {
	node* newNode = safe_malloc(sizeof(node));
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

#endif /* CLIENTLIST_H */