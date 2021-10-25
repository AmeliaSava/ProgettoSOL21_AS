#if !defined(_OPS_H)
#define _OPS_H

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#if !defined(BUFSIZE)
#define BUFSIZE 256
#endif

#if !defined(EXTRA_LEN_PRINT_ERROR)
#define EXTRA_LEN_PRINT_ERROR   512
#endif

#define MAX_SIZE 2048

#define SYSCALL_EXIT(name, r, sc, str, ...) \
    if ((r=sc) == -1) {                     \
      perror(#name);                        \
      int errno_copy = errno;               \
      print_error(str, __VA_ARGS__);    \
      exit(errno_copy);     \
    }

#define SYSCALL_PRINT(name, r, sc, str, ...)  \
    if ((r=sc) == -1) {       \
  perror(#name);        \
  int errno_copy = errno;     \
  print_error(str, __VA_ARGS__);    \
  errno = errno_copy;     \
    }

#define SYSCALL_RETURN(name, r, sc, str, ...) \
    if ((r=sc) == -1) {       \
  perror(#name);        \
  int errno_copy = errno;     \
  print_error(str, __VA_ARGS__);    \
  errno = errno_copy;     \
  return r;                               \
    }

#define CHECK_EQ_EXIT(X, val, str)  \
  if ((X)==val) {                  \
    perror(#str);                   \
    free(X);                        \
    exit(EXIT_FAILURE);             \
  }

#define CHECK_NEQ_EXIT(X, val, str)	\
  if ((X)!=val) {			\
    perror(#str);			\
    free(X);                        \
    exit(EXIT_FAILURE);			\
  }

typedef enum {
  //server ops
  OPEN_FILE = 0,
  CLOSE_FILE = 1,
  READ_FILE = 2,
  READ_FILE_N = 3,
  WRITE_FILE = 4,
  APPEND_FILE = 5,
  REMOVE_FILE = 6,
  LOCK_FILE = 14,
  UNLOCK_FILE = 15, //ATTENTION fix numbers
  //server reply
  SRV_OK = 7,
  SRV_NOK = 8,
  SRV_FILE_NOT_FOUND = 9,
  SRV_FILE_ALREADY_PRESENT = 10,
  SRV_MEM_FULL = 11,
  SRV_READY_FOR_WRITE = 12,
  SRV_FILE_CLOSED = 13,
  SRV_FILE_LOCKED = 16,
  //client messages
  END_COMMUNICATION = 20

} op;

/**
 * \brief Procedura di utilita' per la stampa degli errori
 *
 */
static inline void print_error(const char * str, ...) {
    const char err[]="ERROR: ";
    va_list argp;
    char * p=(char *)malloc(strlen(str)+strlen(err)+EXTRA_LEN_PRINT_ERROR);
    if (!p) {
	perror("malloc");
        fprintf(stderr,"FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p,err);
    strcpy(p+strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    free(p);
}

static inline int isNumber(const char* s, long* n) {
  if (s==NULL) return 1;
  if (strlen(s)==0) return 1;
  char* e = NULL;
  errno=0;
  long val = strtol(s, &e, 10);
  if (errno == ERANGE) return 2;    // overflow/underflow
  if (e != NULL && *e == (char)0) {
    *n = val;
    return 0;   // successo 
  }
  return 1;   // non e' un numero
}

static inline void* safe_malloc (size_t size)
{
  void* alloc = malloc(size);
  CHECK_EQ_EXIT(alloc, NULL, ERROR: malloc);
  return alloc;
}


#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { \
    fprintf(stderr, "ERRORE FATALE lock\n");        \
    pthread_exit((void*)EXIT_FAILURE);          \
  }   
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { \
  fprintf(stderr, "ERRORE FATALE unlock\n");        \
  pthread_exit((void*)EXIT_FAILURE);            \
  }
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
    fprintf(stderr, "ERRORE FATALE wait\n");        \
    pthread_exit((void*)EXIT_FAILURE);            \
}
/* ATTENZIONE: t e' un tempo assoluto! */
#define TWAIT(c,l,t) {              \
    int r=0;                \
    if ((r=pthread_cond_timedwait(c,l,t))!=0 && r!=ETIMEDOUT) {   \
      fprintf(stderr, "ERRORE FATALE timed wait\n");      \
      pthread_exit((void*)EXIT_FAILURE);          \
    }                 \
  }
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       { \
    fprintf(stderr, "ERRORE FATALE signal\n");      \
    pthread_exit((void*)EXIT_FAILURE);          \
  }
#define BCAST(c)     if (pthread_cond_broadcast(c)!=0)    {   \
    fprintf(stderr, "ERRORE FATALE broadcast\n");     \
    pthread_exit((void*)EXIT_FAILURE);            \
  }
static inline int TRYLOCK(pthread_mutex_t* l) {
  int r=0;    
  if ((r=pthread_mutex_trylock(l))!=0 && r!=EBUSY) {        
    fprintf(stderr, "ERRORE FATALE unlock\n");        
    pthread_exit((void*)EXIT_FAILURE);          
  }                   
  return r; 
}

static inline void print_op(op op_type)
{	
	switch(op_type)
	{
		case SRV_OK:
		{
			printf("Operation completed successfully\n");
			break;
		}
		
		case SRV_NOK: 
    {
      printf("Error while executing operation\n");
      break;
		}

		case SRV_FILE_NOT_FOUND:
    {
			printf("File not found\n");
      break;
		}

		case SRV_FILE_ALREADY_PRESENT: 
    {
			printf("File already present\n");
      break;
		}

		case SRV_MEM_FULL: 
    {
      printf("File too big for memory\n");
      break;
		}
		
		case SRV_FILE_CLOSED:
    {
			printf("File is closed\n");
      break;
		}

    case SRV_FILE_LOCKED:
    {
      printf("File is locked");
    }
		
		default: {
			fprintf(stderr, "command not found\n");
			break;
		}
	}
}

#endif /*_CHECK_H*/