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
 * tipo del messaggio
 */
typedef struct MSG {
	int namelenght;
	long size;
	op op_type; 
	char filename[MAX_SIZE];
	char filecontents[MAX_SIZE];
	pid_t pid;
	long fd_con;
	struct MSG* next;
} msg;

typedef struct MSG_LIST
{
	msg* head;
	msg* last;
	size_t size;

} MSGlist;


static inline void msg_list_init(MSGlist* list) 
{
	list->head = NULL;
	list->last = NULL;
	list->size = 0;
}

static inline msg* msg_list_pop_return(MSGlist* list)
{
	if(list->head == NULL)	return NULL;

	msg* current = list->head;
	msg* toReturn;

	if(current->next == NULL) {
		toReturn = current;
		free(current);
		list->head = NULL;
		return toReturn;
	}

	while (current->next->next != NULL)
	{
		current = current->next;
	}

	toReturn = current->next;
	free(current->next);
	current->next = NULL;
	list->last = current;

	return toReturn;
	
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
