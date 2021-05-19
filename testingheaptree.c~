#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>

#define INT_MAX 2147483647
#define MAX_SIZE 2048

typedef struct _nodo{
	int freq; //chiave primaria
	char nome[MAX_SIZE];
	char testo[MAX_SIZE];
	int stato;
	struct _nodo *left;
	struct _nodo *right;
} nodo;

//FUNZIONA
void print (nodo* n){

	if(n==NULL) {
		fprintf(stdout, "fine\n");
		return;
	}
	
	printf("Frequenza: %d\n", n->freq);
	//printf("Nome: %s\n", n->nome);
	//printf("Testo: %s\n", n->testo);
	//printf("Stato: %d\n", n->stato);
	printf("\n");
	print(n->left);
	print(n->right);	
}

//FUNZIONA
nodo* NewNode(int freq, char* nome, char* testo, int stato){
	
	nodo* new = (nodo*) malloc(sizeof(nodo));
	new->freq = freq;
	strcpy(new->nome, nome);
	strcpy(new->testo, testo);
	new->stato = stato;
	new->left = NULL;
	new->right = NULL;
	return new;

}

//FUNZIONA
nodo* insTree(nodo *t, int freq, char* nome, char* testo, int stato) {

	if (t == NULL) {
		return NewNode(freq, nome, testo, stato);
	}
	if (strlen(t->nome) >= strlen(nome))
		t->left = insTree(t->left, freq, nome, testo, stato);
    	else t->right = insTree(t->right, freq, nome, testo, stato);
  
	return t;
}

//FUNZIONA
nodo* findNode(nodo* t, char* str){
	
	if(t == NULL){
		return NULL;
	}
	
	if(strncmp(str, t->nome, MAX_SIZE) == 0){
		return t;
	}
	
	nodo* found = NULL;
	if (strlen(t->nome) >= strlen(str)) found = findNode(t->left, str);
	if (strlen(t->nome) < strlen(str)) found = findNode(t->right, str);
	
	return found;
}

//FUNZIONA
void swapNode (nodo* a, nodo* b) {

	nodo* tmp = NewNode (a->freq, a->nome, a->testo, a->stato);
	
	a->freq = b->freq;
	strcpy(a->nome, b->nome);
	strcpy(a->testo, b->testo);
	a->stato = b->stato;

	b->freq = tmp->freq;
	strcpy(b->nome, tmp->nome);
	strcpy(b->testo, tmp->testo);
	b->stato = tmp->stato;

	free(tmp);

}

//FUNZIONA
void minNode (nodo* t, nodo** min, int* minV) {

	if(t == NULL) return;

	if(*minV > (t->freq)) {
		*minV = t->freq;
		*min = t;
	}
		
	minNode(t->left, min, minV);
	minNode(t->right, min, minV);
	return;
}

//FUNZIONA
nodo* pluckLeaf (nodo* root) {

	if(root == NULL)
		return NULL;
	
	nodo* leaf = NULL;

	//nodo con due figli
	if(root->left != NULL && root->right != NULL){
		leaf = pluckLeaf(root->left);
	} 
	//nodo con solo il figlio dx
	if(root->left == NULL && root->right != NULL){
		//a dx c'è una foglia
		if(root->right->left == NULL && root->right->right == NULL) {
			nodo* temp = (nodo*) malloc(sizeof(nodo));
			swapNode(temp, root->right);
		    	root->right = NULL;
			return temp;
		}
	 	leaf = pluckLeaf(root->right);
	}
	//nodo con solo il figlio sx
	if(root->left != NULL && root->right == NULL){
		//a sx c'è una foglia
		if(root->left->left == NULL && root->left->right == NULL) {
			nodo* temp = (nodo*) malloc(sizeof(nodo));
			swapNode(temp, root->left);
		    	root->left = NULL;
			return temp;
		}
 		leaf = pluckLeaf(root->left);
	}
	return leaf;
}

//FUNZIONA
void LFU_Remove(nodo* root) {
	
	int minValue = root->freq;
	nodo* min = NULL;
	minNode(root, &min, &minValue);

	nodo* leaf = NULL;
	CHECK_EQ_EXIT(leaf = pluckLeaf(root), NULL, "ERROR: deleting leaf");

	swapNode(min, leaf);

	free(leaf);
}

//FUNZIONA
void increaseF (nodo* n) {
	n->freq = n->freq + 1;
}

//FUNZIONA
void UpdateStat (nodo* n) {
	if(n->stato == 1) n->stato = 0;
	if(n->stato == 0) n->stato = 1;
}
int main(){
	
	nodo* root = NULL;
	/*	
	root = insTree(root, 0, "median", "terzo", 0);
	insTree(root, 1, "paswfil", "uno", 0);
	insTree(root, 5, "docfil", "cinque", 0);
	insTree(root, 2, "vid", "due", 0);
	insTree(root, 2, "loolp", "due", 0);
	insTree(root, 4, "prjup", "quattro", 0);
	insTree(root, 0, "docvimk", "zero", 0);
	*/
	root = insTree(root, 4, "abcd", "radice", 0);
    	insTree(root, 2, "ema", "stringa1", 0);
    	insTree(root, 3, "amel", "stringa2", 0);
   	insTree(root, 0, "lorenzo", "stringa3", 0);
    	insTree(root, 5, "federico", "stringa4", 0);
    	insTree(root, 6, "alessa", "stringa5", 0);
	/*printf("\n");
	printf("stampa albero:\n");
	printf("%s\n", root->nome);
	printf("%s\n", root->right->nome);
	print(root);
	printf("\n");
	*/
	printf("Ricerca:\n");
    	nodo* find = NULL;
   	printf("%s\n", root->left->right->nome);
    	printf("%s\n", root->left->right->testo);
    	find = findNode(root, "amel");
    	printf("Trovato '%s': %d\n", find->nome, find->freq);

    	printf("---------\n");
	//nodo* left = NULL;
	//left = pluckLeaf(root);
	//printf("Last SX '%s': %d\n", left->nome, left->freq);
	printf("\n");
	int minValue = root->freq;
	nodo* min = NULL;
	minNode(root, &min, &minValue);
	printf("Min '%s': %d\n", min->nome, min->freq);
	printf("---------\n");
	printf("stampa albero:\n");
	printf("%s\n", root->nome);
	printf("%s\n", root->right->nome);
	print(root);
	printf("---------\n");
	printf("Rimuvi min:\n");
	LFU_Remove(root);
	print(root);
	
 return 0;
}


