#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <ops.h>
#include <coms.h>
#include <conn.h>
#include <message.h>



int main(int argc, char *argv[]) {
	/*
	int opt;

	 while((opt = getopt(argc, argv, “hfwWrEdtcp:”)) != -1) 
    { 
        switch(opt) { 
            case ‘h’:
            break;
            case ‘l’: 
            case ‘r’: 
                printf(“option: %c\n”, opt); 
                break; 
            case ‘f’: 
                printf(“filename: %s\n”, optarg); 
                break; 
            case ‘:’: 
                printf(“option needs a value\n”); 
                break; 
            case ‘?’: 
                printf(“unknown option: %c\n”, optopt);
                break; 
        } 
    } 
	*/

	openConnection(SOCKNAME);

	const char* nome = "./storage/imm1.jpg";
	const char* nome1 = "./storage/es8.tgz";
	const char* nome2 = "./storage/manuale-unix.pdf";
	//const char* nome3 = "./storage/it is a mystery.mp3";
	

	if(atoi(argv[1])==0) {
		openFile(nome, 0);
		openFile(nome1, 0);
		openFile(nome2, 0);
	} 
	else {
		void* buf = NULL;
		size_t sz;
		readFile(nome, &buf, &sz);
		printf("%p\n", buf); // qualcosa non va nel puntatore
		void* buf2 = NULL;
		size_t sz2;
		readFile(nome1, &buf2, &sz2);
		printf("%p\n", buf); // qualcosa non va nel puntatore*/
		//openFile(nome3, 0);
	}

	closeConnection(SOCKNAME);

	return 0;
}
