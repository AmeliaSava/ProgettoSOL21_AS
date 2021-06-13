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
	//const char* nome1 = "./storage/es8.tgz";
	//const char* nome2 = "./storage/manuale-unix.pdf";
	//const char* nome3 = "./storage/it is a mystery.mp3";
	

	if(atoi(argv[1])==0) {
		openFile(nome, 1);
		
		//openFile(nome1, 1);
		//openFile(nome2, 1);
		//openFile(nome3, 1);
	}
	if(atoi(argv[1])==1) {
		writeFile(nome, NULL);
	}
	if(atoi(argv[1])==2) {
		closeFile(nome);
	}
	if(atoi(argv[1])==3) {
		removeFile(nome);
	}
	if(atoi(argv[1])==4) {
		void* buf = NULL;
		size_t sz;
		readFile(nome, &buf, &sz);
		printf("Buf: %p\nSize: %zu/n", buf, sz); // qualcosa non va nel puntatore
	}

	closeConnection(SOCKNAME);

	return 0;
}
