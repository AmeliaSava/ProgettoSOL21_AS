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
 * 
 */
static inline void msg_list_init(MSGlist* list) 
{
	list->head = NULL;
	list->last = NULL;
	list->size = 0;
}

static inline void msg_list_pop_return(MSGlist* list, msg** toReturn)
{
	if(list->head == NULL)	return;

	msg* current = list->head;
	//msg* toReturn = safe_malloc(sizeof(msg));
	
	if(current->next == NULL) {

		msgcpy(*toReturn, current);
		//fprintf(stderr, "toreturn:%s\n", toReturn->filename);
		free(current);
		list->head = NULL;
		list->size = list->size - 1;
		//fprintf(stderr, "toreturn2:%s\n", toReturn->filename);
		return;
	}

	while (current->next->next != NULL)
	{
		//fprintf(stderr, "current:%s\n", current->filename);
		current = current->next;
	}

	msgcpy(*toReturn, current->next);
	free(current->next);
	current->next = NULL;
	list->last = current;
	list->size = list->size - 1;

	return;
	
}

static inline void msg_push_head(msg* head, MSGlist* list) {

	if(list->size == 0) 
	{
		list->head = head;
		list->last = head;
	} 
	else 
	{
		head->next = list->head;
		list->head = head;
	}

	list->size++;
	return;
}

static inline void msg_list_destroy(MSGlist* list)
{
	msg* current = list->head;
	msg* next;

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
