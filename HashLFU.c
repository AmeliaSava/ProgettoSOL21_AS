#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <filelist.h>
#include <HashLFU.h>


void Hash_Init(Table* tab, int size)
{
	tab->bucket = malloc(sizeof(FileList)*size);
	tab->maxSize = size;

	/*for(int i = 0; i < size; i++) 
		tab->bucket[i] = NULL;*/
}

/*
//returns: nothing
//effect: iterates over the tree, when a values smaller then the current min is found
//			it updates the external values of int minV and a pointer to the node that contains it
void minNode (NodeFile* t, NodeFile** min, int* minV) {

	if(t == NULL) return;

	if(*minV > (t->frequency)) {
		*minV = t->frequency;
		*min = t;
	}
	
	minNode(t->left, min, minV);
	minNode(t->right, min, minV);
	return;
}

//returns:
//success: a node detached from the tree with the data of one of the tree leaves
//failure: NULL
//effect: asserts if the parent node has a leaf child, swaps the contents of the
//			 leaf in a new node unrelated to the tree and returns it, if the node
//			is not a leaf 
NodeFile* isLeaf(NodeFile* parent) {
	fprintf(stderr, "dentro isleaf\n");
	if(parent->left == NULL && parent->right == NULL) {
		fprintf(stderr, "èfoglia\n");
			NodeFile* temp = NULL;
			temp = PushNode(temp, parent->frequency, parent->nameFile, parent->textFile, parent->status, parent->FileSize);
			UpdateNode(temp, parent->frequency, parent->textFile, parent->FileSize);
			free(parent);
		    return temp;
	}

	return NULL; 
}

//FUNZIONA
NodeFile* pluckLeaf (NodeFile* root) {
	fprintf(stderr, "dentro pluck\n");

	if(root == NULL)
		return NULL;
	
	NodeFile* leaf = NULL;

	//nodo con due figli
	if(root->left != NULL && root->right != NULL){
		//c'è una foglia
		//a dx c'è una foglia
		if ((leaf = isLeaf(root->right)) != NULL) {
			root->right = NULL;
			return leaf;
		}
		//a sx c'è una foglia
		if ((leaf = isLeaf(root->left)) != NULL) {
			root->left = NULL;
			return leaf;
		}
		
		leaf = pluckLeaf(root->left);
	}
	//nodo con solo il figlio dx
	if(root->left == NULL && root->right != NULL){
		//a dx c'è una foglia
		if ((leaf = isLeaf(root->right)) != NULL) {
			root->right = NULL;
			return leaf;
		}

		leaf = pluckLeaf(root->right);
	}
	//nodo con solo il figlio sx
	if(root->left != NULL && root->right == NULL){
		//a sx c'è una foglia
		if ((leaf = isLeaf(root->left)) != NULL) {
			root->left = NULL;
			return leaf;
		}

		leaf = pluckLeaf(root->left);
	}
	return leaf;
}

//RIMUOVE UN FILE CHE NON SIA MINIMO
//FUNZIONA
void LFU_Remove(NodeFile* root, long* rem) {
	fprintf(stderr, "dentro lfu\n");
	if(root == NULL) return;
	if(root->left == NULL && root->right == NULL) {
		free(root);
		return;
	}

	int minValue = root->frequency;
	NodeFile* min = root;
	minNode(root, &min, &minValue);
	fprintf(stderr, "%d\n", minValue);
	fprintf(stderr, "%ld\n", min->FileSize);
	*rem = min->FileSize;
	NodeFile* leaf = NULL;
	CHECK_EQ_EXIT(leaf = pluckLeaf(root), NULL, "ERROR: deleting leaf");
	swapNode(min, leaf);
	free(leaf);
}

void RemoveFile(NodeFile* toDel, NodeFile* tree, char* Vfile) {
	fprintf(stderr, "dentro remove nodo\n");
	if(toDel == NULL) return;
	
	NodeFile* leaf = NULL;
	CHECK_EQ_EXIT(leaf = pluckLeaf(tree), NULL, "ERROR: deleting leaf");

	swapNode(toDel, leaf);
	free(leaf);
}

void AppendNode(NodeFile* node, int freq, char* append, long newSize) {
	fprintf(stderr, "append node in tree" );

	node->frequency = freq;
	if ((node->textFile = (char*)realloc((node->textFile), ((node->FileSize)+newSize))) == NULL) { perror("ERROR: realloc AppendNode"); free(node->textFile); exit(EXIT_FAILURE);}
	strncat(node->textFile, append, strlen(append));
	node->FileSize += newSize;
}
//FUNZIONA
void increaseF (NodeFile* file) {
	file->frequency = file->frequency + 1;
}
*/

