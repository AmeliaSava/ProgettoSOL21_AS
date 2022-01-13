#if !defined(CONN_H)
#define CONN_H

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXBACKLOG   32

/** 
 * \brief: a struct that defines the messages between client and server and acts as list node
 * \param namelenght: the lenght of the file's name
 * \param size: the file's size
 * \param op_type: defines the type of operation requested on the server
 * \param filename: a string with name of the file
 * \param filecontents: the byte array that rapresents files contents
 * \param pid: the process id of the client that requested the operation
 * \param fd_con: the fd where comunication with the client is happening
 * \param flag: flag needed for certain operations
 * \param next: pointer to the next msg
 */
typedef struct MSG {
	int namelenght;
	long size;
	op op_type; 
	char filename[MAX_SIZE];
	char filecontents[MAX_SIZE];
	pid_t pid;
	long fd_con;
	int flag;
	struct MSG* next;
} msg;

/**
 * \brief: struct for a list of messages
 * \param head: pointer to the head of the list
 * \param last: pointer to the last element
 * \param size: size of the list 
 */
typedef struct MSG_LIST
{
	msg* head;
	msg* last;
	size_t size;

} MSGlist;

/**
 * \brief: a fuction that copies a message into an other
 * \param destination: the destination message
 * \param source: the source message
 */
static inline void msgcpy(msg* destination, msg* source) 
{
	destination->namelenght = source->namelenght;
	destination->size = source->size;
	destination->op_type = source->op_type;

	memcpy(destination->filename, source->filename, strlen(source->filename));
	memcpy(destination->filecontents, source->filecontents, source->size);

	destination->pid = source->pid;
	destination->fd_con = source->fd_con;
	destination->flag = source->flag;

	destination->next = NULL;
}

/**
 * \brief: a function to initialize the list
 * \param list: the list that needs to be initialized
 */
static inline void msg_list_init(MSGlist* list) 
{
	list->head = NULL;
	list->last = NULL;
	list->size = 0;
}

/**
 * \brief: pops the last element of the lists and saves it in pointer to another msg
 * \param list: the list the element is taken from
 * \param toReturn: the pointer to the msg where the data will be saved
 */
static inline void msg_list_pop_return(MSGlist* list, msg** toReturn)
{
	//if the list is null return
	if(list->head == NULL)	return;

	msg* current = list->head;
	
	//the list has only one element
	if(current->next == NULL) {

		//copy the element in the return node
		msgcpy(*toReturn, current);
		//free the only element of the list
		free(current);

		//set the list back to null
		list->head = NULL;
		list->last = NULL;
		list->size = list->size - 1;
		return;
	}

	//cycle the list until the second-last
	while (current->next->next != NULL)
	{
		current = current->next;
	}

	//copying the last element
	msgcpy(*toReturn, current->next);
	//freeing it
	free(current->next);
	//updating the list
	current->next = NULL;
	list->last = current;
	list->size = list->size - 1;

	return;
	
}

/**
 * \brief: pushes a element on top of the list
 * \param head: the node to be pushed
 * \param list: the list where the node will be pushed
 */
static inline void msg_push_head(msg* head, MSGlist* list) {

	//empty list
	if(list->size == 0) 
	{
		//head is the only element
		list->head = head;
		list->last = head;
	} 
	else 
	{
		//list has at least one element
		//attach to the new node the old list
		head->next = list->head;
		//now new node is the head of the list
		list->head = head;
	}

	//increase size of list
	list->size++;
	return;
}

/**
 * \brief: function that destroys the list
 * \param list: the list to destroy
 */
static inline void msg_list_destroy(MSGlist* list)
{

	msg* current = list->head;
	msg* next;

	while (current != NULL)
	{
		//save the next
		next = current->next;
		//free current
		free(current);
		//move to next
		current = next;
	}

	//after all the elements have been destroyed empty list
	list->head = NULL;
	list->last = NULL;

	free(list);

	return;
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
    return 1;
}


#endif /* CONN_H */
