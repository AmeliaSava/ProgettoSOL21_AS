#if !defined(HASHLFU_H)
#define HASHLFU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <filelist.h>

#define MAX_INT 2147483647

/**
 * \brief: an hash table to use as the server memory
 * \param bucket: an array of lists 
 * \param maxSize: the table max size
 * \param curSize: the current size
 */
typedef struct HashLFU
{
	FileList* bucket;
	int maxSize;
	int curSize;

} Table;

/**
 * \brief: initializes the has table
 * \param tab: the table that will be initialized
 * \param size: the max size the table will have
 */
void Hash_Init(Table* tab, int size);

/**
 * \brief: hash function, uses the name as key
 * \param t: the table
 * \param key: key value
 */
size_t Hash_Function(Table* t, char* key);

/**
 * \brief: inserts a value into the table
 * \param t: the table
 * \param frq: the frequency
 * \param fName: the files name
 * \param fStat: the status of the node, 0 is open, 1 is closed 
 */
void Hash_Insert(Table* t, int frq, char* fName, int fStat);

/**
 * \brief: searches for a node and returns a pointer to it
 * \param tab: the table where the node must be searched
 * \param fName: the name of the file to search
 */
FileNode* Hash_SearchNode(Table* tab, char* fName);

/**
 * \brief: searches the lists tail to faind the min value, returns the deleted element
 * \param tab: the table
 */
FileNode* Hash_LFUremove (Table* tab);

/**
 * \brief:remove a node from the table
 * \param tab: the table
 * \param Vfile: the name of the file that is to be deleted
 */
void Hash_Remove(Table* tab, char* Vfile);

/**
 * \brief: increses the node frequency and moves it to the to of its bucket list, so it won't be caught by the LFURemove
 * \param tab: the table
 * \param node: the node that needs to be increased
 */
void Hash_Inc(Table* tab, FileNode* node);

/**
 * \brief: reads n files from a table and saves the into a list
 * \param tab: the table
 * \param n: how many files will be read, if <= 0 the all the files are read
 * \param to_send: pointer to a list where the read files will be saved
 * \param tot: a pointer to a number that tells actually how many files were read
 */
void Hash_Read(Table* tab, int n, FileNode** to_send, int* tot);

/**
 * \brief: prints the table
 */
void Hash_Print (Table* tab);

/**
 * \brief: destroys the table
 */
void Hash_Destroy(Table* t);

#endif /* HASHLFU_H */
