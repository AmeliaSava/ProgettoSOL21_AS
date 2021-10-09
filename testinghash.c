#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <HashLFU.h>
#define INT_MAX 2147483647
#define MAX_SIZE 2048

void print_list (FileList* n){

	if(n==NULL) {
		fprintf(stdout, "NULL\n");
		return;
	}
	
	printf("Frequenza: %d\n", n->frequency);
	printf("Nome: %s\n", n->nameFile);
	printf("Testo: %s\n", n->textFile);
	printf("Stato: %d\n", n->status);
	printf("\n");

	print_list(n->next);	
}

void print_tab (Table* tab)
{
	for(size_t i = 0; i < tab->maxSize; i++)
	{
		printf("Printing hash bucket[%zu]", i);
		print_list(&(tab->bucket[i]));
	}
}

int main(){
	
	Table* hashMap = NULL;
	Hash_Init(hashMap, 10);

	Hash_Insert(hashMap, 0, "paswfil", "01000000110000101101100011011000110111", 0, 22);
	Hash_Insert(hashMap, 0, "docfil", "10111011101110011", 0, 34);
	Hash_Insert(hashMap, 0, "vid", "111011101110011001", 0, 22);
	Hash_Insert(hashMap, 0, "applptxt", "0011001000110010101100110011010010", 0, 34);
	Hash_Insert(hashMap, 0, "prjup", "1110101011100000111000", 0, 25);
	Hash_Insert(hashMap, 0, "docvimk", "01110011001000000110000101", 0, 12);
	
	printf("\n");
	printf("Printing:\n");
	print_tab(hashMap);
	printf("\n");
	printf("\n");
	/*
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
	*/
	
 return 0;
}
