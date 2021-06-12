#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>

#define MAX_INT 2147483647

typedef struct FILE_NODE {
	int frequency;
	char* textFile;
	char* nameFile;
	int status;
	long FileSize;
	struct FILE_NODE *left;
	struct FILE_NODE *right;
} NodeFile;

//returns:
//success: a newly allocated node
//failure: stops if malloc fails
NodeFile* createNode(int frq, char* fName, char* fText, int fStat, long fSize);
//returns:
//success: the tree updated with the new node
//failure: stops if malloc fails in createNode
NodeFile* PushNode(NodeFile *root, int frq, char* fName, char* fText, int fStat, long fSize);

void UpdateNode(NodeFile *newNode, int frq, char* fName, char* fText, long fSize);
//returns:
//success: the files node
//failure: NULL if the file is not present or there's an error
NodeFile* searchNode(NodeFile* root, char* filename);

//returns: nothing
//effect: the contents of the nodes are swapped
void swapNode (NodeFile* file1, NodeFile* file2);

//returns: nothing
//effect: iterates over the tree, when a values smaller then the current min is found
//			it updates the external values of int minV and a pointer to the node that contains it
void minNode (NodeFile* t, NodeFile** min, int* minV);

//returns:
//success: a node detached from the tree with the data of one of the tree leaves
//failure: NULL
//effect: asserts if the parent node has a leaf child, swaps the contents of the
//			 leaf in a new node unrelated to the tree and returns it, if the node
//			is not a leaf 
NodeFile* isLeaf(NodeFile* parent);
//FUNZIONA
NodeFile* pluckLeaf (NodeFile* root);

//RIMUOVE UN FILE CHE NON SIA MINIMO
//FUNZIONA
void LFU_Remove(NodeFile* root, long* rem);
void RemoveFile(NodeFile* toDel, NodeFile* tree, char* Vfile);

//FUNZIONA
void increaseF (NodeFile* file);
//FUNZIONA
void UpdateStat (NodeFile* file);

