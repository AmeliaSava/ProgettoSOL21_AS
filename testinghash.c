#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <HashLFU.h>
#define INT_MAX 2147483647
#define MAX_SIZE 2048
/*
//FUNZIONA
void print (NodeFile* n){

	if(n==NULL) {
		fprintf(stdout, "NULL\n");
		return;
	}
	
	printf("Frequenza: %d\n", n->frequency);
	printf("Nome: %s\n", n->nameFile);
	printf("Testo: %s\n", n->textFile);
	printf("Stato: %d\n", n->status);
	printf("\n");

	print(n->left);
	print(n->right);	
}

int main(){
	
	NodeFile* root = NULL;
	root = PushNode(root, INT_MAX, "median", "fixed_root", 0);
	PushNode(root, 0, "paswfil", "01000000110000101101100011011000110111", 0);
	PushNode(root, 0, "docfil", "10111011101110011", 0);
	PushNode(root, 0, "vid", "111011101110011001", 0);
	PushNode(root, 0, "applptxt", "0011001000110010101100110011010010", 0);
	PushNode(root, 0, "prjup", "1110101011100000111000", 0);
	PushNode(root, 0, "docvimk", "01110011001000000110000101", 0);
	
	printf("\n");
	printf("Printing Tree:\n");
	print(root);
	printf("\n");
	printf("\n");
	printf("Search:\n");
    NodeFile* find = NULL;
    find = searchNode(root, "vid");
    printf("Trovato '%s': %d\n", find->nameFile, find->frequency);
	printf("\n");
	printf("\n");
	int minValue = root->frequency;
	NodeFile* min = root;
	minNode(root, &min, &minValue);
	printf("Min '%s': %d\n", min->nameFile, min->frequency);
	printf("\n");
	printf("\n");
	printf("Remove min:\n");
	LFU_Remove(root);
	print(root);
	printf("\n");
	printf("\n");
	printf("Remove file:\n");
	RemoveFile(root, "prjup");
	print(root);
	
 return 0;
}


*/