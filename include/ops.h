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
#include <errno.h>

#define CHECK_EQ_EXIT(X, val, str)	\
  if ((X)==val) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
  }
#define CHECK_NEQ_EXIT(X, val, str)	\
  if ((X)!=val) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
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

#endif /*_CHECK_H*/