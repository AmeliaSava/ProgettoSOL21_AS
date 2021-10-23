#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ops.h>
#include <filelist.h>



int main(){
	
	FileList* list = safe_malloc(sizeof(FileList));
	list_init(list);
	node_push(list, 0, "paswfil", "01000000110000101101100011011000110111", 0, 22);
	node_push(list, 0, "docfil", "10111011101110011", 0, 34);
	node_push(list, 0, "vid", "111011101110011001", 0, 22);
	node_push(list, 0, "applptxt", "0011001000110010101100110011010010", 0, 34);
	node_push(list, 0, "prjup", "qui ci devo fare un", 0, 25);
	node_push(list, 0, "docvimk", "01110011001000000110000101", 0, 12);
	printf("Regular List:\n");
	printf("\n");
	print_list(list->head);
	
	//FileNode* found = malloc(sizeof(FileNode));
	//found = node_search(list->head, "prjup");
	//printf("Nome: %s\n", found->nameFile);
	//node_update(found, 0, "qui ci devo fare un", 20);
	//printf("Testo: %s\n", found->textFile);
	//append_node(found, (found->frequency)++, " append!!", ((found->FileSize) + sizeof(" append!!")));
	//printf("Append_Test\n");
	//printf("Nome: %s\n", found->nameFile);
	//printf("Testo: %s\n", found->textFile);
	//printf("Size: %zu\n", found->FileSize);
	//printf("Freq: %d\n", found->frequency);
	//node_incrfreq(found);
	//printf("Freq_Test\n");
	//printf("Freq: %d\n", found->frequency);
	node_movetofront(list, "paswfil", strlen("vid"));
	printf("Moving paswfil:\n");
	printf("\n");
	print_list(list->head);
	node_movetofront(list, "vid", strlen("vid"));
	printf("Moving vid:\n");
	printf("\n");
	print_list(list->head);
	node_movetofront(list, "docvimk", strlen("docvimk"));
	//list_pop(list);
	printf("Moving docvimk:\n");
	printf("\n");
	print_list(list->head);
	list_destroy(list);
	//print_list(list->head);
	
	return 0;
}
