#if !defined(HASHLFU_H)
#define HASHLFU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <filelist.h>

#define MAX_INT 2147483647

typedef struct HashLFU
{
	FileList* bucket;
	int maxSize;

} Table;

void Hash_Init(Table* tab, int size);

size_t Hash_Function(Table* t, char* key);

void Hash_Insert(Table* t, int frq, char* fName, char* fText, int fStat, long fSize);

FileNode* Hash_Search(Table* tab, char* fName);

void Hash_Destroy(Table* t);

#endif /* HASHLFU_H */
