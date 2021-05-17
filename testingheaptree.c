#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _nodo{
	long tmpfd;
	int freq;
	char nome[2048];
	int stato;
	struct _nodo *left;
	struct _nodo *right;
} nodo;

nodo* findnode(nodo* t, char* str){
	if(t == NULL){
		return NULL;
	}
	
	if(strcmp(str, t->nome) == 0){
		return t;
	}
	
	findnode(t->left, str);
	findnode(t->right, str);
} 

nodo* createTree(long tmpfd, int freq, char* nome, int stato){
	
	nodo* new = (nodo*) malloc(sizeof(nodo));
	new->tmpfd = tmpfd;
	strcpy(new->nome, nome);
	new->freq = freq;
	new->stato = stato;
	new->left = NULL;
	new->right = NULL;
	return new;

}

void swapNode (nodo** a, nodo** b) {
	long tmpFD = (*a)->tmpfd;
	int tmpF = (*a)->freq;
	char tmpN[2048];
	strcpy(tmpN, );
	int stato;
}

nodo* minNode (nodo* n1, nodo* n2, nodo* n3) {
    fprintf(stderr, "dentro min");
	int f1 = n1->freq;
	int f2 = n2->freq;
	int f3 = n3->freq;
	if(f1 <= f2 && f1 <= f3)
			return n1;
	if(f2 <= f1 && f2 <= f3)
			return n2;
	if(f3 <= f1 && f3 <= f2)
			return n3;
}

void heapify (nodo* root) {
	
	if(root == NULL)
		return;
	fprintf(stderr, "dentro heapify");
	nodo* min = NULL;
	min = minNode(root, root->left, root->right);
	fprintf(stderr, "fuori min heapify");
	if (min->freq == root->freq) {
		fprintf(stderr, "dentro if heapify");
		return;
	}
	else {
		fprintf(stderr, "dentro else heapify");
		swapNode(&root, &min);
		heapify(min);
	}
}

nodo* lastonleft(nodo* root) {
    if(root->left == NULL) {
        return root;
    } 
    
    lastonleft(root);
}

void LFU_Remove(nodo* root)  
{  
	nodo* LastLeft = lastonleft(root);
	printf("%d", LastLeft->freq);
    swapNode(&root, &LastLeft);
	free(LastLeft);
	//heapify(root);
}

nodo* insTree(nodo *t, int key, char* nome){
	if (t == NULL) {
		nodo* new = (nodo *) malloc(sizeof(nodo));
		strcpy(new->nome, nome);
		new->freq=key;
		new->left=NULL;
		new->right=NULL;
		return new;
	}
	if(t->freq < key) t->right = insTree(t->right, key, nome);
	else t->left = insTree(t->left, key, nome);
	return t;
}

void increaseF (nodo* n) {
	n->freq = n->freq + 2;
	heapify(n);
}

void print (nodo* n){
    if(n==NULL) return;
	printf("%d\n", n->freq);
	print(n->left);
	print(n->right);	
}

int main(){
	int k;
	int n;
	int d;
	char* nome = malloc(2048*sizeof(char));
	nodo* root = NULL;
	
	printf("Numero di nodi:");
	scanf("%d", &n);
	printf("Radice:");
	scanf("%d", &k);
	scanf("%s", nome);
	root = createTree(k, nome);
	
	for (int i = 1; i < n; i++){
		printf("Inserisci nodo:");
		scanf("%d", &k);
		scanf("%s", nome);
		insTree(root, k, nome);
	}
	
	printf("Radice: %d\n", root->freq);
	printf("stampa albero:\n");
	print(root);
//	printf("heapify:");
//	heapify(root);
//	print(root);
    printf("swap:\n");
    swapNode(&root,&(root->left));
    print(root);
	printf("Rimuvi min:");
	LFU_Remove(root);
	print(root);
	printf("incremento radice:");
	increaseF(root);
	print(root);
	printf("heapify:");
	heapify(root);
	print(root);
 return 0;
}


