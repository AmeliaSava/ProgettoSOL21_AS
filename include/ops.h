#if !defined(_CHECK_H)
#define _CHECK_H

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

char* malloc_check (char* string, size_t sz) {
	if((string = malloc(sz * sizeof(char))) == NULL) {
		perror("malloc socket name");
		free(string);
		exit(EXIT_FAILURE);
	} else 
		return string;
}

#endif /*_CHECK_H*/