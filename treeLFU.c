#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <treeLFU.h>


//returns:
//success: a newly allocated node
//failure: stops if malloc fails
NodeFile* createNode(int frq, char* fName, char* fText, int fStat, long fSize) {

	NodeFile* newNode = (NodeFile*) malloc(sizeof(NodeFile));
	CHECK_EQ_EXIT(newNode, NULL, ERROR: malloc);

	newNode->frequency = frq;
	newNode->status = fStat;
	newNode->FileSize = fSize;

	if ((newNode->textFile = malloc(strlen(fText)*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(fName); exit(EXIT_FAILURE);}
	strncpy(newNode->textFile, fText, strlen(fText));
	if ((newNode->nameFile = malloc(strlen(fName)*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(fName); exit(EXIT_FAILURE);}
	strncpy(newNode->nameFile, fName, strlen(fName));	

	newNode->left = NULL;
	newNode->right = NULL;
	return newNode;
}

//returns:
//success: the tree updated with the new node
//failure: stops if malloc fails in createNode
NodeFile* PushNode(NodeFile *root, int frq, char* fName, char* fText, int fStat, long fSize) {

	if (root == NULL) {
		return createNode(frq, fName, fText, fStat, fSize);
	}

	if (strlen(root->nameFile) >= strlen(fName))
		root->left = PushNode(root->left, frq, fName, fText, fStat, fSize);
		else root->right = PushNode(root->right, frq, fName, fText, fStat, fSize);

	return root;
}

void UpdateNode(NodeFile* newNode, int frq, char* fName, char* fText, long fSize) {
	newNode->frequency = frq;
	strncpy(newNode->nameFile, fName, MAX_SIZE);
	strncpy(newNode->textFile, fText, fSize);
	newNode->FileSize = fSize;
}

NodeFile* searchNode(NodeFile* root, char* filename) {
	
	if(root == NULL) return NULL;
	
	if(strncmp(filename, root->nameFile, MAX_SIZE) == 0){
		return root;
	}
	
	NodeFile* found = NULL;
	if (strlen(root->nameFile) >= strlen(filename)) found = searchNode(root->left, filename);
	if (strlen(root->nameFile) < strlen(filename)) found = searchNode(root->right, filename);
	
	return found;
}

//returns: nothing
//effect: the contents of the nodes are swapped
void swapNode (NodeFile* file1, NodeFile* file2) {
	//NULL CONTROL!!
	NodeFile* tmp = createNode (file1->frequency, file1->nameFile, file1->textFile, file1->status, file1->FileSize);
		fprintf(stderr, "dentro swap");
	file1->frequency = file2->frequency;
	strcpy(file1->nameFile, file2->nameFile);
	strcpy(file1->textFile, file2->textFile);
	file1->status = file2->status;
	file1->FileSize = file2->FileSize;

	file2->frequency = tmp->frequency;
	strcpy(file2->nameFile, tmp->nameFile);
	strcpy(file2->textFile, tmp->textFile);
	file2->status = tmp->status;
	file2->FileSize = tmp->FileSize;

	free(tmp);

}

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

//FUNZIONA
void increaseF (NodeFile* file) {
	file->frequency = file->frequency + 1;
}

//FUNZIONA
void UpdateStat (NodeFile* file) {
	if(file->status == 1) file->status = 0;
	if(file->status == 0) file->status = 1;
}

